#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace crypto {
namespace ed25519 {

constexpr size_t PUBLIC_KEY_SIZE = 32;   // размер публичного ключа Ed25519
constexpr size_t SIGNATURE_SIZE = 64;    // размер подписи Ed25519

// Проверка подписи Ed25519 над произвольным сообщением
bool verify(const uint8_t* message, size_t message_len,
            const std::array<uint8_t, SIGNATURE_SIZE>& signature,
            const std::array<uint8_t, PUBLIC_KEY_SIZE>& public_key);

}  // namespace ed25519
}  // namespace crypto

