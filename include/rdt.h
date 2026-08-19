#ifndef RDT_H
#define RDT_H

#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>

#pragma comment(lib, "ws2_32.lib")

#define PAYLOAD_SIZE 1024
#define WINDOW_SIZE 64
#define TIMEOUT_MS 500

#pragma pack(push, 1)
struct RDTHeader {
    uint32_t seq_num;
    uint16_t checksum;
    uint8_t  is_ack;
    uint8_t  is_last;
    uint16_t payload_len;
};

struct RDTPacket {
    RDTHeader header;
    char data[PAYLOAD_SIZE];
};
#pragma pack(pop)

uint16_t calculate_checksum(const void* data, size_t len);
bool rdt_send_file(SOCKET sockfd, const char* filename, const char* dest_ip, int dest_port);
bool rdt_receive_file(SOCKET sockfd, const char* save_filename, long long total_file_size = 0);

#endif