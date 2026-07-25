#ifndef RDT_SERVER_H
#define RDT_SERVER_H

#include <winsock2.h>
#include <cstdint>
#include <string>

#pragma pack(push, 1)
struct RDTHeader {
    uint32_t seq_num;     // Số thứ tự gói tin
    uint32_t ack_num;     // Xác nhận gói tin
    uint16_t checksum;    // Mã kiểm tra lỗi
    uint16_t length;      // Kích thước dữ liệu thực tế trong payload
    uint8_t flags;        // Cờ điều khiển (SYN, ACK, FIN)
};

struct RDTPacket {
    RDTHeader header;
    char data[1024];      // Payload tối đa 1KB mỗi gói
};
#pragma pack(pop)

// Hàm tính checksum đơn giản
uint16_t calculate_checksum(const char* data, int length);

// Hàm chính để Server truyền file qua UDP với cơ chế Go-Back-N
bool rdt_send_file(SOCKET sockfd, sockaddr_in& client_addr, const std::string& filepath);

#endif#pragma once
