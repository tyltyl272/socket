#ifndef RDT_CLIENT_H
#define RDT_CLIENT_H

#include <winsock2.h>
#include <cstdint>
#include <string>

#pragma pack(push, 1)
struct RDTHeader {
    uint32_t seq_num;     // Số thứ tự gói tin
    uint32_t ack_num;     // Xác nhận gói tin
    uint16_t checksum;    // Mã kiểm tra lỗi
    uint16_t length;      // Kích thước dữ liệu thực tế
    uint8_t flags;        // Cờ điều khiển (0: Dữ liệu, 1: ACK, 2: FIN)
};

struct RDTPacket {
    RDTHeader header;
    char data[1024];
};
#pragma pack(pop)

// Hàm tính checksum phía Client để đối chiếu
uint16_t calculate_checksum(const char* data, int length);

// Hàm chính để Client nhận file qua UDP và kiểm tra tính toàn vẹn
bool rdt_receive_file(SOCKET sockfd, sockaddr_in& server_addr, const std::string& output_filepath);

#endif