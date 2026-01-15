cmake_minimum_required(VERSION 3.20)
project(confy-cpp VERSION 0.1.0 LANGUAGES CXX)

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Platform-specific compiler settings
if(MSVC)
    add_compile_options(/W4 /utf-8)
    # Treat warnings as errors in CI, but not by default for development
    if(DEFINED ENV{CI})
        add_compile_options(/WX)
    endif()
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
    # Treat warnings as errors in CI, but not by default for development
    if(DEFINED ENV{CI})
        add_compile_options(-Werror)
    endif()
endif()

# Enable color diagnostics
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-fdiagnostics-color=always)
endif()

include(FetchContent)

# nlohmann/json - JSON parsing and value type
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)

# toml++ - TOML parsing
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
    GIT_SHALLOW    TRUE
)

# cxxopts - CLI parsing
FetchContent_Declare(
    cxxopts
    GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
    GIT_TAG        v3.2.0
    GIT_SHALLOW    TRUE
)

message(STATUS "Fetching dependencies (this may take a moment)...")
FetchContent_MakeAvailable(nlohmann_json tomlplusplus cxxopts)

# ============================================================================
# Library: libconfy (static)
# ============================================================================

add_library(confy STATIC
    src/DotPath.cpp
    src/Parse.cpp
    src/Merge.cpp
    src/EnvMapper.cpp
    src/Loader.cpp
    src/Config.cpp
    src/Util.cpp
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
# CLI: confy-cpp executable
# ============================================================================

add_executable(confy-cpp src/cli_main.cpp)
target_link_libraries(confy-cpp PRIVATE confy cxxopts)

# Set output directory for easier testing
set_target_properties(confy-cpp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ============================================================================
# Tests: using Catch2
# ============================================================================

include(CTest)
if(BUILD_TESTING)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.6.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(Catch2)

    # Test executable
    add_executable(confy_tests
        tests/test_main.cpp
        tests/test_dotpath.cpp
        tests/test_parse.cpp
        tests/test_merge.cpp
        tests/test_env_mapper.cpp
        tests/test_config.cpp
    )

    target_link_libraries(confy_tests PRIVATE
        confy
        Catch2::Catch2WithMain
    )

    # Register tests with CTest
    add_test(NAME confy_unit_tests COMMAND confy_tests)

    # Set test output directory
    set_target_properties(confy_tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )
endif()

# ============================================================================
# Installation
# ============================================================================

include(GNUInstallDirs)

install(TARGETS confy confy-cpp
    EXPORT confy-targets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(DIRECTORY include/confy
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export targets for find_package
install(EXPORT confy-targets
    FILE confy-targets.cmake
    NAMESPACE confy::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/confy
)

# ============================================================================
# Summary
# ============================================================================

message(STATUS "")
message(STATUS "=======================================================")
message(STATUS "  confy-cpp Configuration Summary")
message(STATUS "=======================================================")
message(STATUS "  Version:            ${PROJECT_VERSION}")
message(STATUS "  C++ Standard:       C++${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type:         ${CMAKE_BUILD_TYPE}")
message(STATUS "  Compiler:           ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Build Tests:        ${BUILD_TESTING}")
message(STATUS "=======================================================")
message(STATUS "")
