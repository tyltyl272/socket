#include "rdt.h"
#include <iostream>

// Giải thuật tính 16-bit Internet Checksum chuẩn RFC 1071
uint16_t calculate_checksum(const void* data, size_t length) {
    const uint16_t* ptr = static_cast<const uint16_t*>(data);
    uint32_t sum = 0;

    while (length > 1) {
        sum += *ptr++;
        length -= 2;
    }

    // Nếu độ dài lẻ, cộng byte cuối cùng
    if (length > 0) {
        sum += *reinterpret_cast<const uint8_t*>(ptr);
    }

    // Fold 32-bit sum sang 16-bit
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }

    return static_cast<uint16_t>(~sum);
}