add_executable(TMIXCLI
    main.cpp
)

target_link_libraries(TMIXCLI PRIVATE TMIXCore)

if(MSVC)
    target_compile_options(TMIXCLI PRIVATE /W4 /permissive-)
endif()
