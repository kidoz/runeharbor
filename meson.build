project(
    'runeharbor',
    'cpp',
    version: '0.1.0',
    license: 'MIT',
    default_options: [
        'cpp_std=c++23',
        'warning_level=3',
        'werror=true',
        'buildtype=debug',
    ],
)

# Dependencies
sdl3_dep = dependency('sdl3', required: false)
if not sdl3_dep.found()
    # Fallback to pkg-config with alternative names
    sdl3_dep = dependency('SDL3', required: false)
endif
if not sdl3_dep.found()
    error('SDL3 not found. Please install SDL3 development libraries.')
endif

# Include directories
inc_dirs = include_directories('src')

# Subdirectories
subdir('src')
