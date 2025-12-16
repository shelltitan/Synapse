#pragma once
#include <cstddef>
#include <cstdint>

namespace CoreReliableUDP::MessageChannel {
    struct ChannelMessage {
        std::uint16_t message_protocol{ 0 };
        std::uint16_t message_id{ 0 }; // this packet sequence for unreliable channel
        std::uint16_t is_block : 1 { false };
        std::uint16_t block_offset : 15 { 0 }; // offset from start of message_data where the block data begins
        std::uint16_t block_size{ 0 };
        std::byte* message_data{ nullptr };

        auto Reset() -> void {
            message_protocol = 0;
            message_id = 0;
            is_block = 0;
            block_offset = 0;
            block_size = 0;
            message_data = nullptr;
        }
    };

    /**
            Channel counters provide insight into the number of times an action was performed by a channel.
            They are intended for use in a telemetry system, eg. reported to some backend logging system to track behavior in a production environment.
        */
    enum ChannelCounters {
        CHANNEL_COUNTER_MESSAGES_SENT,     // Number of messages sent over this channel.
        CHANNEL_COUNTER_MESSAGES_RECEIVED, // Number of messages received over this channel.
        CHANNEL_COUNTER_NUMBER_OF_COUNTERS // The number of channel counters.
    };
}
