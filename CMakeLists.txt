cmake_minimum_required(VERSION 3.20)
project(confy-cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)

# nlohmann/json
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# toml++
FetchContent_Declare(
  tomlplusplus
  GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
  GIT_TAG        v3.4.0
)
FetchContent_MakeAvailable(tomlplusplus)

# cxxopts (CLI parser)
FetchContent_Declare(
  cxxopts
  GIT_REPOSITORY https://github.com/jarro2783/cxxopts.git
  GIT_TAG        v3.2.0
)
FetchContent_MakeAvailable(cxxopts)

# Library
add_library(confy STATIC
  src/Config.cpp
  src/Util.cpp
)
target_include_directories(confy PUBLIC include)
target_link_libraries(confy PUBLIC nlohmann_json::nlohmann_json tomlplusplus::tomlplusplus)

# CLI
add_executable(confy-cpp src/cli_main.cpp)
target_link_libraries(confy-cpp PRIVATE confy cxxopts)

# Tests (Catch2)
include(CTest)
if(BUILD_TESTING)
  FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.6.0
  )
  FetchContent_MakeAvailable(Catch2)
  add_executable(confy_tests tests/test_config.cpp)
  target_link_libraries(confy_tests PRIVATE confy Catch2::Catch2WithMain)
  add_test(NAME confy_unit COMMAND confy_tests)
endif()

# Install
include(GNUInstallDirs)
install(TARGETS confy confy-cpp
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
