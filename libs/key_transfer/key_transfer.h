#pragma once
#include <array>
#include <cstdint>
#include <vector>

// Вспомогательные функции для передачи корневого ключа через LoRa
namespace KeyTransfer {

// Константы формата пакета
constexpr size_t TAG_LEN = 8;                            // длина тега аутентичности AES-CCM
constexpr uint8_t MAGIC[4] = {'K','T','R','F'};          // сигнатура пакета
constexpr uint8_t VERSION = 1;                           // версия формата

// Специальный корневой ключ AES-CCM, используемый только для обмена ключами
const std::array<uint8_t,16>& rootKey();

// Формирование нонса для обмена ключами (по msg_id и индексу фрагмента)
std::array<uint8_t,12> makeNonce(uint32_t msg_id, uint16_t frag_idx);

// Подготовка кадра с публичным ключом: на выходе полный LoRa-кадр
bool buildFrame(uint32_t msg_id,
                const std::array<uint8_t,32>& public_key,
                const std::array<uint8_t,4>& key_id,
                std::vector<uint8_t>& frame_out);

// Разбор кадра: при успешном декодировании возвращает true и заполняет поля
bool parseFrame(const uint8_t* frame, size_t len,
                std::array<uint8_t,32>& public_key,
                std::array<uint8_t,4>& key_id,
                uint32_t& msg_id_out);

}
