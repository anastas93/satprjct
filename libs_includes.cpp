// Этот файл подключает реализации библиотек, чтобы Arduino-сборка видела их определения
#include "libs/packetizer/packet_splitter.cpp"
#include "libs/packetizer/packet_gatherer.cpp"     // собиратель пакетов
#include "libs/frame/frame_header.cpp"
#include "libs/text_converter/text_converter.cpp"
#include "libs/rs/rs.cpp"            // базовая реализация RS(255,223)
#include "libs/rs255223/rs255223.cpp" // обёртки encode/decode
#include "libs/byte_interleaver/byte_interleaver.cpp" // байтовый интерливинг
#include "libs/viterbi/viterbi.cpp"                  // сверточный кодер
#include "libs/conv_codec/conv_codec.cpp"           // обёртки encodeBits/viterbiDecode
#include "libs/bit_interleaver/bit_interleaver.cpp" // битовый интерливинг
#include "libs/scrambler/scrambler.cpp"             // скремблер
#include "libs/simple_logger/simple_logger.cpp"     // журнал статусов
#include "libs/received_buffer/received_buffer.cpp" // буфер принятых сообщений
#include "libs/key_loader/key_loader.cpp"           // загрузка и сохранение ключа
#include "libs/key_transfer/key_transfer.cpp"       // передача корневого ключа по LoRa
#include "libs/crypto/ed25519.cpp"            // проверка подписей Ed25519 через libsodium
#include "libs/crypto/aes_ccm.cpp"            // AES-CCM шифрование
#include "libs/crypto/hkdf.cpp"               // HKDF-SHA256 для вывода ключевого материала
#include "libs/crypto/sha256.cpp"             // SHA-256 для ключевого хранилища
#include "libs/crypto/curve25519_donna.cpp"   // низкоуровневая математика Curve25519
#include "libs/crypto/x25519.cpp"             // ECDH на Curve25519
#include "libs/mbedtls/aes.c"                 // упрощённый AES
#include "libs/mbedtls/ccm.c"                 // реализация CCM
