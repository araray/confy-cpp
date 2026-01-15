#!/bin/bash
# confy-cpp build script
# Provides convenient commands for building, testing, and cleaning

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_ROOT}/build"

# Default values
BUILD_TYPE="Release"
JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

# Functions
print_usage() {
    cat << EOF
${BLUE}confy-cpp Build Script${NC}

Usage: $0 [COMMAND] [OPTIONS]

Commands:
  config          Configure CMake (creates build directory)
  build           Build the project
  test            Run tests
  clean           Clean build directory
  rebuild         Clean + config + build
  install         Install to system (requires sudo)
  format          Format code with clang-format (if available)
  help            Show this help message

Options:
  -d, --debug     Build in Debug mode (default: Release)
  -r, --release   Build in Release mode
  -j N            Use N parallel jobs (default: ${JOBS})
  -v, --verbose   Verbose build output

Examples:
  $0 config              # Configure with Release build
  $0 config --debug      # Configure with Debug build
  $0 build               # Build the project
  $0 test                # Run tests
  $0 rebuild --debug     # Clean, configure debug, and build
  $0 clean               # Remove build directory

EOF
}

log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

cmd_config() {
    log_info "Configuring CMake build (${BUILD_TYPE})..."

    mkdir -p "${BUILD_DIR}"
    cd "${BUILD_DIR}"

    cmake -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
          -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
          "${PROJECT_ROOT}"

    log_success "Configuration complete"
    log_info "Build directory: ${BUILD_DIR}"
}

cmd_build() {
    if [ ! -d "${BUILD_DIR}" ]; then
        log_warning "Build directory not found. Running config first..."
        cmd_config
    fi

    log_info "Building project (${BUILD_TYPE}, ${JOBS} jobs)..."
    cd "${BUILD_DIR}"

    if [ "${VERBOSE}" = "1" ]; then
        cmake --build . --config "${BUILD_TYPE}" -j "${JOBS}" -- VERBOSE=1
    else
        cmake --build . --config "${BUILD_TYPE}" -j "${JOBS}"
    fi

    log_success "Build complete"
    log_info "Library: ${BUILD_DIR}/libconfy.a"
    log_info "CLI: ${BUILD_DIR}/bin/confy-cpp"
    log_info "Tests: ${BUILD_DIR}/tests/confy_tests"
}

cmd_test() {
    if [ ! -f "${BUILD_DIR}/tests/confy_tests" ]; then
        log_warning "Test executable not found. Building first..."
        cmd_build
    fi

    log_info "Running tests..."
    cd "${BUILD_DIR}"

    if [ "${VERBOSE}" = "1" ]; then
        ctest --output-on-failure --verbose
    else
        ctest --output-on-failure
    fi

    log_success "All tests passed"
}

cmd_clean() {
    if [ -d "${BUILD_DIR}" ]; then
        log_info "Cleaning build directory..."
        rm -rf "${BUILD_DIR}"
        log_success "Build directory removed"
    else
        log_info "Build directory does not exist"
    fi
}

cmd_rebuild() {
    log_info "Rebuilding from scratch..."
    cmd_clean
    cmd_config
    cmd_build
    log_success "Rebuild complete"
}

cmd_install() {
    if [ ! -d "${BUILD_DIR}" ]; then
        log_error "Build directory not found. Run 'build' first."
        exit 1
    fi

    log_info "Installing confy-cpp..."
    cd "${BUILD_DIR}"

    if [ "$EUID" -ne 0 ]; then
        log_warning "Installation requires sudo"
        sudo cmake --install .
    else
        cmake --install .
    fi

    log_success "Installation complete"
}

cmd_format() {
    if ! command -v clang-format &> /dev/null; then
        log_error "clang-format not found. Please install it first."
        exit 1
    fi

    log_info "Formatting C++ code..."

    find "${PROJECT_ROOT}/include" "${PROJECT_ROOT}/src" "${PROJECT_ROOT}/tests" \
        -name '*.cpp' -o -name '*.hpp' | while read -r file; do
        clang-format -i "$file"
        log_info "Formatted: $file"
    done

    log_success "Code formatting complete"
}

# Parse arguments
COMMAND=""
VERBOSE=0

while [[ $# -gt 0 ]]; do
    case $1 in
        config|build|test|clean|rebuild|install|format|help)
            COMMAND="$1"
            shift
            ;;
        -d|--debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        -r|--release)
            BUILD_TYPE="Release"
            shift
            ;;
        -j)
            JOBS="$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        *)
            log_error "Unknown option: $1"
            print_usage
            exit 1
            ;;
    esac
done

# Execute command
case "${COMMAND}" in
    config)
        cmd_config
        ;;
    build)
        cmd_build
        ;;
    test)
        cmd_test
        ;;
    clean)
        cmd_clean
        ;;
    rebuild)
        cmd_rebuild
        ;;
    install)
        cmd_install
        ;;
    format)
        cmd_format
        ;;
    help|"")
        print_usage
        ;;
    *)
        log_error "Unknown command: ${COMMAND}"
        print_usage
        exit 1
        ;;
esac
