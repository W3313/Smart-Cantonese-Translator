# The desktop executable.
add_executable(SmartCantoneseTranslator WIN32 main.cpp)

target_link_libraries(SmartCantoneseTranslator PRIVATE sct_ui)

set_target_properties(SmartCantoneseTranslator PROPERTIES
    OUTPUT_NAME "SmartCantoneseTranslator")

include(GNUInstallDirs)

# ---------------------------------------------------------------------------
# Windows: icon + version-info resource and application manifest
# ---------------------------------------------------------------------------
if(WIN32)
    set(SCT_RC_ICON_PATH "${PROJECT_SOURCE_DIR}/resources/icons/app.ico")
    set(SCT_RC_EXE_NAME "SmartCantoneseTranslator.exe")
    string(TIMESTAMP SCT_RC_YEAR "%Y")
    set(SCT_RC_COPYRIGHT "Copyright (C) ${SCT_RC_YEAR} ${SCT_APP_NAME} contributors")
    if(PROJECT_VERSION_TWEAK)
        set(SCT_RC_VERSION_TWEAK "${PROJECT_VERSION_TWEAK}")
    else()
        set(SCT_RC_VERSION_TWEAK 0)
    endif()

    configure_file(${PROJECT_SOURCE_DIR}/packaging/windows/app.rc.in
                   ${CMAKE_CURRENT_BINARY_DIR}/app.rc @ONLY)
    set_source_files_properties(${CMAKE_CURRENT_BINARY_DIR}/app.rc PROPERTIES
        OBJECT_DEPENDS "${SCT_RC_ICON_PATH}")

    # A .manifest source is merged into the executable's manifest by the MSVC linker.
    target_sources(SmartCantoneseTranslator PRIVATE
        ${CMAKE_CURRENT_BINARY_DIR}/app.rc
        ${PROJECT_SOURCE_DIR}/packaging/windows/app.manifest)
endif()

# ---------------------------------------------------------------------------
# Install / deployment
#
# Windows uses a flat, self-contained layout that can be zipped or wrapped by
# the Inno Setup installer (packaging/windows/installer.iss):
#
#   <prefix>/SmartCantoneseTranslator.exe
#   <prefix>/Qt6*.dll, FFmpeg DLLs (av*.dll, sw*.dll), MSVC runtime DLLs
#   <prefix>/qt.conf                (Prefix=., plugins under ./plugins)
#   <prefix>/plugins/<type>/*.dll   (platforms, styles, imageformats, iconengines,
#                                    tls, networkinformation, texttospeech, multimedia)
#   <prefix>/translations/qt_*.qm
#   <prefix>/licenses/, THIRD_PARTY_NOTICES.md, README.md
#
#   cmake --install <build> --prefix <stage>     # runs windeployqt
# ---------------------------------------------------------------------------
if(WIN32)
    install(TARGETS SmartCantoneseTranslator RUNTIME DESTINATION .)

    # MSVC C++ runtime (vcruntime140*.dll, msvcp140*.dll, concrt140.dll) next to
    # the exe, so the app runs on a clean PC without installing the VC++
    # redistributable. The Universal CRT is part of Windows 10/11.
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
    set(CMAKE_INSTALL_UCRT_LIBRARIES FALSE)
    include(InstallRequiredSystemLibraries)

    if(Qt6_VERSION VERSION_GREATER_EQUAL 6.5)
        # windeployqt options:
        #  - qtexttospeech_mock is a test-only engine; keep it out of the engine list.
        #  - The app is Qt Widgets only: no Direct3D shader compilers or the Mesa
        #    software OpenGL fallback are needed (saves ~25 MB).
        #  - Only the Qt translations the app can actually use.
        #  - The compiler runtime is handled by InstallRequiredSystemLibraries
        #    above (windeployqt would only copy the 25 MB vc_redist installer).
        # TLS: windeployqt deploys tls/qschannelbackend.dll (native Windows TLS,
        # no OpenSSL DLLs needed) and skips qopensslbackend.dll by default.
        # Multimedia: plugins/multimedia/ffmpegmediaplugin.dll + windowsmediaplugin.dll,
        # and windeployqt copies the FFmpeg DLLs Qt ships next to the exe.
        set(SCT_DEPLOY_TOOL_OPTIONS
            --no-system-d3d-compiler
            --no-opengl-sw
            --translations en,zh_TW,zh_CN)
        if(Qt6_VERSION VERSION_GREATER_EQUAL 6.7)  # options added in windeployqt 6.7
            list(APPEND SCT_DEPLOY_TOOL_OPTIONS
                --no-system-dxc-compiler
                --exclude-plugins qtexttospeech_mock)
        endif()
        list(JOIN SCT_DEPLOY_TOOL_OPTIONS " " SCT_DEPLOY_TOOL_OPTIONS_STR)

        # qt_generate_deploy_script (not ..._app_script) so the layout can be flat:
        # Qt's default would put the exe and DLLs under bin/.
        qt_generate_deploy_script(
            TARGET SmartCantoneseTranslator
            OUTPUT_SCRIPT SCT_DEPLOY_SCRIPT
            CONTENT "
set(QT_DEPLOY_BIN_DIR \".\")
qt_deploy_runtime_dependencies(
    EXECUTABLE \"$<TARGET_FILE:SmartCantoneseTranslator>\"
    GENERATE_QT_CONF
    NO_COMPILER_RUNTIME
    DEPLOY_TOOL_OPTIONS ${SCT_DEPLOY_TOOL_OPTIONS_STR}
)
")
        install(SCRIPT "${SCT_DEPLOY_SCRIPT}")
    else()
        message(WARNING
            "Qt ${Qt6_VERSION} is older than 6.5: 'cmake --install' will not run windeployqt. "
            "Release builds use Qt 6.8 LTS.")
    endif()

    set(SCT_DOC_DESTINATION .)
else()
    install(TARGETS SmartCantoneseTranslator
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        BUNDLE DESTINATION .)
    set(SCT_DOC_DESTINATION ${CMAKE_INSTALL_DOCDIR})
endif()

# Notices and license texts for the bundled third-party libraries (Qt, FFmpeg).
install(FILES
    ${PROJECT_SOURCE_DIR}/README.md
    ${PROJECT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md
    DESTINATION ${SCT_DOC_DESTINATION})
install(DIRECTORY ${PROJECT_SOURCE_DIR}/packaging/licenses/
    DESTINATION ${SCT_DOC_DESTINATION}/licenses
    FILES_MATCHING PATTERN "*.txt")
if(EXISTS ${PROJECT_SOURCE_DIR}/LICENSE)
    install(FILES ${PROJECT_SOURCE_DIR}/LICENSE DESTINATION ${SCT_DOC_DESTINATION})
endif()
