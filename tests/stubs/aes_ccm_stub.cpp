#include <cstddef>
#include <cstdint>
#include <vector>

bool encrypt_ccm(const uint8_t*, size_t,
                 const uint8_t*, size_t,
                 const uint8_t*, size_t,
                 const uint8_t*, size_t,
                 std::vector<uint8_t>& out,
                 std::vector<uint8_t>& tag,
                 size_t tag_len) {
  // Заглушка для тестов: функция никогда не используется, просто очищаем буферы.
  out.clear();
  tag.assign(tag_len, 0);
  return false;
}

bool decrypt_ccm(const uint8_t*, size_t,
                 const uint8_t*, size_t,
                 const uint8_t*, size_t,
                 const uint8_t*, size_t,
                 const uint8_t*, size_t,
                 std::vector<uint8_t>& out) {
  // Заглушка для тестов: возвращаем false, реальная реализация не требуется.
  out.clear();
  return false;
}
