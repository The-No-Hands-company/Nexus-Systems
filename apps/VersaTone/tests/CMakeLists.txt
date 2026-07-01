# Tests for DAWG audio engine

# Comprehensive DSP Test Suite (standalone, no external dependencies)
add_executable(comprehensive_dsp_test comprehensive_dsp_test.cpp)
target_link_libraries(comprehensive_dsp_test PRIVATE dawg_engine)
target_include_directories(comprehensive_dsp_test PRIVATE ../engine/include)

# Debug compressor test
add_executable(debug_compressor debug_compressor.cpp)
target_link_libraries(debug_compressor PRIVATE dawg_engine)
target_include_directories(debug_compressor PRIVATE ../engine/include)

# Debug reverb test
add_executable(debug_reverb debug_reverb.cpp)
target_link_libraries(debug_reverb PRIVATE dawg_engine)
target_include_directories(debug_reverb PRIVATE ../engine/include)

find_package(GTest QUIET)
if(GTest_FOUND)
    # Unit tests
    file(GLOB_RECURSE UNIT_TESTS unit/*.cpp)
    add_executable(dawg_unit_tests ${UNIT_TESTS})
    target_link_libraries(dawg_unit_tests PRIVATE dawg_engine GTest::gtest_main)
    gtest_discover_tests(dawg_unit_tests)

    # Integration tests  
    file(GLOB_RECURSE INTEGRATION_TESTS integration/*.cpp)
    add_executable(dawg_integration_tests ${INTEGRATION_TESTS})
    target_link_libraries(dawg_integration_tests PRIVATE dawg_engine GTest::gtest_main)
    
    # Performance benchmarks
    file(GLOB_RECURSE BENCHMARK_TESTS performance/*.cpp)
    add_executable(dawg_benchmarks ${BENCHMARK_TESTS})
    target_link_libraries(dawg_benchmarks PRIVATE dawg_engine)
else()
    message(STATUS "GTest not found, standard tests will not be built")
    message(STATUS "Comprehensive DSP test suite will still be built (no external dependencies)")
endif()
