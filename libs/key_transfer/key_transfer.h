#pragma once
#include <array>
#include <cstdint>
#include <vector>

// Вспомогательные функции для передачи корневого ключа через LoRa
namespace KeyTransfer {

// Константы формата пакета
constexpr size_t TAG_LEN = 8;                            // длина тега аутентичности AES-CCM
constexpr uint8_t MAGIC[4] = {'K','T','R','F'};          // сигнатура пакета
constexpr uint8_t VERSION_LEGACY = 1;                    // версия без эпемерных ключей
constexpr uint8_t VERSION_EPHEMERAL = 2;                 // версия с эпемерным ключом
constexpr uint8_t FLAG_HAS_EPHEMERAL = 0x01;             // флаг наличия эпемерного ключа

// Структура полезной нагрузки после расшифровки кадра
struct FramePayload {
  std::array<uint8_t,32> public_key{};                  // долговременный публичный ключ
  std::array<uint8_t,4> key_id{};                       // идентификатор сессионного ключа
  std::array<uint8_t,32> ephemeral_public{};            // эпемерный публичный ключ (если присутствует)
  bool has_ephemeral = false;                           // признак наличия эпемерного ключа
  uint8_t version = VERSION_LEGACY;                     // версия формата кадра
  uint8_t flags = 0;                                    // дополнительные флаги
};

// Специальный корневой ключ AES-CCM, используемый только для обмена ключами
const std::array<uint8_t,16>& rootKey();

// Формирование нонса для обмена ключами (по msg_id и индексу фрагмента)
std::array<uint8_t,12> makeNonce(uint32_t msg_id, uint16_t frag_idx);

// Подготовка кадра с публичным ключом: на выходе полный LoRa-кадр
bool buildFrame(uint32_t msg_id,
                const std::array<uint8_t,32>& public_key,
                const std::array<uint8_t,4>& key_id,
                std::vector<uint8_t>& frame_out,
                const std::array<uint8_t,32>* ephemeral_public = nullptr);

// Разбор кадра: при успешном декодировании возвращает true и заполняет поля
bool parseFrame(const uint8_t* frame, size_t len,
                FramePayload& payload,
                uint32_t& msg_id_out);

// Совместимость: обёртка вокруг нового парсера без доступа к эпемерному ключу
bool parseFrame(const uint8_t* frame, size_t len,
                std::array<uint8_t,32>& public_key,
                std::array<uint8_t,4>& key_id,
                uint32_t& msg_id_out);

}
