# DAWG Engine Examples
# Demonstrates how to integrate the DAWG Audio Engine into various projects

cmake_minimum_required(VERSION 3.20)

# Voice system demonstration (our current demo)
add_executable(voice_system_demo voice_system_demo.cpp)
target_link_libraries(voice_system_demo dawg_engine)
target_include_directories(voice_system_demo PRIVATE ../engine/include)

# Simple audio player example
add_executable(simple_player simple_player.cpp)
target_link_libraries(simple_player dawg_engine)
target_include_directories(simple_player PRIVATE ../engine/include)

# MIDI processing example  
add_executable(midi_processor midi_processor.cpp)
target_link_libraries(midi_processor dawg_engine)
target_include_directories(midi_processor PRIVATE ../engine/include)

# Real-time effects example
add_executable(realtime_effects realtime_effects.cpp)
target_link_libraries(realtime_effects dawg_engine)
target_include_directories(realtime_effects PRIVATE ../engine/include)

# Standalone integration demonstration
add_executable(standalone_integration_demo standalone_integration_demo.cpp)
target_link_libraries(standalone_integration_demo dawg_engine)
target_include_directories(standalone_integration_demo PRIVATE ../engine/include)

# Real-time audio processing demonstration
add_executable(realtime_audio_demo realtime_audio_demo.cpp)
target_link_libraries(realtime_audio_demo dawg_engine)
target_include_directories(realtime_audio_demo PRIVATE ../engine/include)

# Set output directory
set_target_properties(
    voice_system_demo simple_player midi_processor realtime_effects standalone_integration_demo realtime_audio_demo
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/examples"
)
