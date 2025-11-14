cmake_minimum_required(VERSION 3.10)

project(test_etcd)
# 设置编辑器为g++-12
set(CMAKE_CXX_COMPILER "/usr/bin/g++-12")

set(CMAKE_CXX_STANDARD 20)

# 包含头文件目录
include_directories(include)

# 查找公共的源文件（src目录下的文件）
file(GLOB_RECURSE COMMON_SOURCES src/*.cpp src/*.cc)

# 查找依赖库
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(gflags REQUIRED)
# 链接boost.asio
find_package(Boost REQUIRED COMPONENTS system)

# find_package(Protobuf REQUIRED)

# 添加依赖库

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 公共的链接库列表
set(COMMON_LIBS 
    spdlog::spdlog 
    fmt::fmt 
    pthread 
    gflags
    swscale 
    swresample 
    jsoncpp 
    etcd-cpp-api 
    cpprest 
    ssl 
    crypto 
    dl
    Boost::boost
    Boost::system
    protobuf
)

# 创建 registry 可执行程序
add_executable(registry test/registry.cc ${COMMON_SOURCES})
target_link_libraries(registry ${COMMON_LIBS})
set_target_properties(registry PROPERTIES OUTPUT_NAME registry)

# 创建 discovery 可执行程序
add_executable(discovery test/discovery.cc ${COMMON_SOURCES})
target_link_libraries(discovery ${COMMON_LIBS})