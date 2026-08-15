# frontends/text/CMakeLists.txt

add_executable(bx_crawl_text
    src/main.cpp
    src/render_snapshot.cpp
)

target_include_directories(bx_crawl_text PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(bx_crawl_text PRIVATE
    bx_engine
)

set_target_properties(bx_crawl_text PROPERTIES
    CXX_EXTENSIONS OFF
)
