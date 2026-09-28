# miniaudio is third-party C. Build it as its own object library so the
# project's strict C++ warning flags never reach it (and so MSVC does not warn
# about a later /w overriding an earlier /W4).
add_library(TMIXMiniaudio OBJECT
    miniaudio_impl.c
)
target_include_directories(TMIXMiniaudio PRIVATE ${CMAKE_SOURCE_DIR}/third_party)

# TMIXCore: pure C++17 analysis core. Deliberately free of Qt and JUCE so the
# algorithms can be verified from the CLI and the test binary alone.
# Add algorithm sources here as milestones land.
add_library(TMIXCore STATIC
    Analysis.cpp
    AudioFile.cpp
    Decimator.cpp
    Fft.cpp
    KeyDetector.cpp
    LevelStats.cpp
    MixTiming.cpp
    NoteTiming.cpp
    OnsetDetector.cpp
    Resampler.cpp
    TempoEstimator.cpp
)

target_include_directories(TMIXCore
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_SOURCE_DIR}/third_party
)

target_link_libraries(TMIXCore PRIVATE TMIXMiniaudio)

if(MSVC)
    target_compile_options(TMIXCore PRIVATE /W4 /permissive-)
endif()

if(WIN32)
    # WASAPI device enumeration and COM need ole32; propagated to the
    # executables because a static library does not link on its own.
    target_link_libraries(TMIXCore PUBLIC ole32)
endif()
