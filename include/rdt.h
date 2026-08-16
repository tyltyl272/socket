#ifndef RDT_H
#define RDT_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>

#pragma comment(lib, "ws2_32.lib")

const int MAX_PAYLOAD_SIZE = 1024;
const int TIMEOUT_SEC = 3;
const int MAX_RETRIES = 3;

#pragma pack(push, 1)
struct RDTHeader {
    uint32_t seq_num;
    uint16_t payload_len;
    uint8_t  is_ack;
    uint8_t  is_last;
    uint16_t checksum;
};
#pragma pack(pop)
#pragma pack(push, 1)

struct RDTPacket {
    RDTHeader header;
    char data[MAX_PAYLOAD_SIZE];
};
#pragma pack(pop)

uint16_t calculate_checksum(const void* data, size_t length);

bool rdt_send_file(SOCKET socket, const char* filePath, const char* destIP, int destPort);

bool rdt_receive_file(SOCKET socket, const char* savePath);

#endif