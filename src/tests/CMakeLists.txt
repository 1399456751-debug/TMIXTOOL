add_executable(TMIXTests
    TestHarness.cpp
    main.cpp
    test_decimator.cpp
    test_analysis.cpp
    test_decode.cpp
    test_fft.cpp
    test_key.cpp
    test_fixtures.cpp
    test_level_stats.cpp
    test_mix_timing.cpp
    test_note_timing.cpp
    test_onset.cpp
    test_resampler.cpp
    test_tempo.cpp
)

target_include_directories(TMIXTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(TMIXTests PRIVATE TMIXCore)

if(MSVC)
    target_compile_options(TMIXTests PRIVATE /W4 /permissive-)
endif()

# One CTest entry per registered case name is generated at build time by the
# test binary itself; this single entry runs the whole suite. The binary exits
# non-zero if any case fails, so CTest reports failure correctly.
add_test(NAME TMIXTests COMMAND TMIXTests)
