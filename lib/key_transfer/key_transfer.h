#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include <string>

// Вспомогательные функции для передачи корневого ключа через LoRa
namespace KeyTransfer {

// Константы формата пакета
constexpr uint8_t FRAME_VERSION_AEAD = 2;                // версия кадра с ChaCha20-Poly1305
constexpr size_t TAG_LEN_V1 = 8;                         // длина тега в старом формате AES-CCM
constexpr size_t TAG_LEN = 16;                           // длина тега Poly1305
constexpr uint8_t MAGIC[4] = {'K','T','R','F'};          // сигнатура пакета
constexpr uint8_t VERSION_LEGACY = 1;                    // версия без эпемерных ключей
constexpr uint8_t VERSION_EPHEMERAL = 2;                 // версия с эпемерным ключом
constexpr uint8_t VERSION_CERTIFICATE = 3;               // версия с цепочкой сертификатов
constexpr uint8_t FLAG_HAS_EPHEMERAL = 0x01;             // флаг наличия эпемерного ключа
constexpr uint8_t FLAG_HAS_CERTIFICATE = 0x02;           // флаг наличия сертификата

// Простая запись сертификата Ed25519 в цепочке доверия
struct CertificateRecord {
  std::array<uint8_t,32> issuer_public{};               // публичный ключ подписавшего субъекта
  std::array<uint8_t,64> signature{};                   // подпись Ed25519 на предшествующем ключе
};

struct CertificateBundle {
  bool valid = false;                                   // присутствует ли подпись
  std::vector<CertificateRecord> chain;                 // цепочка от публичного ключа до корня
};

// Структура полезной нагрузки после расшифровки кадра
struct FramePayload {
  std::array<uint8_t,32> public_key{};                  // долговременный публичный ключ
  std::array<uint8_t,4> key_id{};                       // идентификатор сессионного ключа
  std::array<uint8_t,32> ephemeral_public{};            // эпемерный публичный ключ (если присутствует)
  bool has_ephemeral = false;                           // признак наличия эпемерного ключа
  CertificateBundle certificate;                        // цепочка сертификатов
  uint8_t version = VERSION_LEGACY;                     // версия формата кадра
  uint8_t flags = 0;                                    // дополнительные флаги
};

// Специальный корневой ключ AEAD, используемый только для обмена ключами
const std::array<uint8_t,16>& rootKey();

// Формирование нонса для обмена ключами (по упакованным метаданным и msg_id)
std::array<uint8_t,12> makeNonce(uint8_t version,
                                 uint16_t frag_cnt,
                                 uint32_t packed_meta,
                                 uint16_t msg_id);

// Подготовка кадра с публичным ключом: на выходе полный LoRa-кадр
bool buildFrame(uint32_t msg_id,
                const std::array<uint8_t,32>& public_key,
                const std::array<uint8_t,4>& key_id,
                std::vector<uint8_t>& frame_out,
                const std::array<uint8_t,32>* ephemeral_public = nullptr,
                const CertificateBundle* certificate = nullptr);

// Разбор кадра: при успешном декодировании возвращает true и заполняет поля
bool parseFrame(const uint8_t* frame, size_t len,
                FramePayload& payload,
                uint32_t& msg_id_out);

// Совместимость: обёртка вокруг нового парсера без доступа к эпемерному ключу
bool parseFrame(const uint8_t* frame, size_t len,
                std::array<uint8_t,32>& public_key,
                std::array<uint8_t,4>& key_id,
                uint32_t& msg_id_out);

// Настройка доверенного корневого ключа для проверки цепочек
void setTrustedRoot(const std::array<uint8_t,32>& root_public);
bool hasTrustedRoot();
const std::array<uint8_t,32>& getTrustedRoot();

// Проверка цепочки сертификатов для удалённого публичного ключа
bool verifyCertificateChain(const std::array<uint8_t,32>& subject,
                            const CertificateBundle& bundle,
                            std::string* error = nullptr);

// Управление локальной цепочкой сертификатов для исходящих кадров
void setLocalCertificate(const CertificateBundle& bundle);
bool hasLocalCertificate();
const CertificateBundle& getLocalCertificate();

}
