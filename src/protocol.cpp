#include "protocol.hpp"

// encode header
void OrzTCPHeaderEncode(OrzTCPHeader *header, uint8_t type, uint32_t seq, uint32_t ack, uint16_t len) {
    header->type = type;
    header->seq = seq;
    header->ack = ack;
    header->len = len;
    header->checksum = 0;

    // assume no payload
    OrzTCPSetHeaderChecksum(header);
}

// calc checksum
void OrzTCPSetHeaderChecksum(OrzTCPHeader *header) {
    uint16_t *ptr = (uint16_t *)header;
    uint32_t sum = 0;
    int len = sizeof(OrzTCPHeader) + header->len;
    for (int i = 0; i < len / 2; i++) {
        sum += ptr[i];
    }
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    header->checksum = ~sum;
}

// test checksum
bool checkSum(const OrzTCPHeader *header) {
    uint16_t *ptr = (uint16_t *)header;
    uint32_t sum = 0;
    int len = sizeof(OrzTCPHeader) + header->len;
    for (int i = 0; i < len / 2; i++) {
        sum += ptr[i];
    }
    while (sum >> 16) {
        sum = (sum & 0xffff) + (sum >> 16);
    }
    return sum == 0xffff;
}