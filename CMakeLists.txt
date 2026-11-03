cmake_minimum_required(VERSION 3.16)
project(order_book LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 核心库：订单簿本体，不依赖 main
add_library(ob_core STATIC
    src/order.cpp
    src/order_book.cpp
)
target_include_directories(ob_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# 演示程序
add_executable(order_book src/main.cpp)
target_link_libraries(order_book PRIVATE ob_core)

# 打开警告（MSVC / GCC 都对）
if (MSVC)
    target_compile_options(ob_core PRIVATE /W4)
    target_compile_options(order_book PRIVATE /W4)
else()
    target_compile_options(ob_core PRIVATE -Wall -Wextra)
    target_compile_options(order_book PRIVATE -Wall -Wextra)
endif()

# 测试（v4 接 ctest）
enable_testing()
