#pragma once
#include <algorithm>

#include "../../ExternLib/atomic_queue/atomic_queue.h"

#include "../../Macro.h"

#include <Log.h>
#include <Memory.h>
#include <ReadStream.h>
#include <WriteStream.h>

#include "MessageChannel/ChannelShared.h"

namespace CoreReliableUDP::MessageChannel {
    /*
         * UnreliableProcessChannel
         * This channel receives messages that do not get acknowledged, meant for messages that
         * only make sense to process if they arrive in a short time, due to their information content
         * being only valid for a short time, if packets arrive to late or get lost it is not worth to process/resend
         * also we do not have any gurantee for ordering, so if packets are out of order the applications have to handle it
         * (eg discard late packets that are overriden by a newer packet already)
         * This class expects the connection_manager_class to have a function
         * bool HandleUnreliableMessage(ChannelMessage& message)
         * Anything in the ChannelMessage will get deallocated after the call ends, so any data that is reused needs to be copied
         */
    template <class connection_manager_class, std::uint32_t number_of_channels, std::uint32_t channel_index, unsigned int max_connection_count, unsigned int send_queue_size = 1024,
        unsigned int max_messages_per_packet = 256, int packet_budget = -1, unsigned int max_message_type_number = std::numeric_limits<std::uint16_t>::max()>
    class UnreliableProcessChannel {
    public:
        using SendQueue = atomic_queue::AtomicQueue2<ChannelMessage, send_queue_size, true, true, true, false>;

        UnreliableProcessChannel(connection_manager_class* connection_manager) {
            m_connection_manager = connection_manager;
            ResetAll();
        }

        /**
                Unreliable process channel destructor.
                Any messages still in the send or receive queues will be released.
             */

        ~UnreliableProcessChannel() {
            ResetAll();
        }

        void ResetAll() {
            SetErrorLevel(CHANNEL_ERROR_NONE);
            for (int connection_index = 0; connection_index < max_connection_count; ++connection_index) {
                for (int i = 0; i < m_message_send_queue[connection_index].was_size(); ++i) {
                    ChannelMessage message = m_message_send_queue[connection_index].pop();
                    if (message.message_data != nullptr) {
                        Global::GMemory->Release(message.message_data);
                    }
                }
            }

            ResetAllCounters();
        }

        void Reset(int connection_index) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);

            SetErrorLevel(connection_index, CHANNEL_ERROR_NONE);

            for (int i = 0; i < m_message_send_queue[connection_index].was_size(); ++i) {
                ChannelMessage message = m_message_send_queue[connection_index].pop();
                if (message.message_data != nullptr) {
                    Global::GMemory->Release(message.message_data);
                }
            }

            ResetCounters(connection_index);
        }

        void SendMessage(int connection_index, ChannelMessage& message) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            if (GetErrorLevel(connection_index) != CHANNEL_ERROR_NONE) {
                Global::GMemory->Release(message.message_data);
                return;
            }

            if (!CanSendMessage(connection_index)) {
                SetErrorLevel(connection_index, CHANNEL_ERROR_SEND_QUEUE_FULL);
                Global::GMemory->Release(message.message_data);
                return;
            }

            m_message_send_queue[connection_index].push(message);

            ++m_counters_array[connection_index * CHANNEL_COUNTER_NUMBER_OF_COUNTERS + CHANNEL_COUNTER_MESSAGES_SENT];
        }

        int GetPacketData(int connection_index, CoreSerialise::WriteStream& stream, int available_bits) {
            constexpr int message_type_bits = BitsRequired<0, max_message_type_number>();
            constexpr int channel_index_bits = BitsRequired<0, number_of_channels>();
            constexpr int number_of_messages_bits = BitsRequired<0, max_messages_per_packet>();
            /// \todo constant when not to try to fit more messages, we need to consider that we have
            constexpr int give_up_bits = message_type_bits + 4 * CHAR_BIT;

            if (m_message_send_queue[connection_index].was_empty()) {
                return 0;
            }

            if constexpr (packet_budget > 0) {
                available_bits = std::min(packet_budget * CHAR_BIT, available_bits);
            }

            if (available_bits < (channel_index_bits + number_of_messages_bits + 1 + 1)) { // block_message 1 bit(false for unreliable) + has_messages 1 bit
                return 0;
            }

            available_bits -= (channel_index_bits + number_of_messages_bits + 1 + 1); // block_message 1 bit(false for unreliable) + has_messages 1 bit

            int used_bits = 0;
            int number_of_messages = 0;

            std::array<ChannelMessage, max_messages_per_packet> messages;

            while (true) {
                if (m_message_send_queue[connection_index].was_empty()) {
                    break;
                }

                if ((available_bits - used_bits) < give_up_bits) {
                    break;
                }

                if (number_of_messages == max_messages_per_packet) {
                    break;
                }

                ChannelMessage message = m_message_send_queue[connection_index].Pop();

                const int message_bits = message_type_bits + m_connection_manager->GetPacketHandler()->GetMessageBitSize(message);
                if (message.block_size) {
                    message_bits += message.block_size * CHAR_BIT;
                }

                if ((used_bits + message_bits) > available_bits) {
                    Global::GMemory->Release(message.m_message_data);
                    continue;
                }

                used_bits += message_bits;

                ASSERT_CRASH(used_bits <= available_bits);

                messages[number_of_messages++] = message;
            }
            if (number_of_messages == 0) {
                return 0;
            }

            stream.SerialiseInteger(channel_index, 0, number_of_channels - 1);
            /// \todo we probably can get rid of this(next)
            stream.SerialiseBits(0, 1); // block_message 1 bit(false for unreliable)
            stream.SerialiseBits(1, 1); // we have messages
            stream.SerialiseInteger(number_of_messages, 1, max_messages_per_packet);
            for (auto& message : messages) {
                stream.SerialiseInteger(message.message_protocol, 0, max_message_type_number);
                m_connection_manager->GetPacketHandler()->SerialiseMessage(message, stream);
                Global::GMemory->Release(message.m_message_data);
            }

            return used_bits + channel_index_bits + number_of_messages_bits + 1 + 1; // block_message 1 bit(false for unreliable) + has_messages 1 bit
        }

        void ProcessPacketData(int connection_index, const Serialise::ReadStream& packet_data, int number_of_messages, std::uint16_t packet_sequence) {
            if (GetErrorLevel(connection_index) != CHANNEL_ERROR_NONE) {
                return;
            }
            ChannelMessage message;

            for (int i = 0; i < number_of_messages; ++i) {
                if (!packet_data.DeserialiseInteger(message.message_protocol, 0, max_message_type_number)) {
                    CORE_DEBUG("[UnreliableProcessChannel] Failed to deserialise message type");
                    SetErrorLevel(connection_index, CHANNEL_ERROR_FAILED_TO_SERIALISE);
                    return;
                }

                message.message_id = packet_sequence;
                if (!m_connection_manager->GetPacketHandler()->DeserialiseMessage(message, packet_data)) {
                    CORE_DEBUG("[UnreliableProcessChannel] Failed to deserialise message type {}", message.message_protocol);
                    SetErrorLevel(connection_index, CHANNEL_ERROR_FAILED_TO_SERIALISE);
                    return;
                }
                if (!m_connection_manager->HandleUnreliableMessage(message)) {
                    CORE_DEBUG("[UnreliableProcessChannel] Failed to handle message type {} ", message.message_protocol);
                }
                Global::GMemory->Release(message.message_data);
                ++m_counters_array[connection_index * CHANNEL_COUNTER_NUMBER_OF_COUNTERS + CHANNEL_COUNTER_MESSAGES_RECEIVED];
            }
        }

        /**
                Get a counter value.
                @param index The index of the counter to retrieve. See ChannelCounters.
                @returns The value of the counter.
                @see ResetCounters
             */
        std::uint64_t GetCounter(int connection_index, ChannelCounters counter) const {
            ASSERT_CRASH(counter >= 0);
            ASSERT_CRASH(counter < CHANNEL_COUNTER_NUMBER_OF_COUNTERS);
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            return m_counters_array[index];
        }

    private:
        void ResetAllCounters() {
            m_counters_array.fill(0);
        }

        void ResetCounters(int connection_index) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            std::fill(&m_counters_array[connection_index * CHANNEL_COUNTER_NUMBER_OF_COUNTERS], &m_counters_array[(connection_index + 1) * CHANNEL_COUNTER_NUMBER_OF_COUNTERS], 0);
        }

        bool CanSendMessage(int connection_index) const {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            return !m_message_send_queue[connection_index].was_full();
        }

        bool HasMessagesToSend(int connection_index) const {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            return !m_message_send_queue[connection_index].was_empty();
        }

        /**
                Set the channel error level.
                All errors go through this function to make debug logging easier.
            */
        void SetErrorLevel(int connection_index, ChannelErrorLevel error_level) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            if (error_level != m_error_level[connection_index] && error_level != CHANNEL_ERROR_NONE) {
                CORE_ERROR("[UnreliableProcessChannel] Channel went into error state: {}", GetChannelErrorString(error_level));
            }
            m_error_level[connection_index] = error_level;
        }

        /**
                Get the channel error level.
                @returns The channel error level.
            */
        ChannelErrorLevel GetErrorLevel(int connection_index) const {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            return m_error_level[connection_index];
        };

        std::array<SendQueue, max_connection_count> m_message_send_queue;                                      // Message send queue.
        std::array<std::uint64_t, CHANNEL_COUNTER_NUMBER_OF_COUNTERS * max_connection_count> m_counters_array; // Counters for unit testing, stats etc.
        connection_manager_class* m_connection_manager;
    };
}
