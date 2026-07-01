# DAWG Audio Editor - GUI Frontend for DAWG Audio Engine
# Professional DAW interface built on top of the standalone engine

cmake_minimum_required(VERSION 3.20)
project(DawgAudioEditor
    VERSION 1.0.0
    DESCRIPTION "Professional DAW interface for DAWG Audio Engine"
    LANGUAGES CXX
)

# Editor configuration
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Editor build options
option(DAWG_EDITOR_BUILD_PLUGINS "Build editor plugins" ON)
option(DAWG_EDITOR_ENABLE_THEMES "Enable advanced theming" ON)

# Platform-specific compiler settings
if(WIN32 AND MINGW)
    set(CMAKE_C_COMPILER "D:/msys64/mingw64/bin/gcc.exe")
    set(CMAKE_CXX_COMPILER "D:/msys64/mingw64/bin/g++.exe")
    set(CMAKE_MAKE_PROGRAM "D:/msys64/mingw64/bin/ninja.exe")
endif()

# Find the DAWG Engine (should be built first)
find_package(DawgEngine QUIET)
if(NOT DawgEngine_FOUND)
  # If not installed via find_package, assume it's being built in this project
  # since we're processing the editor after the engine in the main CMakeLists.txt
  set(DAWG_ENGINE_TARGET dawg_engine)
else()
  set(DAWG_ENGINE_TARGET Dawg::dawg_engine)
endif()

# Find Qt6 for UI
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Multimedia Network)

# Editor source files
   set(DAWG_EDITOR_SOURCES
       src/MainWindow.cpp
       src/DAWTheme.cpp
       src/FileBrowser.cpp
       src/MixerConsole.cpp
       src/MixerView.cpp
       src/PatternSequencerWidget.cpp
       src/PianoRoll.cpp
       src/PluginBrowser.cpp
       src/PluginEditor.cpp
       src/SimpleWavPlayer.cpp
       src/TimelineView.cpp
       src/TrackView.cpp
       src/TransportBar.cpp
       src/WaveformPreview.cpp
       src/CategoryFilter.cpp
   )

# Create editor executable
add_executable(dawg_editor ${DAWG_EDITOR_SOURCES})

# Link with engine and Qt
target_link_libraries(dawg_editor PRIVATE
    ${DAWG_ENGINE_TARGET}
    Qt6::Core
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::Network
)

# Editor include directories
target_include_directories(dawg_editor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../engine/include
)

# Editor compile definitions
target_compile_definitions(dawg_editor PRIVATE
    DAWG_EDITOR_VERSION="${PROJECT_VERSION}"
    QT_NO_DEBUG_OUTPUT
)

if(DAWG_EDITOR_ENABLE_THEMES)
    target_compile_definitions(dawg_editor PRIVATE DAWG_EDITOR_THEMES)
endif()

# Set output directory
set_target_properties(dawg_editor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Install editor
install(TARGETS dawg_editor
    RUNTIME DESTINATION bin
)
