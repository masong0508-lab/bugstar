#include "session.h"

static int bsnet_len(uint8_t type) {
    switch (type) {
    case BSNET_HELLO: return 5;
    case BSNET_READY: return 1;
    case BSNET_INPUT: return 3;
    case BSNET_STATE: return 8;
    case BSNET_EVENT: return 4;
    case BSNET_ACK: return 0;
    case BSNET_PING: return 2;
    case BSNET_BYE: return 0;
    case BSNET_RAIDER: return 12;
    default: return -1;
    }
}
/* CRC-8 / ATM: initial 0, poly 0x07, no reflection or xor-out. */
static uint8_t bsnet_crc8(const uint8_t *buf, uint8_t n) {
    uint8_t crc = 0;
    uint8_t i;
    while (n--) {
        crc ^= *buf++;
        for (i = 0; i < 8; i++)
            if (crc & 0x80u) crc = (uint8_t)((crc << 1) ^ 0x07u);
            else crc = (uint8_t)(crc << 1);
    }
    return crc;
}
int bsnet_pack(const BsNetPacket *pkt, uint8_t *out, uint8_t capacity) {
    int want;
    uint8_t i, count;
    if (!pkt || !out) return BSNET_E_ARGS;
    want = bsnet_len(pkt->type);
    if (want < 0) return BSNET_E_TYPE;
    if (pkt->length != (uint8_t)want || pkt->length > BSNET_MAX_PAYLOAD)
        return BSNET_E_LENGTH;
    count = (uint8_t)(BSNET_HEADER_BYTES + pkt->length + 1u);
    if (capacity < count) return BSNET_E_LENGTH;
    out[0] = BSNET_MAGIC;
    out[1] = BSNET_VERSION;
    out[2] = pkt->type;
    out[3] = pkt->sequence;
    out[4] = (uint8_t)(pkt->tick & 255u);
    out[5] = (uint8_t)(pkt->tick >> 8);
    out[6] = pkt->length;
    for (i = 0; i < pkt->length; i++) out[7 + i] = pkt->payload[i];
    out[count - 1] = bsnet_crc8(out, (uint8_t)(count - 1));
    return count;
}
int bsnet_unpack(BsNetPacket *out, const uint8_t *data, uint8_t count) {
    BsNetPacket next;
    uint8_t i;
    int want;
    if (!out || !data) return BSNET_E_ARGS;
    if (count < BSNET_HEADER_BYTES + 1u || count > BSNET_MAX_FRAME)
        return BSNET_E_LENGTH;
    if (data[0] != BSNET_MAGIC) return BSNET_E_HEADER;
    if (data[1] != BSNET_VERSION) return BSNET_E_VERSION;
    want = bsnet_len(data[2]);
    if (want < 0) return BSNET_E_TYPE;
    if (data[6] != (uint8_t)want || count != BSNET_HEADER_BYTES + 1u + data[6])
        return BSNET_E_LENGTH;
    if (bsnet_crc8(data, (uint8_t)(count - 1)) != data[count - 1])
        return BSNET_E_CRC;
    next.type = data[2];
    next.sequence = data[3];
    next.tick = (uint16_t)((uint16_t)data[4] | ((uint16_t)data[5] << 8));
    next.length = data[6];
    for (i = 0; i < next.length; i++) next.payload[i] = data[7 + i];
    for (; i < BSNET_MAX_PAYLOAD; i++) next.payload[i] = 0;
    *out = next; /* single successful commit: invalid data cannot poison state */
    return 0;
}
