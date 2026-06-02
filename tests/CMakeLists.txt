# tests/CMakeLists.txt

add_executable(bx_crawl_tests
    test_core.cpp
)

target_include_directories(bx_crawl_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

target_link_libraries(bx_crawl_tests PRIVATE
    bx_engine
)

set_target_properties(bx_crawl_tests PROPERTIES
    CXX_EXTENSIONS OFF
)

add_test(NAME bx_crawl_tests COMMAND bx_crawl_tests)
