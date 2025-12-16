#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include <new>

#include "../../Macro.h"

namespace CoreReliableUDP {
    /**
        Compares two 16 bit sequence numbers and returns true if the first one is greater than the second (considering wrapping).
        IMPORTANT: This is not the same as s1 > s2!
        Greater than is defined specially to handle wrapping sequence numbers.
        If the two sequence numbers are close together, it is as normal, but they are far apart, it is assumed that they have wrapped around.
        Thus, sequence_greater_than( 1, 0 ) returns true, and so does sequence_greater_than( 0, 65535 )!
        @param s1 The first sequence number.
        @param s2 The second sequence number.
        @returns True if the s1 is greater than s2, with sequence number wrapping considered.
    */

    inline bool SequenceGreaterThan(std::uint16_t s1, std::uint16_t s2) {
        return ((s1 > s2) && (s1 - s2 <= 32768)) ||
               ((s1 < s2) && (s2 - s1 > 32768));
    }

    /**
        Compares two 16 bit sequence numbers and returns true if the first one is less than the second (considering wrapping).
        IMPORTANT: This is not the same as s1 < s2!
        Greater than is defined specially to handle wrapping sequence numbers.
        If the two sequence numbers are close together, it is as normal, but they are far apart, it is assumed that they have wrapped around.
        Thus, sequence_less_than( 0, 1 ) returns true, and so does sequence_greater_than( 65535, 0 )!
        @param s1 The first sequence number.
        @param s2 The second sequence number.
        @returns True if the s1 is less than s2, with sequence number wrapping considered.
     */

    inline bool SequenceLessThan(std::uint16_t s1, std::uint16_t s2) {
        return SequenceGreaterThan(s2, s1);
    }

    /**
    Data structure that stores data indexed by sequence number.
    Entries may or may not exist. If they don't exist the sequence value for the entry at that index is set to 0xFFFFFFFF.
    This provides a constant time lookup for an entry by sequence number. If the entry at sequence modulo buffer size doesn't have the same sequence number, that sequence number is not stored.
    This is incredibly useful and is used as the foundation of the packet level ack system and the reliable message send and receive queues.
    */
    template <class T, int max_element_number, unsigned int connection_count>
    class ReliableBuffer {
    public:
        ReliableBuffer() {
            static_assert(max_element_number > 0);
            static_assert(connection_count > 0);
            static_assert(sizeof(T) > 0);
            ResetAll();
        }
        ~ReliableBuffer() = default;

        /**
            Reset the sequence buffer.
            Removes all entries from the sequence buffer and restores it to initial state.
        */
        void ResetAll() {
            m_sequence.fill(0);
            m_entry_sequence.fill(std::numeric_limits<uint32_t>::max());
            m_entry_data.fill(T());
        }
        void Reset(int connection_index) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < connection_count);
            m_sequence[connection_index] = 0;
            std::fill(&m_entry_sequence[connection_index * max_element_number], &m_entry_sequence[(connection_index + 1) * max_element_number], std::numeric_limits<uint32_t>::max());
            std::fill(&m_entry_data[connection_index * max_element_number], &m_entry_data[(connection_index + 1) * max_element_number], T());
        }

        // testing if the packet sequence is older than the current packet sequence - buffer size
        bool TestInsert(int connection_index, std::uint16_t sequence) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < connection_count);
            return SequenceLessThan(sequence, m_sequence[connection_index] - static_cast<std::uint16_t>(max_element_number)) ? false : true;
        }
        /**
            Insert an entry in the sequence buffer.
            IMPORTANT: If another entry exists at the sequence modulo buffer size, it is overwritten.
            @param sequence The sequence number.
            @param guaranteed_order Whether sequence is always the newest value (when sending) or can be out of order (when receiving).
            @returns The sequence buffer entry, which you must fill with your data. nullptr if a sequence buffer entry could not be added for your sequence number (if the sequence number is too old for example).
        */
        T* Insert(int connection_index, std::uint16_t sequence) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < connection_count);
            // check if it is a new sequence
            if (SequenceGreaterThan(sequence + 1, m_sequence[connection_index])) {
                RemoveEntries(connection_index, m_sequence[connection_index], sequence);
                m_sequence[connection_index] = sequence + 1;
            }
            else if (SequenceLessThan(sequence, m_sequence[connection_index] - static_cast<std::uint16_t>(max_element_number))) {
                // testing if the packet sequence is older than the current packet sequence - buffer size + 1
                return nullptr;
            }
            const int index = connection_index * max_element_number + sequence % max_element_number;
            m_entry_sequence[index] = sequence;
            return &m_entry_data[index];
        }
        template <typename Func>
            requires(std::invocable<Func, T*>)
        T* InsertWithCleanup(int connection_index, uint16_t sequence, Func cleanup_function) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < connection_count);
            if (SequenceGreaterThan(sequence + 1, m_sequence[connection_index])) {
                RemoveEntriesWithCleanup(connection_index, m_sequence[connection_index], sequence, cleanup_function);
                m_sequence[connection_index] = sequence + 1;
            }
            else if (SequenceLessThan(sequence, m_sequence[connection_index] - static_cast<std::uint16_t>(max_element_number))) {
                return nullptr;
            }
            int index = connection_index * max_element_number + sequence % max_element_number;
            if (m_entry_sequence[index] != std::numeric_limits<uint32_t>::max()) {
                cleanup_function(&m_entry_data[index]);
            }
            m_entry_sequence[index] = sequence;
            return &m_entry_data[index];
        }

        /**
            Remove an entry from the sequence buffer.
            @param sequence The sequence number of the entry to remove.
        */
        void Remove(int connection_index, std::uint16_t sequence) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < connection_count);
            m_entry_sequence[connection_index * max_element_number + sequence % max_element_number] = std::numeric_limits<uint32_t>::max();
        }
        template <typename Func>
            requires(std::invocable<Func, T*>)
        void RemoveWithCleanup(int connection_index, std::uint16_t sequence, Func cleanup_function) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < connection_count);
            int index = connection_index * max_element_number + sequence % max_element_number;
            if (m_entry_sequence[index] != std::numeric_limits<uint32_t>::max()) {
                m_entry_sequence[index] = std::numeric_limits<uint32_t>::max();
                cleanup_function(&m_entry_data[index]);
            }
        }
        /**
            Helper function to remove entries.
            This is used to remove old entries as we advance the sequence buffer forward.
            Otherwise, if when entries are added with holes (eg. receive buffer for packets or messages, where not all sequence numbers are added to the buffer because we have high packet loss),
            and we are extremely unlucky, we can have old sequence buffer entries from the previous sequence # wrap around still in the buffer, which corrupts our internal connection state.
        */
        void RemoveEntries(int connection_index, int start_sequence, int finish_sequence) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < connection_count);
            if (finish_sequence < start_sequence) {
                finish_sequence += 65536;
            }
            ASSERT_CRASH(finish_sequence >= start_sequence);
            if (finish_sequence - start_sequence < max_element_number) {
                for (int sequence = start_sequence; sequence <= finish_sequence; ++sequence) {
                    m_entry_sequence[connection_index * max_element_number + sequence % max_element_number] = std::numeric_limits<uint32_t>::max();
                }
            }
            else {
                std::fill(&m_entry_sequence[connection_index * max_element_number], &m_entry_sequence[(connection_index + 1) * max_element_number], std::numeric_limits<uint32_t>::max());
            }
        }
        template <typename Func>
            requires(std::invocable<Func, T*>)
        void RemoveEntriesWithCleanup(int connection_index, int start_sequence, int finish_sequence, Func cleanup_function) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < connection_count);
            if (finish_sequence < start_sequence) {
                finish_sequence += 65536;
            }
            if (finish_sequence - start_sequence < max_element_number) {
                int sequence;
                for (sequence = start_sequence; sequence <= finish_sequence; ++sequence) {
                    cleanup_function(&m_entry_data[connection_index * max_element_number + sequence % max_element_number]);
                    m_entry_sequence[connection_index * max_element_number + sequence % max_element_number] = std::numeric_limits<uint32_t>::max();
                }
            }
            else {
                for (int i = connection_index * max_element_number; i < (connection_index + 1) * max_element_number; ++i) {
                    cleanup_function(&m_entry_data[i]);
                }
                std::fill(&m_entry_sequence[connection_index * max_element_number], &m_entry_sequence[(connection_index + 1) * max_element_number], std::numeric_limits<uint32_t>::max());
            }
        }

        void AdvanceSequence(int connection_index, std::uint16_t sequence) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < connection_count);
            if (SequenceGreaterThan(sequence + 1, m_sequence[connection_index])) {
                RemoveEntries(connection_index, m_sequence[connection_index], sequence);
                m_sequence[connection_index] = sequence + 1;
            }
        }
        template <typename Func>
            requires(std::invocable<Func, T*>)
        void AdvanceSequenceWithCleanup(int connection_index, std::uint16_t sequence, Func cleanup_function) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < connection_count);
            if (SequenceGreaterThan(sequence + 1, m_sequence[connection_index])) {
                RemoveEntriesWithCleanup(connection_index, m_sequence[connection_index], sequence, cleanup_function);
                m_sequence[connection_index] = sequence + 1;
            }
        }

        // generate acks for last 32 messages if we received them
        void GenerateAcknowledgementBits(int connection_index, std::uint16_t& ack, uint32_t& ack_bits) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < connection_count);

            ack = m_sequence[connection_index] - 1;
            ack_bits = 0;
            uint32_t mask = 1;
            for (int i = 0; i < 32; ++i) {
                uint16_t sequence = ack - static_cast<uint16_t>(i);
                if (Exists(connection_index, sequence)) {
                    ack_bits |= mask;
                }
                mask <<= 1;
            }
        }

        /**
            Get the entry at the specified index.
            Use this to iterate across entries in the sequence buffer.
            @param index The entry index in [0,GetSize()-1].
            @returns The entry if it exists. nullptr if no entry is in the buffer at the specified index.
        */
        T* GetAtIndex(int connection_index, int index) {
            ASSERT_CRASH(index >= 0);
            ASSERT_CRASH(index < max_element_number);
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < connection_count);

            return m_entry_sequence[connection_index * max_element_number + index] != std::numeric_limits<uint32_t>::max() ? &m_entry_data[connection_index * max_element_number + index] : nullptr;
        }
        /**
            Get the entry at the specified index (const version).
            Use this to iterate across entries in the sequence buffer.
            @param index The entry index in [0,GetSize()-1].
            @returns The entry if it exists. nullptr if no entry is in the buffer at the specified index.
        */
        const T* GetAtIndex(int connection_index, int index) const {
            ASSERT_CRASH(index >= 0);
            ASSERT_CRASH(index < max_element_number);
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < connection_count);

            return m_entry_sequence[connection_index * max_element_number + index] != std::numeric_limits<uint32_t>::max() ? &m_entry_data[connection_index * max_element_number + index] : nullptr;
        }

        /**
            Is the entry corresponding to the sequence number available? eg. Currently unoccupied.
            This works because older entries are automatically set back to unoccupied state as the sequence buffer advances forward.
            @param sequence The sequence number.
            @returns True if the sequence buffer entry is available, false if it is already occupied.
        */
        bool Available(int connection_index, std::uint16_t sequence) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < connection_count);
            return m_entry_sequence[connection_index * max_element_number + sequence % max_element_number] == std::numeric_limits<uint32_t>::max();
        }

        /**
            Does an entry exist for a sequence number?
            @param sequence The sequence number.
            @returns True if an entry exists for this sequence number.
        */
        bool Exists(int connection_index, std::uint16_t sequence) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < connection_count);
            return m_entry_sequence[connection_index * max_element_number + sequence % max_element_number] == static_cast<std::uint32_t>(sequence);
        }

        /**
            Get the entry corresponding to a sequence number.
            @param sequence The sequence number.
            @returns The entry if it exists. nullptr if no entry is in the buffer for this sequence number.
        */
        T* Find(int connection_index, std::uint16_t sequence) {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < connection_count);
            const int index = connection_index * max_element_number + sequence % max_element_number;
            return (m_entry_sequence[index] == static_cast<std::uint32_t>(sequence)) ? (&m_entry_data[index]) : nullptr;
        }
        /**
            Get the entry corresponding to a sequence number (const version).
            @param sequence The sequence number.
            @returns The entry if it exists. nullptr if no entry is in the buffer for this sequence number.
        */
        const T* Find(int connection_index, std::uint16_t sequence) const {
            ASSERT_CRASH(connection_index >= 0);
            ASSERT_CRASH(connection_index < connection_count);
            const int index = connection_index * max_element_number + sequence % max_element_number;
            return (m_entry_sequence[index] == static_cast<std::uint32_t>(sequence)) ? (&m_entry_data[index]) : nullptr;
        }


        /**
            Get the most recent sequence number added to the buffer.
            This sequence number can wrap around, so if you are at 65535 and add an entry for sequence 0, then 0 becomes the new "most recent" sequence number.
            @returns The most recent sequence number.
            @see SequenceGreaterThan
            @see SequenceLessThan
        */
        uint16_t GetSequence(int connection_index) const {
            return m_sequence[connection_index];
        }
        /**
            Get the entry index for a sequence number.
            This is simply the sequence number modulo the sequence buffer size.
            @param sequence The sequence number.
            @returns The sequence buffer index corresponding of the sequence number.
        */
        int GetIndex(uint16_t sequence) const {
            return sequence % max_element_number;
        }
        /**
            Get the size of the sequence buffer.
            @returns The size of the sequence buffer (number of entries).
        */
        inline constexpr int GetSize() const {
            return max_element_number;
        }

    private:
        /// \todo this needs locking as we are using this over many threads
        alignas(std::hardware_destructive_interference_size) std::array<std::uint16_t, connection_count> m_sequence;
        alignas(std::hardware_destructive_interference_size) std::array<std::uint32_t, max_element_number * connection_count> m_entry_sequence;
        alignas(std::hardware_destructive_interference_size) std::array<T, max_element_number * connection_count> m_entry_data;
    };
}