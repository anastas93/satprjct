#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// Универсальные вспомогательные функции для побайтного чтения HTTP-тела.
namespace HttpBodyReader {

// Читает поток побайтно в вектор, контролируя ограничение размера.
// Возвращает true при полном успешном чтении.
// lengthExceeded выставляется при превышении допустимого размера,
// incomplete — если соединение завершилось до получения всех байтов.
template <typename Client, typename DelayCallback>
bool readBodyToVector(Client& client,
                      size_t contentLength,
                      size_t maxPayload,
                      std::vector<uint8_t>& out,
                      DelayCallback delayCb,
                      bool& lengthExceeded,
                      bool& incomplete) {
  constexpr size_t kUnknownLength = static_cast<size_t>(-1);
  lengthExceeded = false;
  incomplete = false;
  out.clear();

  if (contentLength != kUnknownLength) {
    if (contentLength > maxPayload) {
      lengthExceeded = true;
      return false;
    }
    out.resize(contentLength);
    size_t offset = 0;
    while (offset < contentLength) {
      auto chunk = client.read(out.data() + offset, contentLength - offset);
      if (chunk < 0) {
        incomplete = true;
        break;
      }
      if (chunk == 0) {
        if (!client.connected()) {
          incomplete = true;
          break;
        }
        delayCb();
        continue;
      }
      offset += static_cast<size_t>(chunk);
    }
    if (offset != contentLength) {
      out.resize(offset);
      incomplete = true;
      return false;
    }
    return true;
  }

  while (client.connected() || client.available()) {
    while (client.available()) {
      int value = client.read();
      if (value < 0) {
        incomplete = true;
        return false;
      }
      out.push_back(static_cast<uint8_t>(value));
      if (out.size() > maxPayload) {
        lengthExceeded = true;
        return false;
      }
    }
    if (!client.connected() && !client.available()) {
      break;
    }
    delayCb();
  }

  if (out.empty()) {
    incomplete = true;
    return false;
  }
  return true;
}

}  // namespace HttpBodyReader

