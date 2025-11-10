cmake_minimum_required(VERSION 3.29)
project(CoroRpc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

cmake_minimum_required(VERSION 3.29)
project(CoroRpc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找所有需要的包
find_package(Boost REQUIRED COMPONENTS system)
find_package(Protobuf REQUIRED)
find_package(jsoncpp REQUIRED)

# 添加可执行文件
add_executable(CoroRpc main.cpp)

# 包含目录
target_include_directories(CoroRpc PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# 使用导入的目标链接库
target_link_libraries(CoroRpc PRIVATE
        Boost::boost
        Boost::system
        JsonCpp::JsonCpp  # 或者可能是 JsonCpp::JsonCpp
        protobuf::libprotobuf
        ws2_32
        mswsock
)