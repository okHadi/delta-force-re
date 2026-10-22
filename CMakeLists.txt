cmake_minimum_required(VERSION 3.16)
project(delta_force_re LANGUAGES C CXX)

add_library(dfre_core STATIC src/filename.cpp src/network.cpp src/assets.cpp src/models.cpp src/state_loop.cpp src/mission.cpp src/menu.cpp src/terrain.cpp src/profiles.cpp src/soldier_menu.cpp src/scripts.cpp src/items.cpp src/renderer.cpp src/resources.cpp src/campaign.cpp src/player.cpp src/world.cpp src/palette_quantizer.cpp)
target_sources(dfre_core PRIVATE src/model_palette.cpp)
target_sources(dfre_core PRIVATE src/movement.cpp)
target_sources(dfre_core PRIVATE src/audio.cpp src/lobby.cpp)
target_sources(dfre_core PRIVATE src/compression.cpp src/network_send.cpp)
target_sources(dfre_core PRIVATE src/resource_plan.cpp)
set(DFRE_RUNTIME_MODULES
    actor_runtime
    ai
    ai_update
    input_config
    input_state
    lobby_session
    model_bindings
    model_instances
    model_visibility
    motion_step
    motion_queries
    weapons
    item_runtime
    item_callbacks
    world_runtime
    model_characters
    model_specials
    effects
    effect_deferred
    scene_models
    water
    collision
    world_hits
    actor_behavior
    actor_queries
    live_world
    animation
    animation_driver
    mission_animation
    gameplay_dispatch
    motion_clearance
    projectiles
    mission_tick
    mission_geometry
    mission_live
    projectile_hits
    player_step
    actor_collision
    mission_projectile_stage
    player_weapon_runtime
    mission_area_live
    mission_muzzle
    actor_pickups
    mission_player_live
    mission_pickup_live
    mission_projectile_live
    mission_projectile_spawn
    mission_frame_services
    mission_audio_frame
    input_bridge
    mission_script_runtime
    mission_results
    events
    game_messages
    mission_logic
    mission_actor_lifecycle
    mission_host_pump
)
foreach(dfre_module IN LISTS DFRE_RUNTIME_MODULES)
    target_sources(dfre_core PRIVATE src/${dfre_module}.cpp)
    add_executable(${dfre_module}_probe tools/${dfre_module}_probe.cpp)
    target_link_libraries(${dfre_module}_probe PRIVATE dfre_core)
endforeach()
add_executable(mission_profile_probe tools/mission_profile_probe.cpp)
target_link_libraries(mission_profile_probe PRIVATE dfre_core)
target_include_directories(dfre_core PUBLIC include)
target_compile_features(dfre_core PUBLIC cxx_std_17)
set_target_properties(dfre_core PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC AND CMAKE_SIZEOF_VOID_P EQUAL 8)
    enable_language(ASM_MASM)
    target_sources(dfre_core PRIVATE src/palette_variance_x64.asm src/query_angle_x64.asm)
endif()

add_executable(filename_probe tools/filename_probe.cpp)
target_link_libraries(filename_probe PRIVATE dfre_core)

add_executable(network_probe tools/network_probe.cpp)
target_link_libraries(network_probe PRIVATE dfre_core)

add_executable(compression_probe tools/compression_probe.cpp)
target_link_libraries(compression_probe PRIVATE dfre_core)
add_executable(network_send_probe tools/network_send_probe.cpp)
target_link_libraries(network_send_probe PRIVATE dfre_core)

add_executable(assets_probe tools/assets_probe.cpp)
target_link_libraries(assets_probe PRIVATE dfre_core)

add_executable(models_probe tools/models_probe.cpp)
target_link_libraries(models_probe PRIVATE dfre_core)

add_executable(state_loop_probe tools/state_loop_probe.cpp)
target_link_libraries(state_loop_probe PRIVATE dfre_core)

add_executable(mission_probe tools/mission_probe.cpp)
target_link_libraries(mission_probe PRIVATE dfre_core)

add_executable(menu_probe tools/menu_probe.cpp)
target_link_libraries(menu_probe PRIVATE dfre_core)

add_executable(terrain_probe tools/terrain_probe.cpp)
target_link_libraries(terrain_probe PRIVATE dfre_core)

add_executable(profiles_probe tools/profiles_probe.cpp)
target_link_libraries(profiles_probe PRIVATE dfre_core)

add_executable(game_flow_probe tools/game_flow_probe.cpp src/game.cpp)
target_link_libraries(game_flow_probe PRIVATE dfre_core)

add_executable(campaign_probe tools/campaign_probe.cpp)
target_link_libraries(campaign_probe PRIVATE dfre_core)

add_executable(resources_probe tools/resources_probe.cpp)
target_link_libraries(resources_probe PRIVATE dfre_core)
add_executable(resource_plan_probe tools/resource_plan_probe.cpp)
target_link_libraries(resource_plan_probe PRIVATE dfre_core)

add_executable(world_probe tools/world_probe.cpp)
target_link_libraries(world_probe PRIVATE dfre_core)

add_executable(player_probe tools/player_probe.cpp)
target_link_libraries(player_probe PRIVATE dfre_core)

add_executable(renderer_deferred_probe tools/renderer_deferred_probe.cpp)
target_link_libraries(renderer_deferred_probe PRIVATE dfre_core)

add_executable(renderer_probe tools/renderer_probe.cpp)
target_link_libraries(renderer_probe PRIVATE dfre_core)

add_executable(palette_quantizer_probe tools/palette_quantizer_probe.cpp)
target_link_libraries(palette_quantizer_probe PRIVATE dfre_core)

add_executable(model_palette_probe tools/model_palette_probe.cpp)
target_link_libraries(model_palette_probe PRIVATE dfre_core)

add_executable(model_palette_banks_probe tools/model_palette_banks_probe.cpp)
target_link_libraries(model_palette_banks_probe PRIVATE dfre_core)

add_executable(movement_probe tools/movement_probe.cpp)
target_link_libraries(movement_probe PRIVATE dfre_core)

add_executable(audio_probe tools/audio_probe.cpp)
target_link_libraries(audio_probe PRIVATE dfre_core)

add_executable(lobby_probe tools/lobby_probe.cpp)
target_link_libraries(lobby_probe PRIVATE dfre_core)

add_executable(script_probe tools/script_probe.cpp)
target_link_libraries(script_probe PRIVATE dfre_core)

add_executable(items_probe tools/items_probe.cpp)
target_link_libraries(items_probe PRIVATE dfre_core)

option(DFRE_BUILD_WINDOW "Build the game window backend" ${WIN32})
if(WIN32)
    add_executable(reference_capture tools/reference_capture.c)
    target_link_libraries(reference_capture PRIVATE user32)
    set_target_properties(reference_capture PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
endif()
if(DFRE_BUILD_WINDOW)
    if(WIN32)
        add_library(dfre_platform STATIC src/platform_win32.cpp)
        target_link_libraries(dfre_platform PRIVATE user32 gdi32)
    else()
        find_package(SDL2 CONFIG REQUIRED)
        add_library(dfre_platform STATIC src/platform_sdl.cpp)
        target_link_libraries(dfre_platform PRIVATE SDL2::SDL2)
    endif()
    target_include_directories(dfre_platform PUBLIC include)
    target_compile_features(dfre_platform PUBLIC cxx_std_17)
    set_target_properties(dfre_platform PROPERTIES CXX_EXTENSIONS OFF)
    add_executable(window_probe tools/window_probe.cpp)
    target_link_libraries(window_probe PRIVATE dfre_core dfre_platform)
    add_executable(dfre WIN32 src/main.cpp src/game.cpp)
    target_link_libraries(dfre PRIVATE dfre_core dfre_platform)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(native_mission_combat_live tests/native_mission_combat_live.cpp)
    target_link_libraries(native_mission_combat_live PRIVATE dfre_core)
    add_test(NAME native_mission_combat_live COMMAND native_mission_combat_live)
    add_executable(native_mission_messages_live tests/native_mission_messages_live.cpp)
    target_link_libraries(native_mission_messages_live PRIVATE dfre_core)
    add_test(NAME native_mission_messages_live COMMAND native_mission_messages_live)
    foreach(dfre_module IN LISTS DFRE_RUNTIME_MODULES)
        add_executable(native_${dfre_module} tests/native_${dfre_module}.cpp)
        target_link_libraries(native_${dfre_module} PRIVATE dfre_core)
        add_test(NAME native_${dfre_module} COMMAND native_${dfre_module})
    endforeach()
    foreach(dfre_view_test native_world_hits native_mission_geometry native_projectile_hits native_actor_collision native_mission_live native_mission_projectile_stage native_mission_area_live native_mission_player_live native_mission_pickup_live native_mission_projectile_live)
        if(MSVC)
            target_compile_options(${dfre_view_test} PRIVATE /Zc:nrvo-)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${dfre_view_test} PRIVATE -fno-elide-constructors)
        endif()
    endforeach()
    add_executable(native_palette_quantizer tests/native_palette_quantizer.cpp)
    target_link_libraries(native_palette_quantizer PRIVATE dfre_core)
    add_test(NAME native_palette_quantizer COMMAND native_palette_quantizer)
    add_executable(native_model_palette tests/native_model_palette.cpp)
    target_link_libraries(native_model_palette PRIVATE dfre_core)
    add_test(NAME native_model_palette COMMAND native_model_palette)
    add_executable(native_resource_plan tests/native_resource_plan.cpp)
    target_link_libraries(native_resource_plan PRIVATE dfre_core)
    add_test(NAME native_resource_plan COMMAND native_resource_plan)
    add_executable(native_resources tests/native_resources.cpp)
    target_link_libraries(native_resources PRIVATE dfre_core)
    add_test(NAME native_resources COMMAND native_resources)
    add_executable(native_movement tests/native_movement.cpp)
    target_link_libraries(native_movement PRIVATE dfre_core)
    add_test(NAME native_movement COMMAND native_movement)
    add_executable(native_audio tests/native_audio.cpp)
    target_link_libraries(native_audio PRIVATE dfre_core)
    add_test(NAME native_audio COMMAND native_audio)
    add_executable(native_lobby tests/native_lobby.cpp)
    target_link_libraries(native_lobby PRIVATE dfre_core)
    add_test(NAME native_lobby COMMAND native_lobby)
    if(TARGET dfre_platform)
        add_executable(native_platform tests/native_platform.cpp)
        target_link_libraries(native_platform PRIVATE dfre_platform)
        if(NOT WIN32)
            target_link_libraries(native_platform PRIVATE SDL2::SDL2)
        endif()
        add_test(NAME native_platform COMMAND native_platform)
        set_tests_properties(native_platform PROPERTIES TIMEOUT 90)
        if(NOT WIN32)
            set_tests_properties(native_platform PROPERTIES ENVIRONMENT "SDL_VIDEODRIVER=dummy")
        endif()
    endif()
    add_executable(native_filename tests/native_filename.cpp)
    target_link_libraries(native_filename PRIVATE dfre_core)
    add_test(NAME native_filename COMMAND native_filename)
    add_executable(native_network tests/native_network.cpp)
    target_link_libraries(native_network PRIVATE dfre_core)
    add_test(NAME native_network COMMAND native_network)
    add_executable(native_compression tests/native_compression.cpp)
    target_link_libraries(native_compression PRIVATE dfre_core)
    add_test(NAME native_compression COMMAND native_compression)
    add_executable(native_network_send tests/native_network_send.cpp)
    target_link_libraries(native_network_send PRIVATE dfre_core)
    add_test(NAME native_network_send COMMAND native_network_send)
    add_executable(native_assets tests/native_assets.cpp)
    target_link_libraries(native_assets PRIVATE dfre_core)
    add_test(NAME native_assets COMMAND native_assets)
    add_executable(native_models tests/native_models.cpp)
    target_link_libraries(native_models PRIVATE dfre_core)
    add_test(NAME native_models COMMAND native_models)
    add_executable(native_state_loop tests/native_state_loop.cpp)
    target_link_libraries(native_state_loop PRIVATE dfre_core)
    add_test(NAME native_state_loop COMMAND native_state_loop)
    add_executable(native_mission tests/native_mission.cpp)
    target_link_libraries(native_mission PRIVATE dfre_core)
    add_test(NAME native_mission COMMAND native_mission)
    add_executable(native_menu tests/native_menu.cpp)
    target_link_libraries(native_menu PRIVATE dfre_core)
    add_test(NAME native_menu COMMAND native_menu)
    add_executable(native_terrain tests/native_terrain.cpp)
    target_link_libraries(native_terrain PRIVATE dfre_core)
    add_test(NAME native_terrain COMMAND native_terrain)
    add_executable(native_world tests/native_world.cpp)
    target_link_libraries(native_world PRIVATE dfre_core)
    add_test(NAME native_world COMMAND native_world)
    add_executable(native_player tests/native_player.cpp)
    target_link_libraries(native_player PRIVATE dfre_core)
    add_test(NAME native_player COMMAND native_player)
    add_executable(native_campaign tests/native_campaign.cpp)
    target_link_libraries(native_campaign PRIVATE dfre_core)
    add_test(NAME native_campaign COMMAND native_campaign)
    add_executable(native_renderer tests/native_renderer.cpp)
    target_link_libraries(native_renderer PRIVATE dfre_core)
    add_test(NAME native_renderer COMMAND native_renderer)
    add_executable(native_items tests/native_items.cpp)
    target_link_libraries(native_items PRIVATE dfre_core)
    add_test(NAME native_items COMMAND native_items)
    add_executable(native_profiles tests/native_profiles.cpp)
    target_link_libraries(native_profiles PRIVATE dfre_core)
    add_test(NAME native_profiles COMMAND native_profiles)
endif()

if(APPLE)
    find_program(DFRE_CODESIGN codesign)
    if(NOT DFRE_CODESIGN)
        message(FATAL_ERROR "Mac development builds require codesign")
    endif()
    get_property(dfre_targets DIRECTORY PROPERTY BUILDSYSTEM_TARGETS)
    foreach(dfre_target IN LISTS dfre_targets)
        get_target_property(dfre_target_type ${dfre_target} TYPE)
        if(dfre_target_type STREQUAL "EXECUTABLE")
            add_custom_command(TARGET ${dfre_target} POST_BUILD
                COMMAND "${DFRE_CODESIGN}" --force --sign - "$<TARGET_FILE:${dfre_target}>"
                VERBATIM)
        endif()
    endforeach()
endif()
