#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP
#pragma pack(1)
#include <cstdint>
#include <iostream>

//  0                   1                   2                   3  
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |    Type (8)   |                 Reserved (24)                 |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                     Sequence Number (32)                      |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                  Acknowledgment Number (32)                   |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |            Length (16)        |           Checksum (16)       |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |                            Payload ...                        |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// OrzTCP Header
// Type: 8 bits
//   0   1   2   3   4   5   6   7
// +---+---+---+---+---+---+---+---+
// |SYN|ACK|FIN|   |    Reserved   |
// +---+---+---+---+---+---+---+---+
// Sequence Number: 32 bits
// Acknowledgment Number: 32 bits
// Length: 16 bits (excluding header)
// Checksum: 16 bits
// Payload: variable length

typedef struct {
    uint8_t type;
    uint8_t reserved[3];
    uint32_t seq;
    uint32_t ack;
    uint16_t len;
    uint16_t checksum;
} OrzTCPHeader;

typedef struct {
    OrzTCPHeader header;
    char payload[0];
} OrzTCPPacket;

const uint8_t TYPE_SYN = 1 << 7;
const uint8_t TYPE_ACK = 1 << 6;
const uint8_t TYPE_FIN = 1 << 5;
const uint8_t TYPE_DATA = 0;

// encode header
void OrzTCPHeaderEncode(OrzTCPHeader *header, uint8_t type, uint32_t seq, uint32_t ack, uint16_t len);
// calc checksum
void OrzTCPSetHeaderChecksum(OrzTCPHeader *header);
// test checksum
bool checkSum(const OrzTCPHeader *header);
#pragma pack()
#endif