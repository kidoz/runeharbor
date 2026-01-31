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
)

# Main executable
executable(
    'runeharbor',
    sources: [
        'main.cpp',
        util_sources,
        platform_sources,
        engine_sources,
    ],
    include_directories: inc_dirs,
    dependencies: sdl3_dep,
    install: true,
)
