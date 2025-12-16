#pragma once
#include <array>
#include <new>
#include <typeinfo>

#include <Lock.h>


namespace CoreNetwork::Replay {
    template <int connection_count>
    constexpr int LockCount() {
        int recent_sequence_lock_count_remainder = ((sizeof(std::uint64_t) * connection_count) % std::hardware_destructive_interference_size);
        int recent_sequence_lock_count = ((sizeof(std::uint64_t) * connection_count) / std::hardware_destructive_interference_size) + recent_sequence_lock_count_remainder;
        int received_packet_lock_count = connection_count;

        return recent_sequence_lock_count + received_packet_lock_count;
    }
    template <int connection_count>
    constexpr int LockOffsetReceivedPacket() {
        int recent_sequence_lock_count_remainder = ((sizeof(std::uint64_t) * connection_count) % std::hardware_destructive_interference_size);
        int recent_sequence_lock_count = ((sizeof(std::uint64_t) * connection_count) / std::hardware_destructive_interference_size) + recent_sequence_lock_count_remainder;
        return recent_sequence_lock_count;
    }

    template <int replay_protection_buffer_size, unsigned int connection_count>
    class ReplayGuard {
    public:
        ReplayGuard() : m_most_recent_sequence(0) {
            ResetAll();
        }
        ~ReplayGuard() = default;

        bool AlreadyReceived(int connection_index, std::uint64_t sequence) {
            {
                int lock_index = (connection_index * sizeof(std::uint64_t) / std::hardware_destructive_interference_size) + ((sizeof(std::uint64_t) * connection_index) % std::hardware_destructive_interference_size ? 1 : 0);
                READ_LOCK_IDX(lock_index);
                // replaying old packets
                if (sequence + replay_protection_buffer_size <= m_most_recent_sequence[connection_index]) {
                    return true;
                }
            }
            int lock_index = LockOffsetReceivedPacket<connection_count>() + connection_index;
            READ_LOCK_IDX(lock_index);
            int index;
            if constexpr (connection_count == 1) {
                index = static_cast<int>(sequence % replay_protection_buffer_size);
            }
            else {
                index = connection_index * replay_protection_buffer_size + static_cast<int>(sequence % replay_protection_buffer_size);
            }

            if (m_received_packet[index] == std::numeric_limits<uint64_t>::max()) {
                return false;
            }

            // replaying same packet, we need smaller equal as we accept future sequences
            if (m_received_packet[index] >= sequence) {
                return true;
            }

            return false;
        }

        void AdvanceSequence(int connection_index, std::uint64_t sequence) {
            {
                int lock_index = (connection_index * sizeof(std::uint64_t) / std::hardware_destructive_interference_size) + ((sizeof(std::uint64_t) * connection_index) % std::hardware_destructive_interference_size ? 1 : 0);
                WRITE_LOCK_IDX(lock_index);
                if (sequence > m_most_recent_sequence[connection_index]) {
                    m_most_recent_sequence[connection_index] = sequence;
                }
            }
            int lock_index = LockOffsetReceivedPacket<connection_count>() + connection_index;
            WRITE_LOCK_IDX(lock_index);
            int index;
            if constexpr (connection_count == 1) {
                index = static_cast<int>(sequence % replay_protection_buffer_size);
            }
            else {
                index = connection_index * replay_protection_buffer_size + static_cast<int>(sequence % replay_protection_buffer_size);
            }

            m_received_packet[index] = sequence;
        }
        void ResetAll() {
            m_most_recent_sequence.fill(0);
            m_received_packet.fill(std::numeric_limits<uint64_t>::max());
        }

        void Reset(int connection_index) {
            int sequence_lock_index = (connection_index * sizeof(std::uint64_t) / std::hardware_destructive_interference_size) + ((sizeof(std::uint64_t) * connection_index) % std::hardware_destructive_interference_size ? 1 : 0);
            WRITE_LOCK_IDX(sequence_lock_index);
            m_most_recent_sequence[connection_index] = 0;

            int received_packet_lock_index = LockOffsetReceivedPacket<connection_count>() + connection_index;
            WRITE_LOCK_IDX(received_packet_lock_index);
            std::fill(&m_received_packet[replay_protection_buffer_size * connection_index], &m_received_packet[replay_protection_buffer_size * (connection_index + 1)], std::numeric_limits<uint64_t>::max());
        }

    private:
        USE_MANY_LOCKS(LockCount<connection_count>())
        alignas(std::hardware_destructive_interference_size) std::array<std::uint64_t, connection_count> m_most_recent_sequence;
        alignas(std::hardware_destructive_interference_size) std::array<std::uint64_t, replay_protection_buffer_size * connection_count> m_received_packet;
    };
}
