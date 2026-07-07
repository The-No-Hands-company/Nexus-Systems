# Nexus Systems - Test Suite

# ── Legacy Phase Verification Tests ──────────────────────────────────────────
# Compile each phase*_verification.cpp as a standalone CTest executable
file(GLOB VERIFICATION_TESTS_EXCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/phase*_verification.cpp")

foreach(test_file ${VERIFICATION_TESTS})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_executable(${test_name} ${test_file})
    target_link_libraries(${test_name} PRIVATE nexus::utility)
    target_compile_features(${test_name} PRIVATE cxx_std_23)
    target_include_directories(${test_name} PRIVATE ${PROJECT_SOURCE_DIR}/include)

    add_test(
        NAME ${test_name}
        COMMAND ${test_name}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    )

    set_tests_properties(${test_name} PROPERTIES
        TIMEOUT 30
        LABELS "verification"
    )
endforeach()

# ── GoogleTest Unit Tests ────────────────────────────────────────────────────
file(GLOB_RECURSE UNIT_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/unit/*.cpp"
)

if(UNIT_TEST_SOURCES)
    add_executable(nexus_unit_tests ${UNIT_TEST_SOURCES})
    target_link_libraries(nexus_unit_tests PRIVATE
        nexus::utility
        GTest::gtest
        GTest::gtest_main
        GTest::gmock
    )
    target_compile_features(nexus_unit_tests PRIVATE cxx_std_23)
    target_include_directories(nexus_unit_tests PRIVATE
        ${PROJECT_SOURCE_DIR}/include
    )
    target_compile_options(nexus_unit_tests PRIVATE
        $<$<CXX_COMPILER_ID:GNU,Clang>:-Wno-sign-conversion -Wno-conversion>
    )

    gtest_discover_tests(nexus_unit_tests
        PROPERTIES
            TIMEOUT 60
            LABELS "unit"
    )
endif()

# ── Fuzz Targets ─────────────────────────────────────────────────────────────
if(NEXUS_ENABLE_FUZZING)
    file(GLOB_RECURSE FUZZ_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/fuzz/*.cpp")
    foreach(fuzz_file ${FUZZ_SOURCES})
        get_filename_component(fuzz_name ${fuzz_file} NAME_WE)
        add_executable(${fuzz_name} ${fuzz_file})
        target_link_libraries(${fuzz_name} PRIVATE nexus::utility)
        target_compile_features(${fuzz_name} PRIVATE cxx_std_23)
        target_compile_options(${fuzz_name} PRIVATE -fsanitize=fuzzer)
        target_link_options(${fuzz_name} PRIVATE -fsanitize=fuzzer)
    endforeach()
endif()
