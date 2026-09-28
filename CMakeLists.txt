cmake_minimum_required(VERSION 3.22)
project(TMIXTOOL VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Static CRT so the portable package runs on a machine without the VC++
# redistributable installed. Costs roughly 400 KB of binary size.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

if(MSVC)
    add_compile_options(/utf-8)
endif()

enable_testing()

# GUI target needs Qt and is switched on once the QML layer exists (milestone M6).
option(TMIXTOOL_BUILD_GUI "Build the Qt/QML desktop GUI" ON)

add_subdirectory(src/core)
add_subdirectory(src/cli)
add_subdirectory(src/tests)

if(TMIXTOOL_BUILD_GUI)
    add_subdirectory(src/gui)
endif()

# A plain Win32 installer/uninstaller for handing the app to other people.
# Source is complete and compiles, but packaging it needs a tool that is not
# reachable from here (IExpress cannot handle the 1351 files of the QML tree).
option(TMIXTOOL_BUILD_INSTALLER "Build the Windows installer" OFF)

if(TMIXTOOL_BUILD_INSTALLER)
    add_subdirectory(installer)
endif()
