# RuneHarbor Engine - Build automation

# Default recipe - show available commands
default:
    @just --list

# Setup debug build
setup:
    meson setup builddir --buildtype=debug

# Setup release build
setup-release:
    meson setup builddir --buildtype=release

# Setup with address sanitizer
setup-asan:
    meson setup builddir -Db_sanitize=address,undefined --buildtype=debug

# Build the project
build:
    ninja -C builddir

# Run the engine
run: build
    ./builddir/src/runeharbor

# Run with game data path
run-with-data DATA_PATH: build
    ./builddir/src/runeharbor --data {{DATA_PATH}}

# ============================================================================
# LOD Archive Tools
# ============================================================================

# List files in a LOD archive
lod-list ARCHIVE: build
    ./builddir/tools/lod-extract list {{ARCHIVE}}

# Extract a single file from LOD archive
lod-extract ARCHIVE FILENAME OUTPUT="": build
    ./builddir/tools/lod-extract extract {{ARCHIVE}} {{FILENAME}} {{OUTPUT}}

# Extract all files from LOD archive
lod-extract-all ARCHIVE OUTPUT_DIR: build
    ./builddir/tools/lod-extract extract-all {{ARCHIVE}} {{OUTPUT_DIR}}

# ============================================================================
# Testing
# ============================================================================

# Run all unit tests
test: build
    meson test -C builddir --print-errorlogs

# Run tests with verbose output
test-verbose: build
    meson test -C builddir --verbose

# Run specific test by name
test-filter PATTERN: build
    meson test -C builddir --verbose {{PATTERN}}

# Run tests and show coverage (requires gcov setup)
test-coverage: build
    meson test -C builddir
    ninja -C builddir coverage

# ============================================================================
# Static Analysis
# ============================================================================

# Run clang static analyzer (scan-build)
analyze:
    @echo "Running Clang Static Analyzer..."
    @rm -rf builddir-analyze
    scan-build -o builddir-analyze/report meson setup builddir-analyze --buildtype=debug
    scan-build -o builddir-analyze/report ninja -C builddir-analyze
    @echo ""
    @echo "Analysis complete. Check builddir-analyze/report/ for results"

# Run clang-tidy on all source files
tidy:
    @echo "Running clang-tidy..."
    find src -name "*.cpp" | xargs clang-tidy -p builddir --

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
        ./builddir/src/runeharbor
    @echo ""
    @echo "Valgrind report saved to: valgrind-report.txt"
    @cat valgrind-report.txt

# Quick valgrind check (less verbose)
valgrind-quick: build
    valgrind --leak-check=yes ./builddir/src/runeharbor

# Run with address sanitizer (already built with setup-asan)
sanitizer:
    @echo "Running with Address Sanitizer..."
    @echo "Make sure you built with: just setup-asan"
    ./builddir/src/runeharbor

# Memory check with macOS leaks tool (macOS only)
leaks: build
    @echo "Running macOS leaks tool..."
    leaks --atExit -- ./builddir/src/runeharbor

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
    rm -rf builddir builddir-* build-* valgrind-report.txt

# Rebuild from scratch
rebuild: clean setup build

# Reconfigure build (when meson.build changes)
reconfigure:
    meson setup --reconfigure builddir

# ============================================================================
# Development Helpers
# ============================================================================

# Watch and rebuild on file changes (requires entr)
watch:
    find src -name "*.cpp" -o -name "*.hpp" | entr -c just build

# Run with debugging (lldb on macOS, gdb on Linux)
debug: build
    lldb ./builddir/src/runeharbor

# Generate compilation database for IDEs
compdb:
    ninja -C builddir -t compdb > compile_commands.json

# Show build stats
stats:
    @echo "Source files:"
    @find src -name "*.cpp" -o -name "*.hpp" | wc -l
    @echo "Test files:"
    @find tests -name "*.cpp" -o -name "*.hpp" | wc -l
    @echo "Total lines of code:"
    @find src tests -name "*.cpp" -o -name "*.hpp" | xargs wc -l | tail -1
