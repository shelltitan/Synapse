#pragma once
#include <cstdint>
#include "../../Macro.h"

namespace CoreReliableUDP::Error {
    /**
            rUDP error level.
            If the rUDP connections gets in an error the connection should be terminated.
        */
    enum ErrorLevel : std::uint8_t {
        ERROR_NONE = 0,                    // No error. All is well.
        CHANNEL_ERROR_DESYNC,              // This channel has desynced. This means that the connection protocol has desynced and cannot recover. The client should be disconnected.
        CHANNEL_ERROR_SEND_QUEUE_FULL,     // The user tried to send a message but the send queue was full. This will assert out in development, but in production it sets this error on the channel.
        CHANNEL_ERROR_FAILED_TO_SERIALISE, // Serialise read failed for a message sent to this channel. Check your message serialize functions, one of them is returning false on serialise read. This can also be caused by a desync in message read and write.
        CHANNEL_ERROR_OUT_OF_MEMORY        // The channel tried to allocate some memory but couldn't.
    };

    /// Helper function to convert a channel error to a user friendly string.

    inline const char* GetErrorString(ErrorLevel error) {
        switch (error) {
            case ERROR_NONE:
                return "None";
            case CHANNEL_ERROR_DESYNC:
                return "Desync";
            case CHANNEL_ERROR_SEND_QUEUE_FULL:
                return "Send queue full";
            case CHANNEL_ERROR_OUT_OF_MEMORY:
                return "Out of memory";
            case CHANNEL_ERROR_FAILED_TO_SERIALISE:
                return "Failed to serialise";
            default:
                ASSERT_CRASH(false);
                return "(Unknown)";
        }
    }
}
