# Nexus Systems - Example Programs

add_executable(nexus_example_usage example_usage.cpp)
target_link_libraries(nexus_example_usage PRIVATE nexus::utility)
target_compile_features(nexus_example_usage PRIVATE cxx_std_23)
target_include_directories(nexus_example_usage PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_test(
    NAME nexus_example_usage
    COMMAND nexus_example_usage
)
