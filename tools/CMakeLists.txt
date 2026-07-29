add_executable(gen_pregenerated
    gen_pregenerated.cpp
)

add_executable(list_monsters
    list_monsters.cpp
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

target_include_directories(list_monsters PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(list_monsters PRIVATE
    bx_project_options
)

set_target_properties(list_monsters PROPERTIES
    CXX_EXTENSIONS OFF
)

add_executable(show_monster
    show_monster.cpp
)

target_include_directories(show_monster PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(show_monster PRIVATE
    bx_engine
)

set_target_properties(show_monster PROPERTIES
    CXX_EXTENSIONS OFF
)

add_executable(edit_monster
    edit_monster.cpp
)

target_include_directories(edit_monster PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(edit_monster PRIVATE
    bx_engine
)

set_target_properties(edit_monster PROPERTIES
    CXX_EXTENSIONS OFF
)

add_executable(show_treasure_table
    show_treasure_table.cpp
)

target_include_directories(show_treasure_table PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(show_treasure_table PRIVATE
    bx_project_options
)

set_target_properties(show_treasure_table PROPERTIES
    CXX_EXTENSIONS OFF
)

add_executable(sim_surprise
    sim_surprise.cpp
)

target_include_directories(sim_surprise PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(sim_surprise PRIVATE
    bx_engine
)

set_target_properties(sim_surprise PROPERTIES
    CXX_EXTENSIONS OFF
)

add_executable(show_map_features
    show_map_features.cpp
)

target_include_directories(show_map_features PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(show_map_features PRIVATE
    bx_engine
)

set_target_properties(show_map_features PROPERTIES
    CXX_EXTENSIONS OFF
)
