#include <libassert/assert.hpp>
#include "CoreCrypto.h"

CoreCrypto::CoreCrypto() {
    /// \todo move to init function
    if (sodium_init() == -1) {
        DEBUG_ASSERT(false, "Sodium failed");
        std::exit(1);
    }
}

bool CoreCrypto::EncryptMessage(std::byte* message, std::uint64_t message_length,
    std::byte* additional, std::uint64_t additional_length,
    const std::byte* nonce,
    const std::byte* key) {
    unsigned long long encrypted_length;

    int result = crypto_aead_xchacha20poly1305_ietf_encrypt(std::bit_cast<std::uint8_t*>(message), &encrypted_length,
        std::bit_cast<std::uint8_t*>(message), static_cast<unsigned long long>(message_length),
        std::bit_cast<std::uint8_t*>(additional), std::bit_cast<unsigned long long>(additional_length),
        nullptr, std::bit_cast<const std::uint8_t*>(nonce), std::bit_cast<const std::uint8_t*>(key));

    if (result != 0) {
        return false;
    }

    DEBUG_ASSERT(encrypted_length == message_length + MAC_BYTES);

    return true;
}

bool CoreCrypto::DecryptMessage(std::byte* message, std::uint64_t message_length,
    std::byte* additional, std::uint64_t additional_length,
    const std::byte* nonce,
    const std::byte* key) {
    unsigned long long decrypted_length;

    int result = crypto_aead_xchacha20poly1305_ietf_decrypt(std::bit_cast<std::uint8_t*>(message), &decrypted_length,
        nullptr,
        std::bit_cast<std::uint8_t*>(message), std::bit_cast<unsigned long long>(message_length),
        std::bit_cast<std::uint8_t*>(additional), std::bit_cast<unsigned long long>(additional_length),
        std::bit_cast<const std::uint8_t*>(nonce), std::bit_cast<const std::uint8_t*>(key));

    if (result != 0) {
        return false;
    }

    DEBUG_ASSERT(decrypted_length == message_length - MAC_BYTES);

    return true;
}

void CoreCrypto::RandomBytes(std::byte* buffer, std::size_t buffer_size) {
    randombytes_buf(buffer, buffer_size);
}