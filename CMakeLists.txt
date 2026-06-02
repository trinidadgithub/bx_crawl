cmake_minimum_required(VERSION 3.20)
project(bx_crawl VERSION 0.1.0 LANGUAGES CXX)

add_library(bx_project_options INTERFACE)
target_compile_features(bx_project_options INTERFACE cxx_std_20)

# Engine library
add_subdirectory(engine)

# Text frontend
if(UNIX)
    add_subdirectory(frontends/text)
else()
    message(STATUS "Skipping POSIX text frontend on non-UNIX platform")
endif()

# Tests
enable_testing()
add_subdirectory(tests)
