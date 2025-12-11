cmake_minimum_required(VERSION 3.10)
project(bx_crawl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(bx_crawl
    src/main.cpp
    src/game.cpp
    src/map.cpp
    src/description.cpp
    src/chargen.cpp
)

target_include_directories(bx_crawl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_options(bx_crawl PRIVATE /W4)
else()
    target_compile_options(bx_crawl PRIVATE -Wall -Wextra -Wpedantic)
endif()

# --- Tests ---
enable_testing()
add_subdirectory(tests)
