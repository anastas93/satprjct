#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

#include "sha256.h"

namespace crypto {
namespace hkdf {

// Размер выходного блока HMAC-SHA-256
constexpr size_t HASH_LEN = crypto::sha256::DIGEST_SIZE;

// Максимальная длина выходного материала HKDF (255 блоков по HASH_LEN байт)
constexpr size_t MAX_OKM_LEN = HASH_LEN * 255;

// Результат стадии HKDF-Extract
using Prk = std::array<uint8_t, HASH_LEN>;

// Выполнение стадии Extract (HMAC на соли и входном материале)
Prk extract(const uint8_t* salt, size_t salt_len, const uint8_t* ikm, size_t ikm_len);

// Выполнение стадии Expand и генерация выходного ключевого материала
bool expand(const Prk& prk, const uint8_t* info, size_t info_len, uint8_t* okm, size_t okm_len);

// Полный цикл HKDF (Extract + Expand)
bool derive(const uint8_t* salt,
            size_t salt_len,
            const uint8_t* ikm,
            size_t ikm_len,
            const uint8_t* info,
            size_t info_len,
            uint8_t* okm,
            size_t okm_len);

// Обёртка для std::array/std::vector
template <typename SaltContainer, typename IkmContainer, typename InfoContainer, typename OkmContainer>
bool derive(const SaltContainer& salt,
            const IkmContainer& ikm,
            const InfoContainer& info,
            OkmContainer& okm) {
  static_assert(sizeof(typename OkmContainer::value_type) == 1, "Ожидается контейнер байтов");
  return derive(reinterpret_cast<const uint8_t*>(salt.data()), salt.size(),
                reinterpret_cast<const uint8_t*>(ikm.data()), ikm.size(),
                reinterpret_cast<const uint8_t*>(info.data()), info.size(),
                reinterpret_cast<uint8_t*>(okm.data()), okm.size());
}

}  // namespace hkdf
}  // namespace crypto

