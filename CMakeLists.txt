cmake_minimum_required(VERSION 3.16)
project(order_book LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 核心库：订单簿本体，不依赖 main
add_library(ob_core STATIC
    src/order.cpp
    src/order_book.cpp
    src/exchange.cpp
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

# 基准测试（不进 ctest）
add_executable(order_book_bench bench/bench.cpp)
target_link_libraries(order_book_bench PRIVATE ob_core)

# 测试
enable_testing()

# 随机对拍：把当前实现的行为与一份朴素参考实现逐档比对。
# 重构期间它是安全网——输出必须与重构前一致。
add_executable(test_v3 tests/test_v3.cpp)
target_link_libraries(test_v3 PRIVATE ob_core)
add_test(NAME v3_behavior COMMAND test_v3)

add_executable(test_parity tests/test_parity.cpp)
target_link_libraries(test_parity PRIVATE ob_core)
add_test(NAME parity_default COMMAND test_parity 500 20261103)
add_test(NAME parity_seed7   COMMAND test_parity 2000 7)
