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
    'engine/virtual_filesystem.cpp',
)

# Formats sources
formats_sources = files(
    'formats/lod_archive.cpp',
    'formats/image_lod_archive.cpp',
)

# Graphics sources
graphics_sources = files(
    'graphics/palette.cpp',
    'graphics/image.cpp',
    'graphics/sdl_renderer.cpp',
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
    ],
    include_directories: inc_dirs,
    dependencies: [sdl3_dep, zlib_dep],
    install: true,
)
