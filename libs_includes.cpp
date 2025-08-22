// Этот файл подключает реализации библиотек, чтобы Arduino-сборка видела их определения
#include "libs/packetizer/packet_splitter.cpp"
#include "libs/frame/frame_header.cpp"
#include "libs/text_converter/text_converter.cpp"
#include "libs/rs/rs.cpp"            // базовая реализация RS(255,223)
#include "libs/rs255223/rs255223.cpp" // обёртки encode/decode
#include "libs/byte_interleaver/byte_interleaver.cpp" // байтовый интерливинг
