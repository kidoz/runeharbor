project(
    'runeharbor',
    'cpp',
    version: '0.1.0',
    license: 'MIT',
    default_options: [
        'cpp_std=c++23',
        'warning_level=3',
        'buildtype=debug',
    ],
)

# Dependencies
cmake = import('cmake')

sdl3_dep = dependency('sdl3', required: false)
if not sdl3_dep.found()
    sdl3_dep = dependency('SDL3', required: false)
endif
if not sdl3_dep.found()
    sdl3_opts = cmake.subproject_options()
    sdl3_opts.add_cmake_defines({
        'SDL_SHARED': 'OFF',
        'SDL_STATIC': 'ON',
        'SDL_TEST_LIBRARY': 'OFF',
        'SDL_TESTS': 'OFF',
        'SDL_EXAMPLES': 'OFF',
    })
    sdl3_subproj = cmake.subproject('sdl3', options: sdl3_opts)
    sdl3_dep = sdl3_subproj.dependency('SDL3-static')
endif
sdl_shadercross_opts = cmake.subproject_options()
sdl_shadercross_opts.add_cmake_defines({'SDLSHADERCROSS_DXC': 'OFF', 'SDLSHADERCROSS_VENDORED': 'ON', 'SDLSHADERCROSS_CLI': 'OFF', 'SDLSHADERCROSS_SHARED': 'OFF', 'SDLSHADERCROSS_STATIC': 'ON'})
sdl_shadercross_subproj = cmake.subproject('SDL_shadercross', options: sdl_shadercross_opts)
sdl_shadercross_dep = sdl_shadercross_subproj.dependency('SDL3_shadercross-static')

# Include directories
inc_dirs = include_directories('src')

# Subdirectories
subdir('src')
subdir('tests')
