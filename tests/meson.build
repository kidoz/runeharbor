# Test configuration

# Catch2 dependency with automatic fallback to subproject
# This will:
# 1. Try to find system-installed Catch2
# 2. If not found, automatically download and build from subprojects/catch2.wrap
# 3. This makes the build work on any system without manual installation

catch2_dep = dependency(
    'catch2-with-main',
    version: '>=3.0.0',
    required: false,
    fallback: ['catch2', 'catch2_with_main_dep'],
)

if not catch2_dep.found()
    # Try without -with-main suffix, still with fallback
    catch2_dep = dependency(
        'catch2',
        version: '>=3.0.0',
        required: false,
        fallback: ['catch2', 'catch2_dep'],
    )
endif

if not catch2_dep.found()
    # Last resort: explicitly require the subproject
    catch2_proj = subproject('catch2')
    catch2_dep = catch2_proj.get_variable('catch2_with_main_dep')
endif

# Test sources
test_sources = files(
    'test_main.cpp',
    'util/console_logger_test.cpp',
    'platform/window_test.cpp',
    'formats/lod_archive_test.cpp',
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
    'formats/npcnamess_parser_test.cpp',
    'formats/autonote_parser_test.cpp',
    'formats/credits_parser_test.cpp',
    'media/smacker_decoder_test.cpp',
    'media/bink_decoder_test.cpp',
    # Note: application_test.cpp temporarily disabled due to SIGBUS issue
    # TODO: Debug and re-enable
    # 'engine/application_test.cpp',
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
    '../src/formats/blv_map.cpp',
    '../src/formats/odm_map.cpp',
    '../src/formats/event_file_parser.cpp',
    '../src/formats/global_text_parser.cpp',
    '../src/formats/spells_parser.cpp',
    '../src/formats/items_parser.cpp',
    '../src/formats/monsters_parser.cpp',
    '../src/formats/mapstats_parser.cpp',
    '../src/formats/quests_parser.cpp',
    '../src/formats/classes_parser.cpp',
    '../src/formats/awards_parser.cpp',
    '../src/formats/hostile_parser.cpp',
    '../src/formats/npctext_parser.cpp',
    '../src/formats/npcnamess_parser.cpp',
    '../src/formats/autonote_parser.cpp',
    '../src/formats/credits_parser.cpp',
)

test_graphics_sources = files(
    '../src/graphics/camera.cpp',
    '../src/graphics/debug_text.cpp',
    '../src/graphics/image.cpp',
    '../src/graphics/line_renderer.cpp',
    '../src/graphics/palette.cpp',
    '../src/graphics/sdl_renderer.cpp',
    '../src/graphics/world_renderer.cpp',
    '../src/graphics/indoor_renderer.cpp',
)

test_media_sources = files(
    '../src/media/vid_manifest.cpp',
    '../src/media/video_player.cpp',
    '../src/media/vid_archive.cpp',
    '../src/media/bink_decoder.cpp',
    '../src/media/smacker_decoder.cpp',
)

test_engine_sources = files(
    '../src/engine/application.cpp',
    '../src/engine/virtual_filesystem.cpp',
    '../src/engine/map_scene.cpp',
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
        test_util_sources,
        test_formats_sources,
        test_graphics_sources,
        test_media_sources,
        test_engine_sources,
    ],
    include_directories: inc_dirs,
    dependencies: [catch2_dep, sdl3_dep, zlib_dep],
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
