# sct_ui: Qt Widgets user interface (hand-written widgets, no .ui files).
file(GLOB SCT_UI_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_library(sct_ui STATIC ${SCT_UI_SOURCES})

target_link_libraries(sct_ui PUBLIC sct_core sct_tts Qt6::Widgets Qt6::Svg)

# Monochrome UI icons (:/icons/<name>.svg, tinted at runtime) plus the
# application icon (:/icons/app.svg, app.png, app.ico - the .ico carries
# hand-tuned 16-48 px images for the title bar and taskbar).
file(GLOB SCT_UI_ICON_FILES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/resources/icons/*.svg"
    "${PROJECT_SOURCE_DIR}/resources/icons/*.png"
    "${PROJECT_SOURCE_DIR}/resources/icons/*.ico")

qt_add_resources(sct_ui "sct_ui_resources"
    PREFIX "/"
    BASE "${PROJECT_SOURCE_DIR}/resources"
    FILES ${SCT_UI_ICON_FILES})
