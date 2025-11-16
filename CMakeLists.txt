
cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp32_cyd_obd2)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic")

message("[CMake] [INFO] C Compiler: ${CMAKE_C_COMPILER}")
message("[CMake] [INFO] C Flags: ${CMAKE_C_FLAGS}")
message("[CMake] [INFO] C Flags (Debug): ${CMAKE_C_FLAGS_DEBUG}")
message("[CMake] [INFO] C Flags (Release): ${CMAKE_C_FLAGS_RELEASE}")
