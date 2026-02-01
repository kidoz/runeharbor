# Utility sources
util_sources = files(
    'util/console_logger.cpp',
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
    'formats/pcx_image.cpp',
    'formats/blv_map.cpp',
    'formats/odm_map.cpp',
)

# Graphics sources
graphics_sources = files(
    'graphics/debug_text.cpp',
    'graphics/palette.cpp',
    'graphics/image.cpp',
    'graphics/sdl_renderer.cpp',
    'graphics/camera.cpp',
    'graphics/line_renderer.cpp',
)

media_sources = files(
    'media/vid_manifest.cpp',
    'media/vid_archive.cpp',
    'media/smacker_decoder.cpp',
    'media/bink_decoder.cpp',
    'media/video_player.cpp',
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
        graphics_sources,
        media_sources,
    ],
    include_directories: inc_dirs,
    dependencies: [sdl3_dep, zlib_dep],
    install: true,
)
