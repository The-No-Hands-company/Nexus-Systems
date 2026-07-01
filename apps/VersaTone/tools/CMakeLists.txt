# Development tools for DAWG

add_executable(audio_analyzer audio_analyzer.cpp)
target_link_libraries(audio_analyzer PRIVATE dawg_engine)

add_executable(voice_profiler voice_profiler.cpp)
target_link_libraries(voice_profiler PRIVATE dawg_engine)
