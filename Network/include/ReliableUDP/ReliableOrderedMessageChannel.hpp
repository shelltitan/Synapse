#pragma once
#include <algorithm>
#include <bitset>
#include <cmath>
#include <new>

#include <Log.h>
#include <Memory.h>
#include <ReadStream.h>
#include <SerialiseBit.h>
#include <WriteStream.h>

#include "ReliableBuffer.h"
#include "MessageChannel/ChannelShared.h"

#ifdef SendMessage
#undef SendMessage
#endif


namespace CoreReliableUDP::MessageChannel {
    /**
            An entry in the send queue of the reliable-ordered channel.
            Messages stay into the send queue until acked. Each message is acked individually, so there can be "holes" in the message send queue.
        */
    struct MessageSendQueueEntry {
        ChannelMessage m_channel_message; // Message is released when the message is acked and removed from the send queue, channel_message.m_message_data gets deallocated on ack.
        std::uint32_t measuredBits : 31;  // The number of bits the message takes up in a bit stream.
        uint32_t block : 1;               // 1 if this is a block message. Block messages are treated differently to regular messages when sent over a reliable-ordered channel.
        std::int64_t time_last_sent;      // The time the message was last sent. Used to implement ChannelConfig::messageResendTime. Expects milliseconds
    };

    /**
            An entry in the receive queue of the reliable-ordered channel.
         */
    struct MessageReceiveQueueEntry {
        ChannelMessage m_channel_message; // Ownership of the message is passed back to the caller when the message is dequeued, deallocate channel_message.m_message_data on caller side.
    };

    /**
            Maps packet level acks to messages and fragments for the reliable-ordered channel.
         */
    struct SentPacketEntry {
        std::uint16_t* message_ids;           // Pointer to an array of message ids. Dynamically allocated because the user can configure the maximum number of messages in a packet per-channel with ChannelConfig::maxMessagesPerPacket.
        std::uint32_t numMessage_ids : 16;    // The number of message ids in in the array.
        std::uint32_t acked : 1;              // 1 if this packet has been acked.
        std::uint64_t block : 1;              // 1 if this packet contains a fragment of a block message.
        std::uint64_t block_message_id : 16;  // The block message id. Valid only if "block" is 1.
        std::uint64_t block_fragment_id : 16; // The block fragment id. Valid only if "block" is 1.
        std::int64_t time_sent;               // The time the packet was sent. Used to estimate round trip time. Expects milliseconds
    };

    /**
            Internal state for a block being sent across the reliable ordered channel.
            Stores the block data and tracks which fragments have been acked. The block send completes when all fragments have been acked.
            IMPORTANT: Although there can be multiple block messages in the message send and receive queues, only one data block can be in flights over the wire at a time.
         */
    template <unsigned int max_number_of_fragments = 4>
    struct SendBlockData {
        SendBlockData() {
            Reset();
        }

        ~SendBlockData() = default;

        void Reset() {
            active = false;
            number_of_fragments = 0;
            number_of_acked_fragments = 0;
            block_message_id = 0;
            block_size = 0;
            acked_fragment.reset();
            fragment_send_time.fill(0);
        }

        bool active;                                                           // True if we are currently sending a block.
        int block_size;                                                        // The size of the block (bytes).
        int number_of_fragments;                                               // Number of fragments in the block being sent.
        int number_of_acked_fragments;                                         // Number of acked fragments in the block being sent.
        std::uint16_t block_message_id;                                        // The message id the block is attached to.
        std::bitset<max_number_of_fragments> acked_fragment;                   // Has fragment n been received?
        std::array<std::uint64_t, max_number_of_fragments> fragment_send_time; // Last time fragment was sent.

    private:
        SendBlockData(const SendBlockData& other);

        SendBlockData& operator=(const SendBlockData& other);
    };

    /**
            Internal state for a block being received across the reliable ordered channel.
            Stores the fragments received over the network for the block, and completes once all fragments have been received.
            IMPORTANT: Although there can be multiple block messages in the message send and receive queues, only one data block can be in flight over the wire at a time.
         */
    template <unsigned int max_number_of_fragments = 4, unsigned int max_fragment_size = 1024>
    struct ReceiveBlockData {
        ReceiveBlockData() {
            block_data.fill(std::byte(0x00));
            message.message_data = nullptr;
            Reset();
        }

        ~ReceiveBlockData() {
        }

        void Reset() {
            active = false;
            number_of_fragments = 0;
            number_of_received_fragments = 0;
            message_id = 0;
            block_size = 0;
            received_fragment.reset();
        }

        bool active;                                                                   // True if we are currently receiving a block.
        int number_of_fragments;                                                       // The number of fragments in this block
        int number_of_received_fragments;                                              // The number of fragments received.
        std::uint16_t message_id;                                                      // The message id corresponding to the block.
        std::uint32_t block_size;                                                      // Block size in bytes.
        std::bitset<max_number_of_fragments> received_fragment;                        // Has fragment n been received?
        std::array<std::byte, max_number_of_fragments * max_fragment_size> block_data; // Block data for receive.
        ChannelMessage message;                                                        // Message (sent with fragment 0).

    private:
        ReceiveBlockData(const ReceiveBlockData& other);

        ReceiveBlockData& operator=(const ReceiveBlockData& other);
    };


    /// \todo add locks
    template <class connection_manager_class, unsigned int max_connection_count, std::int64_t message_resend_time_milliseconds = 100, unsigned int message_send_queue_size = 512,
        unsigned int message_receive_queue_size = 512, unsigned int message_sent_queue_size = 512, unsigned int max_messages_per_packet = 256, int packet_budget = -1,
        unsigned int max_message_type_number = std::numeric_limits<std::uint16_t>::max(), unsigned int max_number_of_fragments = 4, unsigned int max_fragment_size = 1024,
        std::int64_t fragment_resend_time_milliseconds = 250>
    class ReliableOrderedChannel {
    public:
        ReliableOrderedChannel(connection_manager_class* connection_manager) {
            static_assert((65536 % message_sent_queue_size) == 0);
            static_assert((65536 % message_send_queue_size) == 0);
            static_assert((65536 % message_receive_queue_size) == 0);
            m_connection_manager = connection_manager;
            ResetAll();
        }

        /**
                Reliable ordered channel destructor.
                Any messages still in the send or receive queues will be released.
             */
        ~ReliableOrderedChannel() {
            ResetAll();
        }

        void ResetAll() {
            m_error_level_array.fill(CHANNEL_ERROR_NONE);

            m_send_message_ids.fill(0);
            m_receive_message_ids.fill(0);
            m_oldest_unacked_message_ids.fill(0);

            m_sent_packets.ResetAll();

            for (int connection_index = 0; connection_index < max_connection_count; ++connection_index) {
                for (int index = 0; index < m_message_send_queues.GetSize(); ++index) {
                    MessageSendQueueEntry* entry = m_message_send_queues.GetAtIndex(connection_index, index);
                    if (entry && entry->m_channel_message.message_data) {
                        Global::GMemory->Release(entry->m_channel_message.message_data);
                        entry->m_channel_message.message_data = nullptr;
                    }
                }
            }
            m_message_send_queues.ResetAll();

            for (int connection_index = 0; connection_index < max_connection_count; ++connection_index) {
                for (int index = 0; index < m_message_receive_queues.GetSize(); ++index) {
                    MessageReceiveQueueEntry* entry = m_message_receive_queues.GetAtIndex(connection_index, index);
                    if (entry && entry->m_channel_message.message_data) {
                        Global::GMemory->Release(entry->m_channel_message.message_data);
                        entry->m_channel_message.message_data = nullptr;
                    }
                }
            }
            m_message_receive_queues.ResetAll();

            for (auto& send_block : m_send_blocks) {
                send_block.Reset();
            }

            for (auto& receive_block : m_receive_blocks) {
                if (receive_block.message.message_data) {
                    Global::GMemory->Release(receive_block.message.message_data);
                    receive_block.message.message_data = nullptr;
                }
                receive_block.Reset();
            }

            m_counters_array.fill(0);
        }

        void Reset(int connection_index) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);

            SetErrorLevel(connection_index, CHANNEL_ERROR_NONE);
            m_send_message_ids[connection_index] = 0;
            m_receive_message_ids[connection_index] = 0;
            m_oldest_unacked_message_ids[connection_index] = 0;

            m_sent_packets.Reset(connection_index);

            for (int index = 0; index < m_message_send_queues.GetSize(); ++index) {
                MessageSendQueueEntry* entry = m_message_send_queues.GetAtIndex(connection_index, index);
                if (entry && entry->m_channel_message.message_data) {
                    Global::GMemory->Release(entry->m_channel_message.message_data);
                    entry->m_channel_message.message_data = nullptr;
                }
            }
            m_message_send_queues.Reset(connection_index);

            for (int index = 0; index < m_message_receive_queues.GetSize(); ++index) {
                MessageReceiveQueueEntry* entry = m_message_receive_queues.GetAtIndex(connection_index, index);
                if (entry && entry->m_channel_message.message_data) {
                    Global::GMemory->Release(entry->m_channel_message.message_data);
                    entry->m_channel_message.message_data = nullptr;
                }
            }
            m_message_receive_queues.Reset(connection_index);

            m_send_blocks[connection_index].Reset();
            if (m_receive_blocks[connection_index].message.message_data) {
                Global::GMemory->Release(m_receive_blocks[connection_index].message.message_data);
                m_receive_blocks[connection_index].message.message_data = nullptr;
            }
            m_receive_blocks[connection_index].Reset();

            std::fill(&m_counters_array[connection_index * CHANNEL_COUNTER_NUMBER_OF_COUNTERS], &m_counters_array[(connection_index + 1) * CHANNEL_COUNTER_NUMBER_OF_COUNTERS], 0);
        }

        void SendMessage(int connection_index, ChannelMessage& message) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            if (GetErrorLevel(connection_index) != CHANNEL_ERROR_NONE) {
                Global::GMemory->Release(message.message_data);
                return;
            }

            if (!CanSendMessage(connection_index)) {
                // Increase your send queue size!
                SetErrorLevel(connection_index, CHANNEL_ERROR_SEND_QUEUE_FULL);
                Global::GMemory->Release(message.message_data);
                return;
            }

            message->m_packet_sequence = m_send_message_ids[connection_index];

            MessageSendQueueEntry* entry = m_message_send_queues.Insert(m_send_message_ids[connection_index], connection_index);

            ASSERT_CRASH(entry);

            entry->block = message.is_block;
            entry->m_channel_message = message;
            entry->measuredBits = 0;
            entry->time_last_sent = -1.0;

            if (entry->block) {
                ASSERT_CRASH(message.block_size > 0);
                ASSERT_CRASH(message.block_size <= max_number_of_fragments * max_fragment_size);
            }

            entry->measuredBits = m_connection_manager->GetPacketHandler()->GetMessageSizeInBits(message.message_protocol);
            ++m_counters_array[connection_index * CHANNEL_COUNTER_NUMBER_OF_COUNTERS + CHANNEL_COUNTER_MESSAGES_SENT];
            ++m_send_message_ids[connection_index];
        }

        // IMPORTANT: deallocate channel_message.m_message_data on the receive side of this
        bool ReceiveMessage(int connection_index, ChannelMessage& message) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            if (GetErrorLevel(connection_index) != CHANNEL_ERROR_NONE) {
                return false;
            }

            MessageReceiveQueueEntry* entry = m_message_receive_queues->Find(connection_index, m_receive_message_ids[connection_index]);
            if (!entry) {
                return false;
            }

            message = entry->m_channel_message;
            ASSERT_CRASH(message.message_id == m_receive_message_ids[connection_index]);
            m_message_receive_queues->Remove(connection_index, m_receive_message_ids[connection_index]);
            ++m_counters_array[connection_index * CHANNEL_COUNTER_NUMBER_OF_COUNTERS + CHANNEL_COUNTER_MESSAGES_RECEIVED];
            ++m_receive_message_ids[connection_index];
            return true;
        }

        template <int number_of_channels, int channel_index>
        int GetPacketData(int connection_index, CoreSerialise::WriteStream& stream, std::uint16_t packet_sequence, std::uint32_t available_bits) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            constexpr int channel_index_bits = Serialise::BitsRequired<0, number_of_channels>();
            constexpr int number_of_messages_bits = Serialise::BitsRequired<0, max_messages_per_packet>();

            if (!HasMessagesToSend(connection_index)) {
                return 0;
            }

            if (available_bits > channel_index_bits) {
                return 0;
            }

            available_bits -= channel_index_bits;

            if (SendingBlockMessage(connection_index)) {
                if ((max_fragment_size * CHAR_BIT + 1) > available_bits) { // block_message 1 bit(true)
                    return 0;
                }
                available_bits -= (max_fragment_size * CHAR_BIT + 1); // block_message 1 bit(true)

                uint16_t messageId;
                uint16_t fragmentId;
                int fragmentBytes;
                int numFragments;
                int messageType;

                std::byte* fragmentData = GetFragmentToSend(connection_index, messageId, fragmentId, fragmentBytes, numFragments, messageType, available_bits);

                if (fragmentData) {
                    const int fragmentBits = GetFragmentPacketData<number_of_channels, channel_index>(connection_index, stream, messageId, fragmentId, fragmentData, fragmentBytes, numFragments, messageType);
                    AddFragmentPacketEntry(connection_index, messageId, fragmentId, packet_sequence);
                    Global::GMemory->Release(fragmentData);
                    return fragmentBits;
                }
                return 0;
            }
            else {
                if (available_bits < (number_of_messages_bits + 1 + 1)) { // block_message 1 bit(false) + has_messages 1 bit
                    return 0;
                }

                available_bits -= (channel_index_bits + number_of_messages_bits + 1 + 1); // block_message 1 bit(false) + has_messages 1 bit
                int number_of_message_ids = 0;
                std::array<std::uint16_t, max_messages_per_packet> message_ids;
                const int messageBits = GetMessagesToSend(connection_index, message_ids, number_of_message_ids, available_bits);

                if (number_of_message_ids > 0) {
                    GetMessagePacketData<number_of_channels, channel_index>(connection_index, stream, message_ids, number_of_message_ids);
                    AddMessagePacketEntry(connection_index, message_ids, number_of_message_ids, packet_sequence);
                    return messageBits;
                }

                return 0;
            }
        }

        /**
                Process messages included in a packet.
                Any messages that have not already been received are added to the message receive queue. Messages that are added to the receive queue have a reference added. See Message::AddRef.
                @param numMessages The number of messages to process.
                @param messages Array of pointers to messages.
            */
        void ProcessPacketData(int connection_index, CoreSerialise::ReadStream& packet_data, std::uint16_t packet_sequence) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            if (m_error_level_array[connection_index] != CHANNEL_ERROR_NONE) {
                return;
            }

            const uint16_t min_message_id = m_receive_message_ids[connection_index];
            const uint16_t max_message_id = m_receive_message_ids[connection_index] + message_receive_queue_size - 1;

            int number_of_messages;
            if (!packet_data.DeserialiseInteger(number_of_messages, 0, max_messages_per_packet)) {
                CORE_DEBUG("[ReliableOrderedChannel] Failed to deserialise the number of messages for the channel");
                SetErrorLevel(connection_index, CHANNEL_ERROR_FAILED_TO_SERIALISE);
                return;
            }

            ChannelMessage message;
            std::uint16_t* message_ids = std::bit_cast<std::uint16_t*>(Global::GMemory->Allocate(number_of_messages * sizeof(std::uint16_t)));
            message.message_id = packet_sequence;
            if (!packet_data.DeserialiseBits(message_ids[0], 16)) {
                CORE_DEBUG("[ReliableOrderedChannel] Failed to deserialise message id");
                SetErrorLevel(connection_index, CHANNEL_ERROR_FAILED_TO_SERIALISE);
                Global::GMemory->Release(message_ids);
                return;
            }

            for (int i = 1; i < number_of_messages; ++i) {
                if (!packet_data.DeserialiseSequenceRelative(message_ids[i - 1], message_ids[i])) {
                    CORE_DEBUG("[ReliableOrderedChannel] Failed to deserialise relative sequence at index {}", i);
                    SetErrorLevel(connection_index, CHANNEL_ERROR_FAILED_TO_SERIALISE);
                    Global::GMemory->Release(message_ids);
                    return;
                }
            }

            for (int i = 0; i < number_of_messages; ++i) {
                if (!packet_data.DeserialiseInteger(message.message_protocol, 0, max_message_type_number)) {
                    CORE_DEBUG("[ReliableOrderedChannel] Failed to deserialise message type");
                    SetErrorLevel(connection_index, CHANNEL_ERROR_FAILED_TO_SERIALISE);
                    Global::GMemory->Release(message_ids);
                    return;
                }

                const uint16_t message_id = message_ids[i];

                if (SequenceLessThan(message_id, min_message_id)) {
                    continue;
                }

                if (SequenceGreaterThan(message_id, max_message_id)) {
                    // Did you forget to dequeue messages on the receiver?
                    CORE_DEBUG("[ReliableOrderedChannel] Sequence overflow: {} vs. [{},{}]", message_id, min_message_id, max_message_id);
                    SetErrorLevel(connection_index, CHANNEL_ERROR_DESYNC);
                    Global::GMemory->Release(message_ids);
                    return;
                }

                if (m_message_receive_queues.Find(connection_index, message_id)) {
                    continue;
                }

                ASSERT_CRASH(!m_message_receive_queues.GetAtIndex(connection_index, m_message_receive_queues.GetIndex(message_id)));

                MessageReceiveQueueEntry* entry = m_message_receive_queues.Insert(connection_index, message_id);
                if (!entry) {
                    // For some reason we can't insert the message in the receive queue
                    SetErrorLevel(connection_index, CHANNEL_ERROR_DESYNC);
                    Global::GMemory->Release(message_ids);
                    return;
                }

                if (!m_connection_manager->GetPacketHandler()->DeserialiseMessage(entry->m_channel_message, packet_data)) {
                    CORE_DEBUG("[ReliableOrderedChannel] Failed to deserialise message type {}", message.message_protocol);
                    SetErrorLevel(connection_index, CHANNEL_ERROR_FAILED_TO_SERIALISE);
                    Global::GMemory->Release(message_ids);
                    return;
                }
            }
            Global::GMemory->Release(message_ids);
        }

        void ProcessAcknowledgement(int connection_index, std::uint16_t sequence) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            SentPacketEntry* sent_packet_entry = m_sent_packets.Find(connection_index, sequence);
            if (!sent_packet_entry) {
                return;
            }

            ASSERT_CRASH(!sent_packet_entry->acked);

            for (int i = 0; i < (int)sent_packet_entry->numMessageIds; ++i) {
                const uint16_t message_id = sent_packet_entry->messageIds[i];
                MessageSendQueueEntry* sendQueueEntry = m_message_send_queues.Find(connection_index, message_id);
                if (sendQueueEntry) {
                    ASSERT_CRASH(sendQueueEntry->m_channel_message.message_data);
                    ASSERT_CRASH(sendQueueEntry->m_channel_message.message_id == message_id);
                    Global::GMemory->Release(sendQueueEntry->m_channel_message.message_data);
                    m_message_send_queues.Remove(connection_index, message_id);
                    UpdateOldestUnackedMessageID(connection_index);
                }
            }

            SendBlockData<max_number_of_fragments>& send_block = m_send_blocks[connection_index];
            if (sentPacketEntry->block && send_block.active && send_block.block_message_id == sentPacketEntry->block_message_id) {
                const int messageId = sentPacketEntry->block_message_id;
                const int fragmentId = sentPacketEntry->block_fragment_id;

                if (!send_block.acked_fragment[fragmentId]) {
                    send_block.acked_fragment[fragmentId] = true;
                    ++(send_block.number_of_acked_fragments);
                    if (send_block.number_of_acked_fragments == send_block.number_of_fragments) {
                        send_block.active = false;
                        MessageSendQueueEntry* sendQueueEntry = m_message_send_queues.Find(connection_index, messageId);
                        ASSERT_CRASH(sendQueueEntry);
                        Global::GMemory->Release(sendQueueEntry->m_channel_message.message_data);
                        m_message_send_queues.Remove(connection_index, messageId);
                        UpdateOldestUnackedMessageID(connection_index);
                    }
                }
            }
        }

        /**
                Fill the packet data with block and fragment data.
                This is the payload function that fills the channel packet data while we are sending a block message.
                @param packetData The packet data to fill [out]
                @param messageId The id of the message that the block is attached to.
                @param fragmentId The id of the block fragment being sent.
                @param fragmentData The fragment data.
                @param fragmentSize The size of the fragment data (bytes).
                @param numFragments The number of fragments in the block.
                @param messageType The type of message the block is attached to.
                @returns An estimate of the number of bits required to serialize the block message and fragment data (upper bound).
             */
        template <int number_of_channels, int channel_index>
        int GetFragmentPacketData(int connection_index, CoreSerialise::WriteStream& stream, std::uint16_t message_id, std::uint16_t fragment_id, std::byte* fragment_data, int fragment_size, int number_of_fragments, int message_type) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            constexpr int message_type_bits = Serialise::BitsRequired<0, max_message_type_number>();

            stream.SerialiseInteger(channel_index, 0, number_of_channels - 1);
            stream.SerialiseBits(1, 1); // block_message 1 bit(true)
            stream.SerialiseBits(message_id, 16);
            if constexpr (max_number_of_fragments > 1) {
                stream.SerialiseInteger(number_of_fragments, 1, max_number_of_fragments);
            }
            if (number_of_fragments > 1) {
                stream.SerialiseInteger(fragment_id, 0, number_of_fragments - 1);
            }

            stream.SerialiseInteger(fragment_size, 1, max_fragment_size);

            stream.SerialiseBytes(fragment_data, fragment_size);

            int fragmentBits = message_type_bits + fragment_size * CHAR_BIT;

            if (fragment_id == 0) {
                // block message
                stream.SerialiseInteger(message_type, 1, max_message_type_number);

                MessageSendQueueEntry* entry = m_message_send_queues.Find(connection_index, message_id);

                ASSERT_CRASH(entry);

                stream.SerialiseInteger(entry->m_channel_message.message_protocol, 0, max_message_type_number);
                m_connection_manager->GetPacketHandler()->SerialiseMessage(entry->m_channel_message, stream);
                fragmentBits += entry->measuredBits + message_type_bits;
            }

            return fragmentBits;
        }

        /**
                Process a packet fragment.
                The fragment is added to the set of received fragments for the block. When all packet fragments are received, that block is reconstructed, attached to the block message and added to the message receive queue.
                @param messageType The type of the message this block fragment is attached to. This is used to make sure this message type actually allows blocks to be attached to it.
                @param messageId The id of the message the block fragment belongs to.
                @param numFragments The number of fragments in the block.
                @param fragmentId The id of the fragment in [0,numFragments-1].
                @param fragmentData The fragment data.
                @param fragmentBytes The size of the fragment data in bytes.
             */
        void ProcessPacketFragment(int connection_index, CoreSerialise::ReadStream& packet_data, std::uint16_t packet_sequence) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            void(packet_sequence);
            ChannelMessage message;
            std::uint16_t message_id;
            int number_of_fragments;
            int fragmentId;
            std::byte* fragmentData;
            int fragmentBytes;

            if (!packet_data.DeserialiseBits(message_id, 16)) {
                CORE_DEBUG("[ReliableOrderedChannel] Failed to deserialise message id");
                SetErrorLevel(connection_index, CHANNEL_ERROR_FAILED_TO_SERIALISE);
                return;
            }
            if constexpr (max_number_of_fragments > 1) {
                if (!packet_data.DeserialiseInteger(number_of_fragments, 1, max_number_of_fragments)) {
                    CORE_DEBUG("[ReliableOrderedChannel] Failed to deserialise number of fragments");
                    SetErrorLevel(connection_index, CHANNEL_ERROR_FAILED_TO_SERIALISE);
                    return;
                }
            }
            else {
                number_of_fragments = 1;
            }

            if (number_of_fragments > 1) {
                if (!packet_data.DeserialiseInteger(fragmentId, 1, number_of_fragments - 1)) {
                    CORE_DEBUG("[ReliableOrderedChannel] Failed to deserialise fragment id");
                    SetErrorLevel(connection_index, CHANNEL_ERROR_FAILED_TO_SERIALISE);
                    return;
                }
            }
            else {
                fragmentId = 0;
            }

            if (!packet_data.DeserialiseInteger(fragmentBytes, 1, max_fragment_size)) {
                CORE_DEBUG("[ReliableOrderedChannel] Failed to deserialise fragment size");
                SetErrorLevel(connection_index, CHANNEL_ERROR_FAILED_TO_SERIALISE);
                return;
            }

            fragmentData = Global::GMemory->Allocate(fragmentBytes);

            if (!fragmentData) {
                CORE_DEBUG("[ReliableOrderedChannel] Failed to deserialise block fragment");
                SetErrorLevel(connection_index, CHANNEL_ERROR_FAILED_TO_SERIALISE);
                return false;
            }
            if (!packet_data.DeserialiseBytes(fragmentData, fragmentBytes)) {
                CORE_DEBUG("[ReliableOrderedChannel] Failed to deserialise block fragment");
                Global::GMemory->Release(fragmentData);
                SetErrorLevel(connection_index, CHANNEL_ERROR_FAILED_TO_SERIALISE);
                return;
            }
            if (fragmentId == 0) {
                if (!packet_data.DeserialiseInteger(message.message_protocol, 0, max_message_type_number)) {
                    CORE_DEBUG("[ReliableOrderedChannel] Failed to deserialise fragment size");
                    SetErrorLevel(connection_index, CHANNEL_ERROR_FAILED_TO_SERIALISE);
                    Global::GMemory->Release(fragmentData);
                    return;
                }
                message.message_id = message_id;
                // IMPORTANT: Set the offset inside the packet handler in the message
                if (!m_connection_manager->m_packet_handler.Deserialise(message, packet_data)) {
                    CORE_DEBUG("[ReliableOrderedChannel] Failed to deserialise message type {}", message.message_protocol);
                    SetErrorLevel(connection_index, CHANNEL_ERROR_FAILED_TO_SERIALISE);
                    Global::GMemory->Release(fragmentData);
                    return;
                }
            }

            if (fragmentData) {
                const uint16_t expected_message_id = m_message_receive_queues->GetSequence(connection_index);
                if (message_id != expected_message_id) {
                    Global::GMemory->Release(fragmentData);
                    if (fragmentId == 0) {
                        Global::GMemory->Release(message.message_data);
                    }
                    return;
                }

                // start receiving a new block

                auto& receive_block = m_receive_blocks[connection_index];

                if (!receive_block.active) {
                    ASSERT_CRASH(number_of_fragments >= 0);
                    ASSERT_CRASH(number_of_fragments <= max_number_of_fragments);

                    receive_block.active = true;
                    receive_block.number_of_fragments = number_of_fragments;
                    receive_block.numReceivedFragments = 0;
                    receive_block.message_id = message_id;
                    receive_block.blockSize = 0;
                    receive_block.receivedFragment->Clear();
                }

                // validate fragment

                if (fragmentId >= receive_block.number_of_fragments) {
                    // The fragment id is out of range.
                    SetErrorLevel(connection_index, CHANNEL_ERROR_DESYNC);
                    Global::GMemory->Release(fragmentData);
                    if (fragmentId == 0) {
                        Global::GMemory->Release(message.message_data);
                    }
                    return;
                }

                if (number_of_fragments != receive_block.number_of_fragments) {
                    // The number of fragments is out of range.
                    SetErrorLevel(connection_index, CHANNEL_ERROR_DESYNC);
                    Global::GMemory->Release(fragmentData);
                    if (fragmentId == 0) {
                        Global::GMemory->Release(message.message_data);
                    }
                    return;
                }

                // receive the fragment

                if (!receive_block.receivedFragment[fragmentId]) {
                    receive_block.receivedFragment.set(fragmentId);

                    (void)std::copy_n(fragmentData, fragmentBytes, receive_block.block_data.data() + fragmentId * max_fragment_size);
                    Global::GMemory->Release(fragmentData);

                    if (fragmentId == 0) {
                        // save block message (sent with fragment 0)
                        receive_block.message = message;
                    }

                    if (fragmentId == receive_block.number_of_fragments - 1) {
                        receive_block.blockSize = (receive_block.number_of_fragments - 1) * max_fragment_size + fragmentBytes;

                        if (receive_block.blockSize > (uint32_t)max_fragment_size * max_number_of_fragments) {
                            // The block size is outside range
                            SetErrorLevel(connection_index, CHANNEL_ERROR_DESYNC);
                            return;
                        }
                    }

                    ++receive_block.numReceivedFragments;

                    if (receive_block.numReceivedFragments == receive_block.number_of_fragments) {
                        // finished receiving block

                        if (m_message_receive_queues->GetAtIndex(connection_index, m_message_receive_queues->GetIndex(connection_index, message_id))) {
                            // Did you forget to dequeue messages on the receiver?
                            SetErrorLevel(connection_index, CHANNEL_ERROR_DESYNC);
                            return;
                        }

                        std::byte* block_message_data = Global::GMemory->Allocate(receive_block.message.block_offset + receive_block.block_size);

                        if (!block_message_data) {
                            // Not enough memory to allocate block data
                            SetErrorLevel(connection_index, CHANNEL_ERROR_OUT_OF_MEMORY);
                            return;
                        }

                        // first we copy the actual message, then the block part
                        (void)std::copy_n(receive_block.message.message_data, receive_block.message.block_offset, block_message_data);
                        Global::GMemory->Release(receive_block.message.message_data);
                        (void)std::copy_n(receive_block.block_data.data(), receive_block.block_size, block_message_data + receive_block.message.block_offset);

                        receive_block.message.block_size = receive_block.block_size;
                        receive_block.message.message_data = block_message_data;
                        receive_block.message.message_id = message_id;

                        MessageReceiveQueueEntry* entry = m_message_receive_queues->Insert(connection_index, message_id);
                        ASSERT_CRASH(entry);
                        entry->message = receive_block.message;
                        receive_block.active = false;
                        receive_block.block_data.fill(std::byte(0x00));
                        receive_block.message.Reset();
                    }
                }
                else {
                    Global::GMemory->Release(fragmentData);
                }
            }
        }

        /**
                Get a counter value.
                @param index The index of the counter to retrieve. See ChannelCounters.
                @returns The value of the counter.
             */
        std::uint64_t GetCounter(int connection_index, ChannelCounters counter) const {
            ASSERT_CRASH(index >= 0);
            ASSERT_CRASH(index < CHANNEL_COUNTER_NUMBER_OF_COUNTERS);
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            return m_counters_array[connection_index * CHANNEL_COUNTER_NUMBER_OF_COUNTERS + counter];
        }

    private:
        /**
                Are there any unacked messages in the send queue?
                Messages are acked individually and remain in the send queue until acked.
                @returns True if there is at least one unacked message in the send queue.
             */
        bool HasMessagesToSend(int connection_index) const {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            return m_oldest_unacked_message_ids[connection_index] != m_send_message_ids[connection_index];
        }

        bool CanSendMessage(int connection_index) const {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            return m_message_send_queues.Available(connection_index, m_send_message_ids[connection_index]);
        }

        /**
                True if we are currently sending a block message.
                Block messages are treated differently to regular messages.
                Regular messages are small so we try to fit as many into the packet we can. See ReliableChannelData::GetMessagesToSend.
                Blocks attached to block messages are usually larger than the maximum packet size or channel budget, so they are split up fragments.
                While in the mode of sending a block message, each channel packet data generated has exactly one fragment from the current block in it. Fragments keep getting included in packets until all fragments of that block are acked.
                @returns True if currently sending a block message over the network, false otherwise.
                @see BlockMessage
                @see GetFragmentToSend
            */
        bool SendingBlockMessage(int connection_index) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            ASSERT_CRASH(HasMessagesToSend(connection_index));

            MessageSendQueueEntry* entry = m_message_send_queues.Find(connection_index, m_oldest_unacked_message_ids[connection_index]);

            return entry ? entry->block : false;
        }

        /**
                Get messages to include in a packet.
                Messages are measured to see how many bits they take, and only messages that fit within the channel packet budget will be included. See ChannelConfig::packetBudget.
                Takes care not to send messages too rapidly by respecting ChannelConfig::messageResendTime for each message, and to only include messages that that the receiver is able to buffer in their receive queue. In other words, don't run ahead of the receiver.
                @param messageIds Array of message ids to be filled [out]. Fills up to ChannelConfig::maxMessagesPerPacket messages, make sure your array is at least this size.
                @param numMessageIds The number of message ids written to the array.
                @param available_bits Number of bits remaining in the packet. Considers this as a hard limit when determining how many messages can fit into the packet.
                @returns Estimate of the number of bits required to serialize the messages (upper bound).
                @see GetMessagePacketData
            */
        int GetMessagesToSend(int connection_index, std::array<uint16_t, max_messages_per_packet>& messageIds, int& numMessageIds, int available_bits) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            ASSERT_CRASH(HasMessagesToSend(connection_index));
            constexpr int message_limit = std::min(message_send_queue_size, message_receive_queue_size);
            constexpr int message_type_bits = Serialise::BitsRequired<0, max_message_type_number>();
            /// \todo need to calculate if we have enough space for the channel header
            constexpr int giveUpBits = message_type_bits + 4 * CHAR_BIT;

            numMessageIds = 0;

            if constexpr (packet_budget > 0) {
                available_bits = std::min(packet_budget * CHAR_BIT, available_bits);
            }

            uint16_t previousMessageId = 0;
            int usedBits = 0;
            int giveUpCounter = 0;

            for (int i = 0; i < message_limit; ++i) {
                if (available_bits - usedBits < giveUpBits) {
                    break;
                }

                if (giveUpCounter > message_send_queue_size) {
                    break;
                }

                uint16_t messageId = m_oldest_unacked_message_ids[connection_index] + i;
                MessageSendQueueEntry* entry = m_message_send_queues.Find(connection_index, messageId);
                if (!entry) {
                    continue;
                }

                if (entry->block) {
                    break;
                }

                if (entry->time_last_sent + message_resend_time <= m_connection_manager->GetTime() && available_bits >= (int)entry->measuredBits) {
                    int messageBits = entry->measuredBits + message_type_bits;

                    if (numMessageIds == 0) {
                        messageBits += 16;
                    }
                    else {
                        messageBits += Serialise::BitRequiredToSerializeSequenceRelative(previousMessageId, messageId);
                    }

                    if (usedBits + messageBits > available_bits) {
                        ++giveUpCounter;
                        continue;
                    }

                    usedBits += messageBits;
                    messageIds[numMessageIds++] = messageId;
                    previousMessageId = messageId;
                    entry->time_last_sent = m_connection_manager->GetTime();
                }

                if (numMessageIds == max_messages_per_packet) {
                    break;
                }
            }

            return usedBits;
        }

        /**
                Get the next block fragment to send.
                The next block fragment is selected by scanning left to right over the set of fragments in the block, skipping over any fragments that have already been acked or have been sent within ChannelConfig::fragmentResendTime.
                @param messageId The id of the message that the block is attached to [out].
                @param fragmentId The id of the fragment to send [out].
                @param fragmentBytes The size of the fragment in bytes.
                @param numFragments The total number of fragments in this block.
                @param messageType The type of message the block is attached to. See MessageFactory.
                @returns Pointer to the fragment data.
            */
        std::byte* GetFragmentToSend(int connection_index, std::uint16_t& messageId, std::uint16_t& fragmentId, int& fragmentBytes, int& numFragments, int& messageType, std::uint32_t available_bits) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            constexpr std::uint32_t message_type_bits = Serialise::BitsRequired<0, max_message_type_number>();
            MessageSendQueueEntry* entry = m_message_send_queues.Find(connection_index, m_oldest_unacked_message_ids[connection_index]);

            ASSERT_CRASH(entry);
            ASSERT_CRASH(entry->block);

            ChannelMessage& block_message = entry->m_channel_message;

            messageId = block_message.message_id;

            const int blockSize = block_message.block_size;

            SendBlockData<max_number_of_fragments>& send_block = m_send_blocks[connection_index];

            if (!send_block.active) {
                // start sending this block

                send_block.active = true;
                send_block.block_size = blockSize;
                send_block.block_message_id = messageId;
                send_block.number_of_fragments = (int)std::ceil(blockSize / float(max_fragment_size));
                send_block.number_of_acked_fragments = 0;

                ASSERT_CRASH(send_block.number_of_fragments > 0);
                ASSERT_CRASH(send_block.number_of_fragments <= max_number_of_fragments);

                send_block.acked_fragment.reset();

                for (int i = 0; i < max_number_of_fragments; ++i) {
                    send_block.fragment_send_time[i] = 0;
                }
            }

            numFragments = send_block.number_of_fragments;

            // find the next fragment to send (there may not be one)

            fragmentId = std::numeric_limits<uint16_t>::max();

            for (int i = 0; i < send_block.number_of_fragments; ++i) {
                if (!send_block.acked_fragment[i] && send_block.fragment_send_time[i] + fragment_resend_time < m_connection_manager->GetTime()) {
                    fragmentId = uint16_t(i);
                    break;
                }
            }

            if (fragmentId == std::numeric_limits<uint16_t>::max()) {
                return nullptr;
            }
            if (fragmentId == 0 && (available_bits < entry->measuredBits + message_type_bits)) {
                return nullptr;
            }

            // allocate and return a copy of the fragment data

            fragmentBytes = max_fragment_size;

            const int fragmentRemainder = blockSize % max_fragment_size;

            if (fragmentRemainder && fragmentId == send_block.number_of_fragments - 1) {
                fragmentBytes = fragmentRemainder;
            }

            std::byte* fragmentData = std::bit_cast<std::byte*>(Global::GMemory->Allocate(fragmentBytes));

            ASSERT_CRASH(block_message.message_data);

            if (fragmentData) {
                (void)std::copy_n(block_message.message_data + block_message.block_offset + fragmentId * max_fragment_size, block_message.block_offset + fragmentId * max_fragment_size + fragmentBytes, fragmentData);

                send_block.fragment_send_time[fragmentId] = m_connection_manager->GetTime();
            }

            return fragmentData;
        }

        /**
                Fill channel packet data with messages.
                This is the payload function to fill packet data while sending regular messages (without blocks attached).
                Messages have references added to them when they are added to the packet. They also have a reference while they are stored in a send or receive queue. Messages are cleaned up when they are no longer in a queue, and no longer referenced by any packets.
                @param packetData The packet data to fill [out]
                @param messageIds Array of message ids identifying which messages to add to the packet from the message send queue.
                @param numMessageIds The number of message ids in the array.
                @see GetMessagesToSend
             */
        template <int number_of_channels, int channel_index>
        void GetMessagePacketData(int connection_index, CoreSerialise::WriteStream& stream, const std::array<uint16_t, max_messages_per_packet>& message_ids, int number_of_message_ids) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            ASSERT_CRASH(number_of_message_ids);

            stream.SerialiseInteger(channel_index, 0, number_of_channels - 1);
            stream.SerialiseBits(0, 1); // block_message 1 bit(false)
            stream.SerialiseBits(1, 1); // we have messages
            stream.SerialiseInteger(number_of_message_ids, 1, max_messages_per_packet);

            for (std::uint16_t message_id : message_ids) {
                MessageSendQueueEntry* entry = m_message_send_queues.Find(connection_index, message_id);
                ASSERT_CRASH(entry);
                ASSERT_CRASH(entry->m_channel_message.message_data);
                m_connection_manager->GetPacketHandler()->SerialiseMessage(entry->m_channel_message, stream);
            }
        }

        /**
                Adds a packet entry for the fragment.
                This lets us look up the fragment that was in the packet later on when it is acked, so we can ack that block fragment.
                @param messageId The message id that the block was attached to.
                @param fragmentId The fragment id.
                @param sequence The sequence number of the packet the fragment was included in.
            */
        void AddFragmentPacketEntry(int connection_index, std::uint16_t messageId, std::uint16_t fragmentId, std::uint16_t sequence) {
            SentPacketEntry* sent_packet = m_sent_packets.Insert(sequence, true);
            ASSERT_CRASH(sentPacket);
            if (sent_packet) {
                sent_packet->numMessageIds = 0;
                sent_packet->messageIds = nullptr;
                sent_packet->time_sent = m_connection_manager->GetTime();
                sent_packet->acked = 0;
                sent_packet->block = 1;
                sent_packet->block_message_id = messageId;
                sent_packet->block_fragment_id = fragmentId;
            }
        }

        /**
                Add a packet entry for the set of messages included in a packet.
                This lets us look up the set of messages that were included in that packet later on when it is acked, so we can ack those messages individually.
                @param messageIds The set of message ids that were included in the packet.
                @param numMessageIds The number of message ids in the array.
                @param sequence The sequence number of the connection packet the messages were included in.
            */
        void AddMessagePacketEntry(int connection_index, const std::array<uint16_t, max_messages_per_packet>& message_ids, int numMessageIds, std::uint16_t sequence) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            SentPacketEntry* sent_packet = m_sent_packets.Insert(connection_index, sequence);
            ASSERT_CRASH(sent_packet);
            if (sent_packet) {
                sent_packet->acked = 0;
                sent_packet->block = 0;
                sent_packet->time_sent = m_connection_manager->GetTime();
                sent_packet->messageIds = &m_sent_packet_message_ids_array[connection_index * max_messages_per_packet + (sequence % message_sent_queue_size) * max_messages_per_packet];
                sent_packet->numMessageIds = numMessageIds;
                for (int i = 0; i < numMessageIds; ++i) {
                    sent_packet->messageIds[i] = message_ids[i];
                }
            }
        }

        /**
                Track the oldest unacked message id in the send queue.
                Because messages are acked individually, the send queue is not a true queue and may have holes.
                Because of this it is necessary to periodically walk forward from the previous oldest unacked message id, to find the current oldest unacked message id.
                This lets us know our starting point for considering messages to include in the next packet we send.
                @see GetMessagesToSend
            */
        void UpdateOldestUnackedMessageID(int connection_index) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            const std::uint16_t stop_message_id = m_message_send_queues.GetSequence(connection_index);

            while (true) {
                if (m_oldest_unacked_message_ids[connection_index] == stop_message_id || m_message_send_queues.Find(connection_index, m_oldest_unacked_message_ids[connection_index])) {
                    break;
                }
                ++m_oldest_unacked_message_ids[connection_index];
            }

            ASSERT_CRASH(!SequenceGreaterThan(m_oldest_unacked_message_ids[connection_index], stop_message_id));
        }

        /**
                Get the channel error level.
                @returns The channel error level.
            */
        ChannelErrorLevel GetErrorLevel(int connection_index) const {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            return m_error_level_array[connection_index];
        };

        /**
                Set the channel error level.
                All errors go through this function to make debug logging easier.
            */
        void SetErrorLevel(int connection_index, ChannelErrorLevel error_level) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < max_connection_count);
            if (error_level != m_error_level_array[connection_index] && error_level != CHANNEL_ERROR_NONE) {
                CORE_DEBUG("Channel went into error state: {}", GetChannelErrorString(error_level));
            }
            m_error_level_array[connection_index] = error_level;
        }

        alignas(std::hardware_destructive_interference_size) std::array<std::uint16_t, max_connection_count> m_send_message_ids;           // Id of the next message to be added to the send queue.
        alignas(std::hardware_destructive_interference_size) std::array<std::uint16_t, max_connection_count> m_receive_message_ids;        // Id of the next message to be added to the receive queue.
        alignas(std::hardware_destructive_interference_size) std::array<std::uint16_t, max_connection_count> m_oldest_unacked_message_ids; // Id of the oldest unacked message in the send queue.

        /// Please consider your packet send rate and make sure you have at least a few seconds worth of entries in this buffer.
        ReliableBuffer<SentPacketEntry, message_sent_queue_size, max_connection_count> m_sent_packets;                       // Stores information per sent connection packet about messages and block data included in each packet. Used to walk from connection packet level acks to message and data block fragment level acks.
        ReliableBuffer<MessageSendQueueEntry, message_send_queue_size, max_connection_count> m_message_send_queues;          // Message send queue.
        ReliableBuffer<MessageReceiveQueueEntry, message_receive_queue_size, max_connection_count> m_message_receive_queues; // Message receive queue.

        alignas(std::hardware_destructive_interference_size) std::array<uint16_t, max_messages_per_packet * message_sent_queue_size * max_connection_count> m_sent_packet_message_ids_array; // Array of n message ids per sent connection packet. Allows the maximum number of messages per-packet to be allocated dynamically.

        alignas(std::hardware_destructive_interference_size) std::array<SendBlockData<max_number_of_fragments>, max_connection_count> m_send_blocks;                          // Data about the block being currently sent.
        alignas(std::hardware_destructive_interference_size) std::array<ReceiveBlockData<max_number_of_fragments, max_fragment_size>, max_connection_count> m_receive_blocks; // Data about the block being currently received.

        alignas(std::hardware_destructive_interference_size) std::array<std::uint64_t, CHANNEL_COUNTER_NUMBER_OF_COUNTERS * max_connection_count> m_counters_array; // Counters for unit testing, stats etc.
        connection_manager_class* m_connection_manager;
    };
}
