cmake_minimum_required(VERSION 3.10)
project(bx_crawl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(bx_crawl
    src/main.cpp
)

# If you want warnings cranked up:
if (MSVC)
    target_compile_options(bx_crawl PRIVATE /W4)
else()
    target_compile_options(bx_crawl PRIVATE -Wall -Wextra -Wpedantic)
endif()

