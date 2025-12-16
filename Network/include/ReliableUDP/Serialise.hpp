#pragma once
#include <algorithm>
#include <array>

#include <Log.h>
#include <Serialise.h>

namespace CoreReliableUDP::Serialise {
    // [prefix_byte][sequence 2 bytes][sequence (16 bit or 8 bit)][ack bytes max 4 of them]
    // prefix_byte bit 1 is 0 for rUDP and 1 for fragmented
    int WritePacketHeader(std::byte* packet_data, std::uint16_t sequence, std::uint16_t ack, uint32_t ack_bits);

    // prefix_byte bit 1 is 0 for rUDP and 1 for fragmented
    // max_fragement_size is the maximum amount of data we want to store in one packet
    template <int fragment_size>
    int WriteFaragmentHeader(std::byte* packet_data, std::byte** q, std::byte* end, int number_of_fragments, int faragment_id, std::uint16_t sequence, std::uint16_t ack, uint32_t ack_bits) {
        std::byte* p = packet_data;

        CoreSerialise::WriteUint8(&p, std::byte(1));
        CoreSerialise::WriteUint16(&p, sequence);
        CoreSerialise::WriteUint8(&p, std::byte(static_cast<std::uint8_t>(faragment_id)));
        CoreSerialise::WriteUint8(&p, std::byte(static_cast<std::uint8_t>(number_of_fragments - 1)));

        if (faragment_id == 0) {
            int packet_header_bytes = WritePacketHeader(p, sequence, ack, ack_bits);
            p += packet_header_bytes;
        }

        int bytes_to_copy = fragment_size;
        if ((*q) + bytes_to_copy > end) {
            bytes_to_copy = (int)(end - (*q));
        }

        (void)std::copy_n((*q), bytes_to_copy, p);

        p += bytes_to_copy;
        (*q) += bytes_to_copy;

        return (int)(p - packet_data);
    }

    // prefix_byte bit 1 is 0 for rUDP and 1 for fragmented
    int ReadPacketHeader(std::byte* packet_data, int packet_bytes, uint16_t& sequence, uint16_t& ack, uint32_t& ack_bits);

    // prefix_byte bit 1 is 0 for rUDP and 1 for fragmented
    template <int max_number_of_fragments, int fragment_size>
    int ReadFragmentHeader(std::byte* packet_data,
            int packet_bytes,
            int& fragment_id,
            int& num_fragments,
            int& fragment_bytes,
            uint16_t& sequence,
            uint16_t& ack,
            uint32_t& ack_bits) {
        if (packet_bytes < FRAGMENT_HEADER_BYTES) {
            CORE_DEBUG("[CoreReliableUDP] Packet is too small to read fragment header");
            return -1;
        }

        std::byte* p = packet_data;

        std::uint8_t prefix_byte = CoreSerialise::ReadUint8(&p);
        if (prefix_byte != 1) {
            CORE_DEBUG("[CoreReliableUDP] Prefix byte is not a fragment");
            return -1;
        }

        sequence = CoreSerialise::ReadUint16(&p);
        fragment_id = static_cast<int>(CoreSerialise::ReadUint8(&p));
        num_fragments = static_cast<int>(CoreSerialise::ReadUint8(&p)) + 1;

        if (num_fragments > max_number_of_fragments) {
            CORE_DEBUG("[CoreReliableUDP] Number of fragments {} outside of range of max fragments {}", num_fragments, max_number_of_fragments);
            return -1;
        }

        if (fragment_id >= num_fragments) {
            CORE_DEBUG("[CoreReliableUDP] Fragment id {} outside of range of num fragments {}", fragment_id, num_fragments);
            return -1;
        }

        fragment_bytes = packet_bytes - FRAGMENT_HEADER_BYTES;

        uint16_t packet_sequence = 0;
        uint16_t packet_ack = 0;
        uint32_t packet_ack_bits = 0;

        if (fragment_id == 0) {
            int packet_header_bytes = ReadPacketHeader(packet_data + FRAGMENT_HEADER_BYTES,
                    packet_bytes,
                    packet_sequence,
                    packet_ack,
                    packet_ack_bits);

            if (packet_header_bytes < 0) {
                CORE_DEBUG("[CoreReliableUDP] Bad packet header in fragment");
                return -1;
            }

            if (packet_sequence != sequence) {
                CORE_DEBUG("[CoreReliableUDP] Bad packet sequence in fragment. expected {}, got {}", sequence, packet_sequence);
                return -1;
            }

            fragment_bytes = packet_bytes - packet_header_bytes - FRAGMENT_HEADER_BYTES;
        }

        ack = packet_ack;
        ack_bits = packet_ack_bits;

        if (fragment_bytes > fragment_size) {
            CORE_DEBUG("[CoreReliableUDP] Fragment bytes {} > fragment size {}", fragment_bytes, fragment_size);
            return -1;
        }

        if (fragment_id != num_fragments - 1 && fragment_bytes != fragment_size) {
            CORE_DEBUG("[CoreReliableUDP] Fragment {} is {} bytes, which is not the expected fragment size {}", fragment_id, fragment_bytes, fragment_size);
            return -1;
        }

        return static_cast<int>(p - packet_data);
    }
}
