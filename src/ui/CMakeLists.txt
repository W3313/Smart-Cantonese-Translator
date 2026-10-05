# sct_ui: Qt Widgets user interface.
file(GLOB SCT_UI_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/*.ui)

add_library(sct_ui STATIC ${SCT_UI_SOURCES})

target_link_libraries(sct_ui PUBLIC sct_core sct_tts Qt6::Widgets Qt6::Svg)
