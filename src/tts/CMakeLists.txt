# sct_tts: read-aloud (Windows/system voices via QTextToSpeech, Azure neural voices via REST).
file(GLOB SCT_TTS_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_library(sct_tts STATIC ${SCT_TTS_SOURCES})

target_link_libraries(sct_tts PUBLIC sct_core Qt6::TextToSpeech Qt6::Multimedia Qt6::Network)
