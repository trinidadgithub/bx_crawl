# tests/CMakeLists.txt

add_executable(bx_crawl_tests
    test_core.cpp
    ${CMAKE_SOURCE_DIR}/src/map.cpp
    ${CMAKE_SOURCE_DIR}/src/game.cpp
    ${CMAKE_SOURCE_DIR}/src/description.cpp
    ${CMAKE_SOURCE_DIR}/src/chargen.cpp
)

target_include_directories(bx_crawl_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

add_test(NAME bx_crawl_tests COMMAND bx_crawl_tests)

