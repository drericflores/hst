cmake_minimum_required(VERSION 3.16)
project(HardwareStressTestingTool CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.2 COMPONENTS Widgets REQUIRED)

add_executable(hst
    main.cpp
)

target_link_libraries(hst
    Qt6::Widgets
)

# Faster warnings (optional)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(hst PRIVATE -Wall -Wextra -Wpedantic)
endif()
