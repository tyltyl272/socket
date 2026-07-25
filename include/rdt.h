#ifndef RDT_H
#define RDT_H
#include <cstdint>

#pragma pack(push, 1)
struct PacketHeader {
    uint16_t seq_num;
    uint16_t checksum;
    uint16_t length;
};
#pragma pack(pop)

// Khai báo hàm
uint16_t calculate_checksum(const char* data, int length);
void rdt_send_file();
void rdt_receive_file();

#endif