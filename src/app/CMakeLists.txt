# The desktop executable.
add_executable(SmartCantoneseTranslator WIN32 main.cpp)

target_link_libraries(SmartCantoneseTranslator PRIVATE sct_ui)

set_target_properties(SmartCantoneseTranslator PROPERTIES
    OUTPUT_NAME "SmartCantoneseTranslator")
