# DAWG Audio Engine - Standalone Engine Module
# Professional-grade audio engine with 292,864+ voice capacity
# Engine is completely GUI-independent and can be integrated into any project

# Engine configuration (inherits from parent project)
# C++ standard is set by parent project

# Engine build options (can be overridden by parent)
if(NOT DEFINED DAWG_ENGINE_BUILD_SHARED)
    option(DAWG_ENGINE_BUILD_SHARED "Build engine as shared library" OFF)
endif()
if(NOT DEFINED DAWG_ENGINE_ENABLE_SIMD)
    option(DAWG_ENGINE_ENABLE_SIMD "Enable SIMD optimizations" ON)
endif()
if(NOT DEFINED DAWG_ENGINE_ENABLE_LTO)
    option(DAWG_ENGINE_ENABLE_LTO "Enable Link Time Optimization" ON)
endif()

# Add SIMD definition if enabled
if(DAWG_ENGINE_ENABLE_SIMD)
    add_definitions(-DDAWG_ENABLE_SIMD)
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Core engine sources
set(DAWG_ENGINE_CORE_SOURCES
    src/core/AudioEngine.cpp
    src/core/AudioBuffer.cpp
    src/core/VoiceManager.cpp
    src/core/Transport.cpp
    src/core/Timeline.cpp
    src/core/dawg.cpp
)

# DSP sources
set(DAWG_ENGINE_DSP_SOURCES
    src/dsp/Effects.cpp
    src/dsp/Filters.cpp
    src/dsp/Reverb.cpp
    src/dsp/Dynamics.cpp
    src/dsp/Modulation.cpp
    src/dsp/SineGenerator.cpp
    src/dsp/Mixer.cpp
    # Modular DSP effects
    src/dsp/dynamics/Compressor.cpp
    src/dsp/dynamics/Limiter.cpp
    src/dsp/eq/ParametricEQ.cpp
    src/dsp/time/AlgorithmicReverb.cpp
    src/dsp/time/Delay.cpp
    src/dsp/modulation/Chorus.cpp
)

# MIDI sources
set(DAWG_ENGINE_MIDI_SOURCES
    src/midi/MidiInterface.cpp
    src/midi/MidiMapping.cpp
    src/midi/MidiSequencer.cpp
    src/midi/MidiEffects.cpp
)

# I/O sources
set(DAWG_ENGINE_IO_SOURCES
    src/io/AudioFileReader.cpp
    src/io/AudioFileWriter.cpp
    src/io/StreamingEngine.cpp
    src/io/DeviceManager.cpp
)

# Combine all engine sources
set(DAWG_ENGINE_SOURCES
    ${DAWG_ENGINE_CORE_SOURCES}
    ${DAWG_ENGINE_DSP_SOURCES}
    ${DAWG_ENGINE_MIDI_SOURCES}
    ${DAWG_ENGINE_IO_SOURCES}
)

# Build engine library
if(DAWG_ENGINE_BUILD_SHARED)
    add_library(dawg_engine SHARED ${DAWG_ENGINE_SOURCES})
    target_compile_definitions(dawg_engine PRIVATE DAWG_ENGINE_BUILDING_DLL)
    target_compile_definitions(dawg_engine PUBLIC DAWG_ENGINE_SHARED)
else()
    add_library(dawg_engine STATIC ${DAWG_ENGINE_SOURCES})
endif()

# Engine library properties
set_target_properties(dawg_engine PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME "dawg_engine"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Engine public headers
target_include_directories(dawg_engine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# System libraries
find_package(Threads REQUIRED)
target_link_libraries(dawg_engine PUBLIC Threads::Threads)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(dawg_engine PRIVATE 
        kernel32 user32 gdi32 winspool shell32 ole32 
        oleaut32 uuid comdlg32 advapi32
    )
endif()

# Install configuration for engine-only distribution
install(TARGETS dawg_engine
    EXPORT DawgEngineTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(DIRECTORY include/
    DESTINATION include
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# Create and install the export set
install(EXPORT DawgEngineTargets
    FILE DawgEngineConfig.cmake
    NAMESPACE Dawg::
    DESTINATION lib/cmake/DawgEngine
)

# Generate and install version file
include(CMakePackageConfigHelpers)
if(NOT PROJECT_VERSION)
    set(PROJECT_VERSION "1.0.0")
    set(PROJECT_VERSION_MAJOR "1")
    set(PROJECT_VERSION_MINOR "0")
    set(PROJECT_VERSION_PATCH "0")
    message(STATUS "Setting default PROJECT_VERSION=${PROJECT_VERSION}")
endif()
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/DawgEngineConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/DawgEngineConfigVersion.cmake"
    DESTINATION lib/cmake/DawgEngine
)
