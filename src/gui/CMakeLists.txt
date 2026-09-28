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
