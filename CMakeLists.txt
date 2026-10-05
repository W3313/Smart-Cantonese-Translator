cmake_minimum_required(VERSION 3.21)

project(SmartCantoneseTranslator
    VERSION 1.0.0
    DESCRIPTION "AI-powered English <-> Cantonese translator with read-aloud"
    HOMEPAGE_URL "https://github.com/w3313/smart-cantonese-translator"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt 6.4 is the minimum (Ubuntu 24.04 ships it); Windows release builds use Qt 6.8 LTS.
find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Widgets Network TextToSpeech Multimedia Svg)

option(SCT_BUILD_TESTS "Build unit tests" ON)

set(SCT_APP_NAME "Smart Cantonese Translator")
set(SCT_APP_ID "SmartCantoneseTranslator")
set(SCT_ORG_NAME "SmartCantonese")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/core/Version.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/generated/core/Version.h @ONLY)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /Zc:__cplusplus)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_compile_definitions(QT_NO_URL_CAST_FROM_STRING)

add_subdirectory(src/core)
add_subdirectory(src/tts)
add_subdirectory(src/ui)
add_subdirectory(src/app)

if(SCT_BUILD_TESTS)
    enable_testing()
    find_package(Qt6 REQUIRED COMPONENTS Test)
    add_subdirectory(tests)
endif()
