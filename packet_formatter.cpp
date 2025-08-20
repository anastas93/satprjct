#include "packet_formatter.h"
#include "config.h"
#include "libs/ccsds_link/ccsds_link.h"
#include <string.h>

PacketFormatter::PacketFormatter(Fragmenter& frag, IEncryptor& enc, PipelineMetrics& m)
  : frag_(frag), enc_(enc), metrics_(m) {}

// Применяет шифрование AES-CCM к полезной нагрузке
bool PacketFormatter::applyEncryption(const uint8_t* in, size_t len, const uint8_t* aad, size_t aad_len, std::vector<uint8_t>& out) {
  if (!enc_enabled_ || !enc_.isReady()) {
    out.assign(in, in + len);
    return true;
  }
  return enc_.encrypt(in, len, aad, aad_len, out);
}

// Подготовка кадра: фрагментация, шифрование, скремблирование
size_t PacketFormatter::prepareFrames(const OutgoingMessage& msg, bool requireAck, std::vector<PreparedFrame>& out) {
  out.clear();
  bool willEnc = enc_enabled_ && enc_.isReady();
  const uint16_t base_payload_max =
      (payload_len_ < (cfg::LORA_MTU - FRAME_HEADER_SIZE)) ? payload_len_ : (cfg::LORA_MTU - FRAME_HEADER_SIZE);
  const uint16_t enc_overhead = willEnc ? (1 + cfg::ENC_TAG_LEN) : 0;
  const uint16_t eff_payload_max = (base_payload_max > enc_overhead) ? (uint16_t)(base_payload_max - enc_overhead) : 0;
  auto parts = frag_.split(msg.id, msg.data.data(), msg.data.size(), requireAck, eff_payload_max);

  std::array<uint8_t, FRAME_HEADER_SIZE> aad{};
  std::vector<uint8_t> payload; payload.reserve(cfg::LORA_MTU);
  std::vector<uint8_t> frame; frame.reserve(cfg::LORA_MTU);

  for (auto& fr : parts) {
    FrameHeader hdr = fr.hdr;
    hdr.hdr_crc = 0; hdr.frame_crc = 0;
    hdr.encode(aad.data(), aad.size());

    payload.clear();
    if (!applyEncryption(fr.payload.data(), fr.payload.size(), aad.data(), aad.size(), payload)) {
      metrics_.enc_fail++;
      continue; // переходим к следующему фрагменту
    }

    ccsds::Params cp;
    cp.fec = fec_enabled_ ? (ccsds::FecMode)fec_mode_ : ccsds::FEC_OFF;
    cp.interleave = interleave_depth_;
    cp.scramble = true;
    std::vector<uint8_t> ccsds_buf;
    ccsds::encode(payload.data(), payload.size(), hdr.msg_id, cp, ccsds_buf);
    payload.swap(ccsds_buf);

    if (pilot_interval_bytes_ > 0 && !payload.empty()) {
      size_t pos = pilot_interval_bytes_;
      while (pos < payload.size()) {
        payload.insert(payload.begin() + pos, cfg::PILOT_SEQ, cfg::PILOT_SEQ + cfg::PILOT_LEN);
        pos += pilot_interval_bytes_ + cfg::PILOT_LEN;
      }
    }

    FrameHeader final_hdr = fr.hdr;
    if (willEnc) final_hdr.flags |= F_ENC;
    final_hdr.payload_len = (uint16_t)payload.size();
    final_hdr.hdr_crc = 0; final_hdr.frame_crc = 0;
    uint8_t hdr_buf[FRAME_HEADER_SIZE];
    final_hdr.encode(hdr_buf, FRAME_HEADER_SIZE);
    uint16_t hcrc = crc16_ccitt(hdr_buf, FRAME_HEADER_SIZE - 4, 0xFFFF);
    final_hdr.hdr_crc = hcrc;
    final_hdr.encode(hdr_buf, FRAME_HEADER_SIZE);
    uint16_t fcrc = crc16_ccitt(hdr_buf, FRAME_HEADER_SIZE, 0xFFFF);
    fcrc = crc16_ccitt(payload.data(), payload.size(), fcrc);
    final_hdr.frame_crc = fcrc;
    final_hdr.encode(hdr_buf, FRAME_HEADER_SIZE);

    size_t frame_len = FRAME_HEADER_SIZE + payload.size();
    if (hdr_dup_enabled_) frame_len += FRAME_HEADER_SIZE;
    frame.resize(frame_len);
    memcpy(frame.data(), hdr_buf, FRAME_HEADER_SIZE);
    size_t off = FRAME_HEADER_SIZE;
    if (hdr_dup_enabled_) {
      memcpy(frame.data()+off, hdr_buf, FRAME_HEADER_SIZE);
      off += FRAME_HEADER_SIZE;
    }
    memcpy(frame.data()+off, payload.data(), payload.size());

    for (uint8_t r = 0; r < repeat_count_; ++r) {
      PreparedFrame pf{final_hdr, frame};
      out.push_back(std::move(pf));
    }
  }
  return out.size();
}
