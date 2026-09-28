# The installer is a plain Win32 binary - no Qt, no runtime beyond what every
# Windows install already has. It has to run on a friend's machine before
# anything else from this project exists there.

add_executable(TMIXSetup WIN32
    setup.cpp
    setup.rc
)

target_include_directories(TMIXSetup PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# shell32 for the shell link and known folders, ole32 for COM, comctl32 for the
# progress bar, shlwapi kept for path helpers.
target_link_libraries(TMIXSetup PRIVATE shell32 ole32 comctl32 shlwapi)

target_compile_definitions(TMIXSetup PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)

if(MSVC)
    target_compile_options(TMIXSetup PRIVATE /W4 /permissive-)
    # The .rc carries Chinese captions and declares its own code page, so no
    # extra resource-compiler flag is needed here.
endif()
