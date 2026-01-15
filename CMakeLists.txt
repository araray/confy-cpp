# ============================================================================
# confy-cpp CMake Build Configuration
# ============================================================================
#
# Cross-platform configuration management library for C++.
# A C++ port of the Python confy library with exact behavioral parity.
#
# Requirements:
# - C++17 compiler (GCC 11+, Clang 14+, MSVC 2022)
# - CMake 3.20+
#
# Usage:
#   mkdir build && cd build
#   cmake -DCMAKE_BUILD_TYPE=Release ..
#   cmake --build . -j
#   ctest --output-on-failure
#
# ============================================================================

cmake_minimum_required(VERSION 3.20)

project(confy
    VERSION 0.2.0
    DESCRIPTION "Configuration management library for C++ (Phase 2: Source Loaders)"
    LANGUAGES CXX
)

# ============================================================================
# Compiler Settings
# ============================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific flags
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror=return-type)
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 11)
        message(FATAL_ERROR "GCC 11+ required for C++17 support")
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
    add_compile_options(-Wall -Wextra -Wpedantic -Werror=return-type)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
            message(FATAL_ERROR "Apple Clang 14+ required")
        endif()
    else()
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
            message(FATAL_ERROR "Clang 14+ required")
        endif()
    endif()
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4 /permissive-)
    if(MSVC_VERSION LESS 1930)
        message(FATAL_ERROR "MSVC 2022 (v17.0+) required")
    endif()
endif()

# ============================================================================
# Dependencies - System Packages (preferred) or FetchContent (fallback)
# ============================================================================

# nlohmann/json - JSON value type and parsing
find_package(nlohmann_json 3.11 QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found, fetching from source...")
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)
else()
    message(STATUS "Found nlohmann_json: ${nlohmann_json_VERSION}")
endif()

# toml++ - TOML parsing (Phase 2)
find_package(tomlplusplus 3.4 QUIET)
if(NOT tomlplusplus_FOUND)
    message(STATUS "tomlplusplus not found, fetching from source...")
    include(FetchContent)
    FetchContent_Declare(
        tomlplusplus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        GIT_TAG        v3.4.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(tomlplusplus)
else()
    message(STATUS "Found tomlplusplus: ${tomlplusplus_VERSION}")
endif()

# cxxopts - CLI parsing (Phase 4): header-only, inline if not found
find_package(cxxopts 3.0 QUIET)
if(NOT cxxopts_FOUND)
    message(STATUS "cxxopts not found, creating interface target...")
    # Create a placeholder interface target; CLI is Phase 4 stub anyway
    add_library(cxxopts INTERFACE)
    add_library(cxxopts::cxxopts ALIAS cxxopts)
    target_compile_definitions(cxxopts INTERFACE CXXOPTS_NOT_AVAILABLE=1)
else()
    message(STATUS "Found cxxopts: ${cxxopts_VERSION}")
endif()

# ============================================================================
# Library: libconfy
# ============================================================================

add_library(confy STATIC
    # Phase 1: Core Infrastructure
    src/DotPath.cpp
    src/Parse.cpp
    src/Merge.cpp
    src/Util.cpp

    # Phase 2: Source Loaders
    src/EnvMapper.cpp
    src/Loader.cpp

    # Phase 3: Config Class (stub)
    src/Config.cpp
)

target_include_directories(confy PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(confy PUBLIC
    nlohmann_json::nlohmann_json
    tomlplusplus::tomlplusplus
)

# Enable detailed error messages
target_compile_definitions(confy PUBLIC
    CONFY_DETAILED_ERRORS=1
)

# ============================================================================
# CLI: confy-cpp executable (Phase 4 - stub)
# ============================================================================

add_executable(confy-cpp src/cli_main.cpp)
target_link_libraries(confy-cpp PRIVATE confy)
# cxxopts only linked if available
if(TARGET cxxopts::cxxopts)
    target_link_libraries(confy-cpp PRIVATE cxxopts::cxxopts)
elseif(TARGET cxxopts)
    target_link_libraries(confy-cpp PRIVATE cxxopts)
endif()

set_target_properties(confy-cpp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ============================================================================
# Tests: using GoogleTest
# ============================================================================

include(CTest)
if(BUILD_TESTING)
    # Try system GTest first
    find_package(GTest QUIET)
    if(NOT GTest_FOUND)
        message(STATUS "GoogleTest not found, fetching from source...")
        include(FetchContent)
        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG        v1.14.0
            GIT_SHALLOW    TRUE
        )
        # Prevent overriding the parent project's compiler/linker settings on Windows
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(googletest)
    else()
        message(STATUS "Found GoogleTest: ${GTest_VERSION}")
    endif()

    # Test executable
    add_executable(confy_tests
        tests/test_main.cpp
        tests/test_dotpath.cpp
        tests/test_env_mapper.cpp
        tests/test_loader.cpp
        tests/test_config.cpp
    )

    target_link_libraries(confy_tests PRIVATE
        confy
        GTest::gtest_main
    )

    # Suppress warnings for generated code
    target_compile_options(confy_tests PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-Wno-maybe-uninitialized>
    )

    # Register tests with CTest
    include(GoogleTest)
    gtest_discover_tests(confy_tests)
endif()

# ============================================================================
# Installation (disabled by default when using FetchContent dependencies)
# ============================================================================

option(CONFY_INSTALL "Enable installation targets" OFF)

if(CONFY_INSTALL)
    include(GNUInstallDirs)

    install(TARGETS confy
        EXPORT confy-targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(DIRECTORY include/confy
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(TARGETS confy-cpp
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    # Package Configuration
    install(EXPORT confy-targets
        FILE confy-targets.cmake
        NAMESPACE confy::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/confy
    )

    include(CMakePackageConfigHelpers)

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/confy-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/confy-config.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/confy
    )

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/confy-config-version.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/confy-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/confy-config-version.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/confy
    )
endif()

# ============================================================================
# Status Summary
# ============================================================================

message(STATUS "")
message(STATUS "=== confy-cpp Build Configuration ===")
message(STATUS "Version:     ${PROJECT_VERSION}")
message(STATUS "Build type:  ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Testing: ${BUILD_TESTING}")
message(STATUS "")
