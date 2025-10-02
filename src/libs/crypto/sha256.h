#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace crypto {
namespace sha256 {

// Размер выходного хеша SHA-256 в байтах
constexpr size_t DIGEST_SIZE = 32;

// Контекст вычисления SHA-256
struct Context {
  std::array<uint32_t, 8> state{};   // внутреннее состояние
  std::array<uint8_t, 64> block{};   // буфер для входных данных
  uint64_t total_len = 0;            // количество обработанных байтов
  size_t buffer_len = 0;             // длина данных в буфере
};

// Инициализация контекста
void init(Context& ctx);

// Добавление данных в поток хеширования
void update(Context& ctx, const uint8_t* data, size_t len);

// Завершение вычислений и получение хеша
void finish(Context& ctx, uint8_t out[DIGEST_SIZE]);

// Упрощённая обёртка для получения SHA-256 от произвольного буфера
std::array<uint8_t, DIGEST_SIZE> hash(const uint8_t* data, size_t len);

// Упрощённая обёртка для работы с std::array/std::vector
template <typename Container>
std::array<uint8_t, DIGEST_SIZE> hash(const Container& c) {
  return hash(reinterpret_cast<const uint8_t*>(c.data()), c.size());
}

}  // namespace sha256
}  // namespace crypto

