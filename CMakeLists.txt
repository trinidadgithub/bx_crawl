cmake_minimum_required(VERSION 3.10)
project(bx_crawl LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Engine library
add_subdirectory(engine)

# Text frontend
add_subdirectory(frontends/text)

# Tests
enable_testing()
add_subdirectory(tests)

