# engine/CMakeLists.txt

add_library(bx_engine STATIC
    src/campaign_state.cpp
    src/game.cpp
    src/game_session.cpp
    src/map.cpp
    src/module_manifest.cpp
    src/rules_catalog.cpp
    src/description.cpp
    src/chargen.cpp
    src/item_effects.cpp
    src/sound.cpp
    src/debug_log.cpp
)

target_include_directories(bx_engine PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(bx_engine PUBLIC
    bx_project_options
)

set_target_properties(bx_engine PROPERTIES
    CXX_EXTENSIONS OFF
)

if (MSVC)
    target_compile_options(bx_engine PRIVATE /W4)
else()
    target_compile_options(bx_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()
