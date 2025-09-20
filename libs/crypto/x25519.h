#pragma once
#include <array>
#include <cstdint>
#include <cstddef>

namespace crypto {
namespace x25519 {

// Размер ключей Curve25519
constexpr size_t KEY_SIZE = 32;

// Генерация пары ключей на Curve25519
// private_key должен быть заполнен случайными байтами и будет "зажат" согласно RFC7748
// public_key = private_key * base_point
void derive_public(std::array<uint8_t, KEY_SIZE>& public_key,
                   std::array<uint8_t, KEY_SIZE>& private_key);

// Вычисление общего секрета: shared = private_key * peer_public
// Возвращает false при недопустимом публичном ключе (все нули)
bool compute_shared(const std::array<uint8_t, KEY_SIZE>& private_key,
                    const std::array<uint8_t, KEY_SIZE>& peer_public,
                    std::array<uint8_t, KEY_SIZE>& shared_secret);

// Заполнение буфера случайными байтами, совместимая с ESP32 и ПК реализация
void random_bytes(uint8_t* out, size_t len);

}  // namespace x25519
}  // namespace crypto

