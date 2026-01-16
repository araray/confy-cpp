/**
 * @file test_main.cpp
 * @brief GoogleTest main entry point
 *
 * This file provides the main() function for running all GoogleTest tests.
 * Each test file registers its tests automatically via the TEST() macro.
 *
 * Build: cmake --build . --target confy_tests
 * Run:   ./confy_tests
 *
 * @copyright (c) 2026. MIT License.
 */

#include <gtest/gtest.h>

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
