#include <array>
#include <source_location>

#include "../../Macro.h"
#include "Serialise/SerialiseReliable.h"


namespace CoreReliableUDP::Serialise {
    int WritePacketHeader(std::byte* packet_data, std::uint16_t sequence, std::uint16_t ack, uint32_t ack_bits) {
        std::byte* p = packet_data;

        std::uint8_t prefix_byte = 0;

        if ((ack_bits & 0x000000FF) != 0x000000FF) {
            prefix_byte |= (1 << 1);
        }

        if ((ack_bits & 0x0000FF00) != 0x0000FF00) {
            prefix_byte |= (1 << 2);
        }

        if ((ack_bits & 0x00FF0000) != 0x00FF0000) {
            prefix_byte |= (1 << 3);
        }

        if ((ack_bits & 0xFF000000) != 0xFF000000) {
            prefix_byte |= (1 << 4);
        }

        int sequence_difference = sequence - ack;
        if (sequence_difference < 0) {
            sequence_difference += 65536;
        }
        if (sequence_difference <= 255) { // 255 is max of 8 bit integer we decide wether we send 16 bit sequence or just the difference
            prefix_byte |= (1 << 5);
        }

        CoreSerialise::WriteUint8(&p, std::byte(prefix_byte));

        CoreSerialise::WriteUint16(&p, sequence);

        if (sequence_difference <= 255) {
            CoreSerialise::WriteUint8(&p, std::byte(static_cast<std::uint8_t>(sequence_difference)));
        }
        else {
            CoreSerialise::WriteUint16(&p, ack);
        }

        if ((ack_bits & 0x000000FF) != 0x000000FF) {
            CoreSerialise::WriteUint8(&p, std::byte(static_cast<std::uint8_t>(ack_bits & 0x000000FF)));
        }

        if ((ack_bits & 0x0000FF00) != 0x0000FF00) {
            CoreSerialise::WriteUint8(&p, std::byte(static_cast<std::uint8_t>((ack_bits & 0x0000FF00) >> 8)));
        }

        if ((ack_bits & 0x00FF0000) != 0x00FF0000) {
            CoreSerialise::WriteUint8(&p, std::byte(static_cast<std::uint8_t>((ack_bits & 0x00FF0000) >> 16)));
        }

        if ((ack_bits & 0xFF000000) != 0xFF000000) {
            CoreSerialise::WriteUint8(&p, std::byte(static_cast<std::uint8_t>((ack_bits & 0xFF000000) >> 24)));
        }

        ASSERT_CRASH(p - packet_data <= MAX_RUDP_HEADER_BYTES);

        return static_cast<int>(p - packet_data);
    }

    int ReadPacketHeader(std::byte* packet_data, int packet_bytes, uint16_t& sequence, uint16_t& ack, uint32_t& ack_bits) {
        if (packet_bytes < MIN_RUDP_HEADER_BYTES) {
            CORE_DEBUG("[CoreReliableUDP] Packet too small for packet header (1)");
            return -1;
        }

        std::byte* p = packet_data;

        std::uint8_t prefix_byte = CoreSerialise::ReadUint8(&p);

        if ((prefix_byte & 0x1) != 0) {
            std::source_location src = std::source_location::current();
            CORE_DEBUG("[CoreReliableUDP] Prefix byte does not indicate a regular packet");
            return -1;
        }

        sequence = CoreSerialise::ReadUint16(&p);

        // see WritePacketHeader function for explanation
        if (prefix_byte & (1 << 5)) {
            std::uint8_t sequence_difference = CoreSerialise::ReadUint8(&p);
            ack = sequence - sequence_difference;
        }
        else {
            if (packet_bytes < 3 + 2) {
                CORE_DEBUG("[CoreReliableUDP] Packet too small for packet header (2)");
                return -1;
            }
            ack = CoreSerialise::ReadUint16(&p);
        }

        int expected_bytes = 0;
        int i;
        for (i = 1; i <= 4; ++i) {
            if (prefix_byte & (1 << i)) {
                ++expected_bytes;
            }
        }
        if (packet_bytes < (p - packet_data) + expected_bytes) {
            CORE_DEBUG("[CoreReliableUDP] Packet too small for packet header (4)");
            return -1;
        }

        ack_bits = 0xFFFFFFFF;

        if (prefix_byte & (1 << 1)) {
            ack_bits &= 0xFFFFFF00;
            ack_bits |= (uint32_t)(CoreSerialise::ReadUint8(&p));
        }

        if (prefix_byte & (1 << 2)) {
            ack_bits &= 0xFFFF00FF;
            ack_bits |= (uint32_t)(CoreSerialise::ReadUint8(&p)) << 8;
        }

        if (prefix_byte & (1 << 3)) {
            ack_bits &= 0xFF00FFFF;
            ack_bits |= (uint32_t)(CoreSerialise::ReadUint8(&p)) << 16;
        }

        if (prefix_byte & (1 << 4)) {
            ack_bits &= 0x00FFFFFF;
            ack_bits |= (uint32_t)(CoreSerialise::ReadUint8(&p)) << 24;
        }

        return static_cast<int>(p - packet_data);
    }
}
