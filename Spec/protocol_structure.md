# Packet Sending Protocol — Structure & Explanations

> Scope: MCU (ESP32/Zephyr) ↔ SBC (Radxa) ↔ RF/Satellite modem. High RTT, non‑zero BER environments.

---

## 1. Top‑Level Frame Layout

```
| Preamble | Sync | Ver/Flags | StreamID | MsgID | Seq | Tstamp | PLen | HdrCRC |
| Payload (0..N) |
| (opt) FragHdr |
| (opt) Crypto Tag |
| (opt) FEC Parity |
| FrameCRC |
```

**Byte order:** big‑endian for multi‑byte fields.  
**Alignment:** none; fields are tightly packed.

### 1.1 Header Fields

| Field      | Size | Description |
|------------|------|-------------|
| Preamble   | 4 B  | `0x55 55 55 55` — AGC/clock settle (not part of CRCs). |
| Sync       | 2 B  | `0xAA D5` — frame start marker. |
| Ver/Flags  | 1 B  | Version + feature flags (see below). |
| StreamID   | 1 B  | Logical stream (CTRL/TELEM/DATA…). |
| MsgID      | 2 B  | Message identifier (shared across fragments). |
| Seq        | 2 B  | Per‑link sequence (mod 65536). |
| Tstamp     | 4 B  | Tx time (ms or unix32). Used for RTT/reorder. |
| PLen       | 2 B  | Payload length (bytes), excludes Crypto/FEC. |
| HdrCRC     | 2 B  | CRC‑16/X25 over header from `Sync`..`PLen`. |

**Trailer / Optional:**

| Part         | Size      | Description |
|--------------|-----------|-------------|
| FragHdr      | 4 B (opt) | Present when `FRAG=1`. `FragIdx(2)`, `FragCnt(2)`. |
| Crypto Tag   | 16 B opt  | AES‑GCM authentication tag, when `ENC=1`. |
| FEC Parity   | m B opt   | Reed‑Solomon/LDPC parity bytes. |
| FrameCRC     | 2 B       | CRC‑16/X25 over the whole frame (post‑FEC). |

### 1.2 Ver/Flags (1 byte)

```
bit: 7   6   5   4    3    2     1      0
     V2  V1  V0  ENC  FEC  FRAG  ACKREQ PRIO
```

| Bit | Name   | Meaning |
|-----|--------|---------|
| 7:5 | VER    | Protocol version (e.g., `001b`). |
| 4   | ENC    | AES‑GCM enabled for Payload(+FragHdr). |
| 3   | FEC    | Forward error correction bytes appended. |
| 2   | FRAG   | Payload is fragmented; FragHdr present. |
| 1   | ACKREQ | Request ACK/bitmap for this frame/window. |
| 0   | PRIO   | Elevated priority hint. |

---

## 2. Payload Format (TLV)

```
| Type (1 B) | Len (2 B) | Value (Len B) | ... |
```

| Type | Name       | Notes (encoding) |
|------|------------|------------------|
| 0x01 | CTRL       | Short control cmds (JSON/CBOR). |
| 0x02 | DATA       | Binary data blocks (file/bulk). |
| 0x03 | TELEM      | Telemetry (CBOR/Protobuf). |
| 0x04 | ACK/NACK   | Link‑layer acknowledgments (see §5). |
| 0x05 | KEEPALIVE  | Ping/keepalive without ACKREQ. |
| 0x06 | QOS_HINT   | Measured RTT/PER, rate hints. |
| 0x07 | SIG        | Signatures/security metadata. |

---

## 3. Encryption (ENC=1)

- **Cipher:** AES‑256‑GCM.  
- **AAD:** complete header bytes `Sync..PLen`.  
- **Nonce (12 B):** `Tstamp(4) | MsgID(2) | Seq(2) | Salt(4)`; Salt comes from session key/handshake.  
- **Encrypted area:** `Payload` (+ `FragHdr` if present). Output appends **Crypto Tag (16 B)**.  
- On GCM failure → drop frame silently.

---

## 4. FEC & Interleaving (FEC=1)

- **Preferred:** RS(255, k) on bytes for short/medium frames; LDPC at PHY where supported.  
- **Interleaving:** block interleaver, depth `d=1..8` to smear burst errors.  
- **Placement:** parity appended before `FrameCRC`. Common practice: protect `Payload(+FragHdr)`; keep header parsable.

---

## 5. ARQ/ACK & Windows (Selective Repeat)

- **Tx window:** size `W` (32/64/128).  
- **ACK TLV** (`Type=0x04`):

```
| Type=0x04 | Len | MsgID(2) | BaseSeq(2) | Bitmap(N bytes) |
```

`Bitmap` marks received `Seq` in range `[BaseSeq, BaseSeq + N*8)`.  
Optional **NACK** lists explicit missing `Seq` for fast retransmit.

- **Timers:** `RTO = max(2*RTT_smoothed, 500ms)`, exponential backoff on loss.  
- **Dedup:** by `(MsgID, Seq)`; still update ACK map.

---

## 6. Fragmentation (FRAG=1)

- Split large message (same `MsgID`) into `k` pieces.  
- Each frame carries a fragment + `FragHdr {FragIdx, FragCnt}`.  
- Reassemble on RX when all indices `0..k-1` are present; retransmit missing via ACK/NACK bitmap.

---

## 7. Priorities & QoS Profiles

- `PRIO` + `StreamID` define queue order. Recommended classes:  
  - **P0:** CTRL/ACK/KEEPALIVE — highest.  
  - **P1:** TELEM — small, periodic.  
  - **P2:** DATA — background.  
  - **P3:** BULK — large transfers; shaped by residual rate.

Rate shaping: caps on pkt/s and B/s per class; drain order P0→P3.

---

## 8. Integrity

- **HdrCRC:** CRC‑16/X25 (poly 0x1021, init 0xFFFF, refin/refout, xorout 0xFFFF) on `Sync..PLen`.  
- **FrameCRC:** CRC‑16/X25 over `Sync..(end of data/FEC)`.

---

## 9. Transmit Procedure

1. Build TLV payload (CTRL/DATA/TELEM...).  
2. If oversize → enable `FRAG` and slice; fill `FragHdr`.  
3. Fill header: `Ver/Flags, StreamID, MsgID, Seq, Tstamp, PLen`.  
4. Compute **HdrCRC**.  
5. If `ENC` → AES‑GCM encrypt body; append **Crypto Tag**.  
6. If `FEC` → encode parity; append **FEC Parity**.  
7. Compute **FrameCRC**; enqueue into send window; transmit.  
8. Track timers; on ACK bitmap/NACK retransmit only missing `Seq`.  
9. Slide window; adapt rate by RTT/PER (add `QOS_HINT`).

---

## 10. Receive Procedure

1. Locate `Sync`; verify **HdrCRC**.  
2. If `ENC` → GCM verify/decrypt (AAD = header). Fail → drop.  
3. Optionally correct via FEC; then check **FrameCRC**.  
4. Deduplicate by `(MsgID, Seq)`.  
5. Reassemble fragments by `FragHdr`; on completion, deliver message.  
6. Emit **ACK** bitmap (and NACK if used).  
7. Apply priority rules upward.

---

## 11. Minimal Example (no ENC/FEC)

```
Preamble: 55 55 55 55
Sync:     AA D5
Ver/Flags: 0x02   ; VER=0, FEC=0, ENC=0, FRAG=0, ACKREQ=1, PRIO=0
StreamID:  0x01   ; CTRL
MsgID:     0x10 0x7B
Seq:       0x00 0x2A
Tstamp:    0x00 0x01 0xE2 0x40   ; 123456
PLen:      0x00 0x10
HdrCRC:    0xAB 0xCD

Payload (TLV):
  01 00 0C 7B 22 63 6D 64 22 3A 22 50 49 4E 47 22 7D
  ^  ^^^  ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
  |  Len=12   {"cmd":"PING"} (JSON)
  Type=CTRL

FrameCRC:  0x12 0x34
```

---

## 12. Constants (recommended)

```c
#define SYNC_WORD        0xAAD5
#define PREAMBLE_BYTE    0x55
#define GCM_TAG_SIZE     16
#define CRC16_X25_POLY   0x1021
#define CRC16_X25_INIT   0xFFFF
#define CRC16_X25_XOROUT 0xFFFF

enum TlvType {
  TLV_CTRL = 0x01,
  TLV_DATA = 0x02,
  TLV_TELEM= 0x03,
  TLV_ACK  = 0x04,
  TLV_KEEP = 0x05,
  TLV_QOS  = 0x06,
  TLV_SIG  = 0x07,
};
```

---

## 13. Pseudocode — Sender (Selective Repeat)

```c
build_tlv(payload);
frags = maybe_fragment(payload);

for (f in frags) {
  header = fill_header(flags, stream, msgid, seq, tstamp, f.len);
  hdr_crc = crc16_x25(header.sync_to_plen);
  body = concat(frag_hdr_if_any(f), f.bytes);
  if (ENC) body = aes_gcm_encrypt(body, aad=header.sync_to_plen);
  if (FEC) body = rs_encode(body);
  frame = assemble(sync, header, hdr_crc, body);
  frame_crc = crc16_x25(frame.sync_to_end);
  send(frame);
  window.track(seq).start_timer();
}
```

---

## 14. “Lite” Profile (for quick bring‑up)

- No ENC, no FEC.  
- MTU ≤ 1024 B to avoid fragmentation.  
- ACK bitmap every `N` frames (e.g., 8); cumulative advance.  
- KEEPALIVE every 5–10 s, no ACKREQ.
