# Unit tests (Qt Test). Run with: ctest --test-dir <build> --output-on-failure

# sct_add_test(<name> SOURCES <files...> LIBS <targets...>)
function(sct_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;LIBS" ${ARGN})
    add_executable(${name} ${ARG_SOURCES})
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARG_LIBS})
    add_test(NAME ${name} COMMAND ${name})
    # Tests never need a display or real audio device.
    set_tests_properties(${name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endfunction()

add_subdirectory(core)
add_subdirectory(tts)
add_subdirectory(ui)
