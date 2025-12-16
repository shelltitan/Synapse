#pragma once
#include <cstdint>

#pragma once
#include <cstddef>
#include <memory>

#include <sodium.h>

class CoreCrypto {
public:
    CoreCrypto();

    static constexpr unsigned int MAC_BYTES = crypto_aead_chacha20poly1305_ietf_ABYTES;

    static bool EncryptMessage(std::byte* message, std::uint64_t message_length,
        std::byte* additional, std::uint64_t additional_length,
        const std::byte* nonce,
        const std::byte* key);
    static bool DecryptMessage(std::byte* message, std::uint64_t message_length,
        std::byte* additional, std::uint64_t additional_length,
        const std::byte* nonce,
        const std::byte* key);

    static void RandomBytes(std::byte* buffer, std::size_t buffer_size);
};

namespace Global {
    inline std::unique_ptr<CoreCrypto> GCryptoManager = std::make_unique<CoreCrypto>();
}