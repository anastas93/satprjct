#ifndef SATPRJCT_LIBS_CRYPTO_CHACHA20_POLY1305_H_
#define SATPRJCT_LIBS_CRYPTO_CHACHA20_POLY1305_H_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace crypto {
namespace chacha20poly1305 {

// Размеры ключа, нонса и тега для варианта IETF (12-байтовый nonce)
inline constexpr size_t KEY_SIZE = 32;                 // длина ключа алгоритма
inline constexpr size_t NONCE_SIZE = 12;               // длина нонса
inline constexpr size_t TAG_SIZE = 16;                 // длина тега Poly1305

// Шифрование с отделённым тегом аутентичности
// key/nonce могут иметь размеры 16 и 12 байт соответственно: ключ автоматически
// расширяется до 32 байт через HKDF, что обеспечивает совместимость с
// существующими 128-битными сессионными ключами.
bool encrypt(const uint8_t* key, size_t key_len,
             const uint8_t* nonce, size_t nonce_len,
             const uint8_t* aad, size_t aad_len,
             const uint8_t* input, size_t input_len,
             std::vector<uint8_t>& output,
             std::vector<uint8_t>& tag);

// Дешифрование и проверка тега. Возвращает true только при успешной
// верификации Poly1305.
bool decrypt(const uint8_t* key, size_t key_len,
             const uint8_t* nonce, size_t nonce_len,
             const uint8_t* aad, size_t aad_len,
             const uint8_t* input, size_t input_len,
             const uint8_t* tag, size_t tag_len,
             std::vector<uint8_t>& output);

} // namespace chacha20poly1305
} // namespace crypto

#endif  // SATPRJCT_LIBS_CRYPTO_CHACHA20_POLY1305_H_

