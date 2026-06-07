cmake_minimum_required(VERSION 3.20)
project(bx_crawl VERSION 0.1.0 LANGUAGES CXX)

add_library(bx_project_options INTERFACE)
target_compile_features(bx_project_options INTERFACE cxx_std_20)

# -- Data codegen: generates include/bx_data.hpp from dnd-data/schemas/ + dnd-data/data/ --
set(BX_DATA_HEADER ${CMAKE_SOURCE_DIR}/include/bx_data.hpp)
set(BX_CODEGEN_SCRIPT ${CMAKE_SOURCE_DIR}/scripts/gen_data.py)

add_custom_command(
    OUTPUT ${BX_DATA_HEADER}
    COMMAND python3 ${BX_CODEGEN_SCRIPT}
    DEPENDS
        ${BX_CODEGEN_SCRIPT}
        ${CMAKE_SOURCE_DIR}/dnd-data/schemas/01-ability-scores.json
        ${CMAKE_SOURCE_DIR}/dnd-data/schemas/02-character-classes.json
        ${CMAKE_SOURCE_DIR}/dnd-data/schemas/03-spells.json
        ${CMAKE_SOURCE_DIR}/dnd-data/schemas/04-equipment.json
        ${CMAKE_SOURCE_DIR}/dnd-data/schemas/05-monsters.json
        ${CMAKE_SOURCE_DIR}/dnd-data/schemas/06-attack-tables.json
        ${CMAKE_SOURCE_DIR}/dnd-data/schemas/07-saving-throws.json
        ${CMAKE_SOURCE_DIR}/dnd-data/schemas/08-treasure-types.json
        ${CMAKE_SOURCE_DIR}/dnd-data/data/spells.json
        ${CMAKE_SOURCE_DIR}/dnd-data/data/monsters.json
    COMMENT "Generating bx_data.hpp from B/X rule data"
)
add_custom_target(bx_data DEPENDS ${BX_DATA_HEADER})

# Engine library
add_subdirectory(engine)
add_dependencies(bx_engine bx_data)

# Text frontend
if(UNIX)
    add_subdirectory(frontends/text)
else()
    message(STATUS "Skipping POSIX text frontend on non-UNIX platform")
endif()

# Tests
enable_testing()
add_subdirectory(tests)
