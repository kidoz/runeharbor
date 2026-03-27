# RuneHarbor Engine - Build automation

# Build directory (change this to customize build location)
build_dir := "build"

# Default recipe - show available commands
default:
    @just --list

# Setup debug build
setup:
    meson setup {{build_dir}} --buildtype=debug

# Setup release build
setup-release:
    meson setup {{build_dir}} --buildtype=release

# Setup with address sanitizer
setup-asan:
    meson setup {{build_dir}} -Db_sanitize=address,undefined --buildtype=debug

# Build the project
build:
    ninja -C {{build_dir}} src/runeharbor tests/runeharbor_tests

# Run the engine
run: build
    ./{{build_dir}}/src/runeharbor

# Run with game data path
run-with-data DATA_PATH: build
    ./{{build_dir}}/src/runeharbor --data "{{DATA_PATH}}"

# Run with game data path and map
run-map DATA_PATH MAP_NAME: build
    ./{{build_dir}}/src/runeharbor --data "{{DATA_PATH}}" --map "{{MAP_NAME}}"

# ============================================================================
# LOD Archive Tools
# ============================================================================

# List files in a LOD archive
lod-list ARCHIVE: build
    ./{{build_dir}}/tools/lod-extract list {{ARCHIVE}}

# Extract a single file from LOD archive
lod-extract ARCHIVE FILENAME OUTPUT="": build
    ./{{build_dir}}/tools/lod-extract extract {{ARCHIVE}} {{FILENAME}} {{OUTPUT}}

# Extract all files from LOD archive
lod-extract-all ARCHIVE OUTPUT_DIR: build
    ./{{build_dir}}/tools/lod-extract extract-all {{ARCHIVE}} {{OUTPUT_DIR}}

# ============================================================================
# Testing
# ============================================================================

# Run all unit tests
test: build
    meson test -C {{build_dir}} --print-errorlogs

# Run tests with verbose output
test-verbose: build
    meson test -C {{build_dir}} --verbose

# Run specific test by name
test-filter PATTERN: build
    ./{{build_dir}}/tests/runeharbor_tests "{{PATTERN}},[{{PATTERN}}],*{{PATTERN}}*"

# Run tests and show coverage (requires gcov setup)
test-coverage: build
    meson test -C {{build_dir}}
    ninja -C {{build_dir}} coverage

# ============================================================================
# Static Analysis
# ============================================================================

# Run clang static analyzer (scan-build)
analyze:
    @echo "Running Clang Static Analyzer..."
    @analyzer="$(command -v scan-build 2>/dev/null || true)"; \
    if [ -z "$analyzer" ]; then \
        analyzer="$(xcrun --find scan-build 2>/dev/null || true)"; \
    fi; \
    if [ -z "$analyzer" ]; then \
        llvm_prefix="$(brew --prefix llvm 2>/dev/null || true)"; \
        if [ -n "$llvm_prefix" ] && [ -x "$llvm_prefix/bin/scan-build" ]; then \
            analyzer="$llvm_prefix/bin/scan-build"; \
        fi; \
    fi; \
    if [ -z "$analyzer" ]; then \
        echo "Error: scan-build is not installed or not discoverable via xcrun."; \
        echo "Install LLVM tools, then rerun: just analyze"; \
        echo "Hint (Homebrew): brew install llvm"; \
        exit 1; \
    fi; \
    rm -rf {{build_dir}}-analyze; \
    "$analyzer" -o {{build_dir}}-analyze/report meson setup {{build_dir}}-analyze --buildtype=debug; \
    "$analyzer" -o {{build_dir}}-analyze/report ninja -C {{build_dir}}-analyze src/runeharbor tests/runeharbor_tests
    @echo ""
    @echo "Analysis complete. Check {{build_dir}}-analyze/report/ for results"

# Run clang-tidy on all source files
tidy:
    @echo "Running clang-tidy..."
    @tidy_bin="$(command -v clang-tidy 2>/dev/null || true)"; \
    if [ -z "$tidy_bin" ]; then \
        llvm_prefix="$(brew --prefix llvm 2>/dev/null || true)"; \
        if [ -n "$llvm_prefix" ] && [ -x "$llvm_prefix/bin/clang-tidy" ]; then \
            tidy_bin="$llvm_prefix/bin/clang-tidy"; \
        fi; \
    fi; \
    if [ -z "$tidy_bin" ]; then \
        echo "Error: clang-tidy is not installed or not discoverable."; \
        echo "Install clang-tidy, then rerun: just tidy"; \
        exit 1; \
    fi
    find src -name "*.cpp" | xargs "$tidy_bin" -p {{build_dir}} --

# Run all static analysis tools
analyze-all: analyze tidy
    @echo "All static analysis complete"

# ============================================================================
# Memory Analysis
# ============================================================================

# Memory check with valgrind (Linux/macOS with valgrind installed)
valgrind: build
    @echo "Running Valgrind memory check..."
    valgrind \
        --leak-check=full \
        --show-leak-kinds=all \
        --track-origins=yes \
        --verbose \
        --log-file=valgrind-report.txt \
        ./{{build_dir}}/src/runeharbor
    @echo ""
    @echo "Valgrind report saved to: valgrind-report.txt"
    @cat valgrind-report.txt

# Quick valgrind check (less verbose)
valgrind-quick: build
    valgrind --leak-check=yes ./{{build_dir}}/src/runeharbor

# Run with address sanitizer (already built with setup-asan)
sanitizer:
    @echo "Running with Address Sanitizer..."
    @echo "Make sure you built with: just setup-asan"
    ./{{build_dir}}/src/runeharbor

# Memory check with macOS leaks tool (macOS only)
leaks: build
    @echo "Running macOS leaks tool..."
    leaks --atExit -- ./{{build_dir}}/src/runeharbor

# ============================================================================
# Code Quality
# ============================================================================

# Format all source code
format:
    find src tests tools -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i

# Check formatting without making changes
format-check:
    find src tests tools -name "*.cpp" -o -name "*.hpp" | xargs clang-format --dry-run -Werror

# Run all quality checks (format, analyze, test)
quality: format-check analyze test
    @echo "All quality checks passed!"

# ============================================================================
# Build Management
# ============================================================================

# Clean build directories
clean:
    rm -rf {{build_dir}} {{build_dir}}-* valgrind-report.txt

# Rebuild from scratch
rebuild: clean setup build

# Reconfigure build (when meson.build changes)
reconfigure:
    meson setup --reconfigure {{build_dir}}

# ============================================================================
# Development Helpers
# ============================================================================

# Watch and rebuild on file changes (requires entr)
watch:
    find src -name "*.cpp" -o -name "*.hpp" | entr -c just build

# Run with debugging (lldb on macOS, gdb on Linux)
debug: build
    lldb ./{{build_dir}}/src/runeharbor

# Generate compilation database for IDEs
compdb:
    ninja -C {{build_dir}} -t compdb > compile_commands.json

# Show build stats
stats:
    @echo "Source files:"
    @find src -name "*.cpp" -o -name "*.hpp" | wc -l
    @echo "Test files:"
    @find tests -name "*.cpp" -o -name "*.hpp" | wc -l
    @echo "Total lines of code:"
    @find src tests -name "*.cpp" -o -name "*.hpp" | xargs wc -l | tail -1
