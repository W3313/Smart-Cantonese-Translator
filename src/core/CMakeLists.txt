# sct_core: translation engine, settings, history. No widget dependencies.
file(GLOB SCT_CORE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_library(sct_core STATIC ${SCT_CORE_SOURCES})

target_include_directories(sct_core PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_BINARY_DIR}/generated)

target_link_libraries(sct_core PUBLIC Qt6::Core Qt6::Network)

if(WIN32)
    target_link_libraries(sct_core PRIVATE crypt32)
endif()
