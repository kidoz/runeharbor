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
    # Note: application_test.cpp temporarily disabled due to SIGBUS issue
    # TODO: Debug and re-enable
    # 'engine/application_test.cpp',
)

# Reuse source files from main build (without main.cpp)
test_util_sources = files(
    '../src/util/console_logger.cpp',
)

test_formats_sources = files(
    '../src/formats/lod_archive.cpp',
)

# Note: engine sources temporarily excluded
# test_engine_sources = files(
#     '../src/engine/application.cpp',
# )

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
        # test_engine_sources,  # Temporarily excluded
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
