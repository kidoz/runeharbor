# Test configuration

# Catch2 dependency with automatic fallback to subproject
# This will:
# 1. Try to find system-installed Catch2
# 2. If not found, automatically download and build from subprojects/catch2.wrap
# 3. This makes the build work on any system without manual installation

# Catch2 builds its own SelfTest suite by default, which does not compile under
# current Clang (TEMPLATE_PRODUCT_TEST_CASE). We only consume the library, so turn
# the subproject's tests and install rules off wherever it may be configured.
catch2_subproject_options = ['tests=false', 'install=false']

catch2_dep = dependency(
    'catch2-with-main',
    version: '>=3.0.0',
    required: false,
    fallback: ['catch2', 'catch2_with_main_dep'],
    default_options: catch2_subproject_options,
)

if not catch2_dep.found()
    # Try without -with-main suffix, still with fallback
    catch2_dep = dependency(
        'catch2',
        version: '>=3.0.0',
        required: false,
        fallback: ['catch2', 'catch2_dep'],
        default_options: catch2_subproject_options,
    )
endif

if not catch2_dep.found()
    # Last resort: explicitly require the subproject
    catch2_proj = subproject('catch2', default_options: catch2_subproject_options)
    catch2_dep = catch2_proj.get_variable('catch2_with_main_dep')
endif

# Test sources (only files that exist on disk)
test_sources = files(
    'test_main.cpp',
    'util/console_logger_test.cpp',
    'util/string_utils_test.cpp',
    'platform/window_test.cpp',
    'formats/lod_archive_test.cpp',
    'formats/image_lod_archive_test.cpp',
    'formats/sprite_lod_archive_test.cpp',
    'formats/frame_tables_test.cpp',
    'formats/tile_table_test.cpp',
    'formats/event_file_parser_test.cpp',
    'formats/global_text_parser_test.cpp',
    'formats/spells_parser_test.cpp',
    'formats/items_parser_test.cpp',
    'formats/monsters_parser_test.cpp',
    'formats/mapstats_parser_test.cpp',
    'formats/quests_parser_test.cpp',
    'formats/classes_parser_test.cpp',
    'formats/awards_parser_test.cpp',
    'formats/hostile_parser_test.cpp',
    'formats/npctext_parser_test.cpp',
    'formats/npcnames_parser_test.cpp',
    'formats/autonote_parser_test.cpp',
    'formats/credits_parser_test.cpp',
    'media/smacker_decoder_test.cpp',
    'media/bink_decoder_test.cpp',
    'media/vid_manifest_test.cpp',
    'engine/application_test.cpp',
    'engine/physics_test.cpp',
    'engine/ini_config_test.cpp',
    'game/character_test.cpp',
    'game/party_test.cpp',
    'game/combat_test.cpp',
    'game/inventory_test.cpp',
    'game/shop_system_test.cpp',
    'game/usable_items_test.cpp',
    'game/quest_log_test.cpp',
    'game/save_game_test.cpp',
    'game/event_engine_test.cpp',
    'graphics/light_stack_test.cpp',
    'graphics/sprite_decoder_test.cpp',
    'graphics/outdoor_renderer_math_test.cpp',
    'graphics/billboard_test.cpp',
    'graphics/clip_utils_test.cpp',
    'graphics/sprite_facing_test.cpp',
    'graphics/texture_coordinates_test.cpp',
    'graphics/hud_test.cpp',
)

# Reuse source files from main build (without main.cpp)
test_util_sources = files(
    '../src/util/console_logger.cpp',
    '../src/util/string_utils.cpp',
    '../src/util/fft.cpp',
)

test_formats_sources = files(
    '../src/formats/lod_archive.cpp',
    '../src/formats/image_lod_archive.cpp',
    '../src/formats/game_lod_archive.cpp',
    '../src/formats/snd_archive.cpp',
    '../src/formats/sound_list.cpp',
    '../src/formats/pcx_image.cpp',
    '../src/formats/sprite_parser.cpp',
    '../src/formats/sprite_lod_archive.cpp',
    '../src/formats/face_geometry.cpp',
    '../src/formats/blv_map.cpp',
    '../src/formats/odm_map.cpp',
    '../src/formats/tile_table.cpp',
    '../src/formats/event_file_parser.cpp',
    '../src/formats/global_text_parser.cpp',
    '../src/formats/spells_parser.cpp',
    '../src/formats/items_parser.cpp',
    '../src/formats/placemon_parser.cpp',
    '../src/formats/rnditems_parser.cpp',
    '../src/formats/monsters_parser.cpp',
    '../src/formats/mapstats_parser.cpp',
    '../src/formats/quests_parser.cpp',
    '../src/formats/classes_parser.cpp',
    '../src/formats/awards_parser.cpp',
    '../src/formats/hostile_parser.cpp',
    '../src/formats/npcdata_parser.cpp',
    '../src/formats/npctopic_parser.cpp',
    '../src/formats/npctext_parser.cpp',
    '../src/formats/npcnames_parser.cpp',
    '../src/formats/autonote_parser.cpp',
    '../src/formats/two_d_events_parser.cpp',
    '../src/formats/merchant_parser.cpp',
    '../src/formats/credits_parser.cpp',
    '../src/formats/frame_tables.cpp',
    '../src/formats/evt_script_parser.cpp',
    '../src/formats/npcprof_parser.cpp',
    '../src/formats/npcgreet_parser.cpp',
)

test_graphics_sources = files(
    '../src/graphics/bitmap_font.cpp',
    '../src/graphics/camera.cpp',
    '../src/graphics/debug_text.cpp',
    '../src/graphics/image.cpp',
    '../src/graphics/image_loader.cpp',
    '../src/graphics/line_renderer.cpp',
    '../src/graphics/palette.cpp',
    '../src/graphics/sdl_renderer.cpp',
    '../src/graphics/sprite_decoder.cpp',
    '../src/graphics/sprite_loader.cpp',
    '../src/graphics/texture_manager.cpp',
    '../src/graphics/world_renderer.cpp',
    '../src/graphics/indoor_renderer.cpp',
    '../src/graphics/outdoor_renderer.cpp',
    '../src/graphics/visibility.cpp',
)

test_media_sources = files(
    '../src/media/vid_manifest.cpp',
    '../src/media/video_player.cpp',
    '../src/media/vid_archive.cpp',
    '../src/media/bink_decoder.cpp',
    '../src/media/smacker_decoder.cpp',
)

test_audio_sources = files(
    '../src/audio/audio_system.cpp',
)

test_game_sources = files(
    '../src/game/character.cpp',
    '../src/game/building_type.cpp',
    '../src/game/party.cpp',
    '../src/game/game_world.cpp',
    '../src/game/generation.cpp',
    '../src/game/event_engine.cpp',
    '../src/game/inventory.cpp',
    '../src/game/combat.cpp',
    '../src/game/spells.cpp',
    '../src/game/quest_log.cpp',
    '../src/game/save_game.cpp',
    '../src/game/shop_system.cpp',
    '../src/game/travel_destinations.cpp',
)

test_ui_sources = files(
    '../src/ui/layout.cpp',
    '../src/ui/widget.cpp',
    '../src/ui/widgets.cpp',
    '../src/ui/ui_manager.cpp',
    '../src/ui/hud.cpp',
    '../src/ui/dialogue.cpp',
    '../src/ui/inventory_widget.cpp',
    '../src/ui/character_stats_widget.cpp',
    '../src/ui/spellbook_widget.cpp',
    '../src/ui/rest_widget.cpp',
    '../src/ui/map_widget.cpp',
    '../src/ui/shop_window.cpp',
    '../src/ui/journal_widget.cpp',
)

test_engine_sources = files(
    '../src/engine/application.cpp',
    '../src/engine/ini_config.cpp',
    '../src/engine/map_transition_resolver.cpp',
    '../src/engine/physics.cpp',
    '../src/engine/virtual_filesystem.cpp',
    '../src/engine/map_scene.cpp',
    '../src/engine/states/state_context.cpp',
    '../src/engine/states/intro_state.cpp',
    '../src/engine/states/title_state.cpp',
    '../src/engine/states/character_creation_state.cpp',
    '../src/engine/states/loading_state.cpp',
    '../src/engine/states/ingame_state.cpp',
    '../src/engine/states/credits_state.cpp',
    '../src/engine/states/load_game_state.cpp',
)

# Note: We don't include sdl_window.cpp in tests because it requires SDL
# We use mocks instead for testing

# Get zlib dependency for LOD tests
zlib_dep = dependency('zlib',
    version: '>=1.2.11',
    required: false,
    fallback: ['zlib', 'zlib_dep']
)

if not zlib_dep.found()
    error('zlib is required for LOD archive tests')
endif

# Test executable
test_exe = executable(
    'runeharbor_tests',
    sources: [
        test_sources,
        shaders_header,
        test_util_sources,
        test_formats_sources,
        test_graphics_sources,
        test_media_sources,
        test_audio_sources,
        test_game_sources,
        test_ui_sources,
        test_engine_sources,
    ],
    include_directories: inc_dirs,
    dependencies: [catch2_dep, sdl3_dep, zlib_dep, sdl_shadercross_dep],
    build_by_default: true,
)

# Register tests with Meson
test('unit tests', test_exe)

# Informational messages
message('Tests enabled - run with: meson test or just test')
if catch2_dep.type_name() == 'internal'
    message('Using Catch2 from subproject (auto-downloaded via Meson wrap)')
else
    message('Using system-installed Catch2')
endif
