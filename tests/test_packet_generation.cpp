#include <cassert>
#include <vector>
#include "message_buffer.h"
#include "libs/packetizer/packet_splitter.h"

// Проверка работы MessageBuffer
void testMessageBuffer() {
    MessageBuffer buf(2);
    uint8_t d1[3] = {1,2,3};
    uint8_t d2[2] = {4,5};
    uint32_t id1 = buf.enqueue(d1, 3);
    uint32_t id2 = buf.enqueue(d2, 2);
    // третье сообщение не должно поместиться
    uint8_t d3[1] = {0};
    uint32_t id3 = buf.enqueue(d3, 1);
    assert(id1 > 0 && id2 > 0 && id3 == 0);
    uint32_t outId; std::vector<uint8_t> out;
    assert(buf.pop(outId, out) && outId == id1 && out.size() == 3);
    assert(buf.pop(outId, out) && outId == id2 && out.size() == 2);
    assert(!buf.pop(outId, out));
}

// Проверка делителя пакетов
void testPacketSplitter() {
    MessageBuffer buf(10);
    PacketSplitter splitter(PayloadMode::SMALL); // 32 байта
    std::vector<uint8_t> data(70, 0xAA); // 70 байт
    uint32_t firstId = splitter.splitAndEnqueue(buf, data.data(), data.size());
    assert(firstId > 0);
    // должно быть 3 пакета: 32 + 32 + 6
    uint32_t id; std::vector<uint8_t> out;
    assert(buf.pop(id, out) && out.size() == 32);
    assert(buf.pop(id, out) && out.size() == 32);
    assert(buf.pop(id, out) && out.size() == 6);
    assert(!buf.pop(id, out));
}

int main() {
    testMessageBuffer();
    testPacketSplitter();
    return 0;
}
