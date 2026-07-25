cmake_minimum_required(VERSION 3.8)
project(Hybrid_FTP_Project)

# Chỉ định thư mục chứa file header
include_directories(include)

# Tạo thư viện dùng chung cho các file UDP RDT
add_library(rdt_common src/common/rdt_sender.cpp src/common/rdt_receiver.cpp src/common/utils.cpp)