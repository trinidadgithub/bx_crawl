# engine/CMakeLists.txt

add_library(bx_engine STATIC
    src/game.cpp
    src/map.cpp
    src/description.cpp
    src/chargen.cpp
)

target_include_directories(bx_engine PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

if (MSVC)
    target_compile_options(bx_engine PRIVATE /W4)
else()
    target_compile_options(bx_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

