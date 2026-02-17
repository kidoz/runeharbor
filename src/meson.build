# Utility sources
util_sources = files(
    'util/console_logger.cpp',
    'util/string_utils.cpp',
    'util/fft.cpp',
)

# Platform sources
platform_sources = files(
    'platform/sdl3/sdl_window.cpp',
)

# Engine sources
engine_sources = files(
    'engine/application.cpp',
    'engine/map_scene.cpp',
    'engine/virtual_filesystem.cpp',
)

# Formats sources
formats_sources = files(
    'formats/lod_archive.cpp',
    'formats/image_lod_archive.cpp',
    'formats/sprite_lod_archive.cpp',
    'formats/game_lod_archive.cpp',
    'formats/snd_archive.cpp',
    'formats/sound_list.cpp',
    'formats/pcx_image.cpp',
    'formats/sprite_parser.cpp',
    'formats/blv_map.cpp',
    'formats/odm_map.cpp',
    'formats/face_geometry.cpp',
    'formats/event_file_parser.cpp',
    'formats/global_text_parser.cpp',
    'formats/spells_parser.cpp',
    'formats/items_parser.cpp',
    'formats/monsters_parser.cpp',
    'formats/mapstats_parser.cpp',
    'formats/quests_parser.cpp',
    'formats/classes_parser.cpp',
    'formats/awards_parser.cpp',
    'formats/hostile_parser.cpp',
    'formats/npctext_parser.cpp',
    'formats/npcnamess_parser.cpp',
    'formats/autonote_parser.cpp',
    'formats/credits_parser.cpp',
)

# Graphics sources
graphics_sources = files(
    'graphics/debug_text.cpp',
    'graphics/palette.cpp',
    'graphics/image.cpp',
    'graphics/image_loader.cpp',
    'graphics/sprite_decoder.cpp',
    'graphics/sprite_loader.cpp',
    'graphics/texture_manager.cpp',
    'graphics/sdl_renderer.cpp',
    'graphics/camera.cpp',
    'graphics/line_renderer.cpp',
    'graphics/world_renderer.cpp',
    'graphics/indoor_renderer.cpp',
)

media_sources = files(
    'media/vid_manifest.cpp',
    'media/vid_archive.cpp',
    'media/smacker_decoder.cpp',
    'media/bink_decoder.cpp',
    'media/video_player.cpp',
)

# Game sources
game_sources = files(
    'game/character.cpp',
)

# UI sources
ui_sources = files(
    'ui/layout.cpp',
)

# Dependencies
zlib_dep = dependency('zlib',
    version: '>=1.2.11',
    required: false,
    fallback: ['zlib', 'zlib_dep']
)

if not zlib_dep.found()
    error('zlib is required for LOD archive decompression')
endif

# Main executable
executable(
    'runeharbor',
    sources: [
        'main.cpp',
        util_sources,
        platform_sources,
        engine_sources,
        formats_sources,
        game_sources,
        graphics_sources,
        media_sources,
        ui_sources,
    ],
    include_directories: inc_dirs,
    dependencies: [sdl3_dep, zlib_dep],
    install: true,
)
