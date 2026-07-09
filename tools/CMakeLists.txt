add_executable(gen_pregenerated
    gen_pregenerated.cpp
)

target_include_directories(gen_pregenerated PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(gen_pregenerated PRIVATE
    bx_engine
)

set_target_properties(gen_pregenerated PROPERTIES
    CXX_EXTENSIONS OFF
)
