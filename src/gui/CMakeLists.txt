find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 Concurrent)

qt_standard_project_setup(REQUIRES 6.5)

# Palette and strings are singletons: one instance, referenced by name from
# every QML file rather than threaded down through properties.
set_source_files_properties(Theme.qml Lang.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_executable(TMIXTOOL
    main.cpp
    # Compiled into the .exe so Explorer, shortcuts and pinned taskbar entries
    # show the application icon rather than a blank document.
    app.rc
)

# rc.exe pulls assets/tmixtool.ico in through app.rc, and CMake cannot see
# that: listing app.rc as a source only tracks edits to the .rc itself.
# Replacing the icon therefore rebuilt nothing and left the .exe carrying the
# previous one, with no warning and nothing in the build output to show for it.
# Naming the icon here is what makes the resource recompile.
#
# The qt_add_resources copy below is a separate thing with its own dependency
# tracking, which is why the icon inside the Qt resource bundle stayed current
# while the one Explorer reads went stale.
set_property(SOURCE app.rc PROPERTY OBJECT_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/tmixtool.ico")

# qt_add_resources rather than a .qrc listed in the sources: AUTORCC is not
# switched on by qt_standard_project_setup, so a plain .qrc was silently
# ignored and the icon and artwork never made it into the binary.
qt_add_resources(TMIXTOOL "app_assets"
    PREFIX "/"
    FILES
        assets/tmixtool.ico
        assets/artwork.png
)

qt_add_qml_module(TMIXTOOL
    URI Tmixtool
    VERSION 1.0
    SOURCES
        AnalysisController.cpp
        AnalysisController.h
        WindowPin.cpp
        WindowPin.h
    QML_FILES
        Main.qml
        Theme.qml
        Lang.qml
        Card.qml
        TimeTable.qml
        DelayTable.qml
)

target_link_libraries(TMIXTOOL PRIVATE
    TMIXCore
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Concurrent
)

if(MSVC)
    target_compile_options(TMIXTOOL PRIVATE /W4 /permissive-)
endif()

# GUI binary: no console window on launch.
set_target_properties(TMIXTOOL PROPERTIES WIN32_EXECUTABLE ON)
