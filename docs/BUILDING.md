# Building Smart Cantonese Translator

The app is C++20 with Qt 6 Widgets and is built with CMake. Windows is the
shipping platform. Linux is supported for development and CI.

```
src/core   translation engine, settings, history, secret storage  (sct_core)
src/tts    read-aloud: Qt TextToSpeech + Azure neural TTS          (sct_tts)
src/ui     Qt Widgets user interface                               (sct_ui)
src/app    main() and the executable, Windows resources, install rules
tests/     Qt Test unit tests (ctest)
packaging/ Windows resources (.rc/.manifest), Inno Setup script, license texts, icon tools
```

## Windows (release-quality build)

### Prerequisites

| Tool | Version | Notes |
|---|---|---|
| Visual Studio 2022 | 17.x | Workload **Desktop development with C++**. It includes CMake and Ninja. |
| Qt | **6.8.3**, *MSVC 2022 64-bit* | Plus the add-ons **Qt Multimedia** and **Qt Speech**. Qt Svg is part of the base install. |
| CMake | ≥ 3.21 | The copy that comes with VS 2022 works. |
| Inno Setup | 6.3+ | Only needed to build the installer. |

Install Qt with the [Qt Online Installer](https://www.qt.io/download-qt-installer-oss)
(choose *Qt 6.8.3 → MSVC 2022 64-bit*, *Qt Multimedia*, *Qt Speech*), or from
the command line:

```powershell
pip install aqtinstall
aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 -m qtmultimedia qtspeech -O C:\Qt
```

### Build, test, run

Run these from an **x64 Native Tools Command Prompt for VS 2022** (or a
"Developer PowerShell"):

```powershell
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64
cmake --build build
ctest --test-dir build --output-on-failure        # set QT_QPA_PLATFORM=offscreen to run headless

# Self-contained app folder: exe + Qt DLLs + plugins + FFmpeg + MSVC runtime (runs windeployqt)
cmake --install build --prefix dist\SmartCantoneseTranslator
dist\SmartCantoneseTranslator\SmartCantoneseTranslator.exe
```

To run the exe straight from `build\src\app` during development, put
`C:\Qt\6.8.3\msvc2022_64\bin` on `PATH`. Qt Creator and Visual Studio (*Open
Folder*) do this for you.

### Installer

```powershell
& "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" /DAppVersion=1.0.0 `
    "/DStageDir=$PWD\dist\SmartCantoneseTranslator" "/DOutputDir=$PWD\dist" `
    packaging\windows\installer.iss
```

This creates `dist\SmartCantoneseTranslator-Setup-1.0.0.exe`. It is a per-user
install that needs no admin rights, with an optional "install for all users"
choice.

### What `cmake --install` deploys

`src/app/CMakeLists.txt` uses Qt's `qt_generate_deploy_script` /
`qt_deploy_runtime_dependencies` (which runs `windeployqt`) and CMake's
`InstallRequiredSystemLibraries`. The result is a flat folder:

| Path | Contents |
|---|---|
| `SmartCantoneseTranslator.exe`, `qt.conf` | the app. `qt.conf` points Qt at `plugins\` |
| `Qt6*.dll` | Qt libraries |
| `avcodec-*.dll` … `swscale-*.dll` | FFmpeg, used by Qt Multimedia to play Azure audio |
| `vcruntime140*.dll`, `msvcp140*.dll` | MSVC runtime (app-local, no VC++ redist install needed) |
| `plugins\platforms\qwindows.dll` | Windows platform plugin |
| `plugins\styles\`, `imageformats\`, `iconengines\` | style, SVG image and SVG icon support |
| `plugins\tls\qschannelbackend.dll` | HTTPS through Windows' own TLS (no OpenSSL needed) |
| `plugins\networkinformation\` | network reachability |
| `plugins\texttospeech\qtexttospeech_winrt.dll`, `..._sapi.dll` | Windows voices (OneCore/WinRT and SAPI) |
| `plugins\multimedia\ffmpegmediaplugin.dll`, `windowsmediaplugin.dll` | audio playback back-ends |
| `translations\qt_*.qm` | Qt's own strings (en, zh_TW, zh_CN) |
| `licenses\`, `THIRD_PARTY_NOTICES.md`, `README.md` | notices |

The test-only `qtexttospeech_mock` engine, the D3D/DXC shader compilers and the
software OpenGL renderer are left out on purpose. The app is Widgets-only.

## Linux (development build)

Ubuntu 24.04 ships Qt 6.4, which is enough to build and run the tests:

```bash
sudo apt-get install cmake ninja-build g++ \
    qt6-base-dev qt6-base-dev-tools qt6-svg-dev qt6-speech-dev \
    qt6-multimedia-dev qt6-declarative-dev libgl1-mesa-dev libxkbcommon-dev
# Optional, to hear voices: speech-dispatcher plus a Cantonese voice, e.g.
#   sudo apt-get install qt6-speech-speechd-plugin speech-dispatcher espeak-ng

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure
./build/src/app/SmartCantoneseTranslator
```

API keys are stored with reversible obfuscation on Linux, not DPAPI. Linux
builds are for development only.

## Continuous integration

- `.github/workflows/windows.yml` runs on every push, pull request, `v*` tag
  and manual trigger. It installs Qt 6.8.3 (`win64_msvc2022_64` with
  `qtmultimedia qtspeech`) via `jurplel/install-qt-action`, builds with MSVC
  and Ninja, runs ctest, runs `cmake --install`, checks that every required DLL
  and plugin was deployed, and starts the app once with no Qt on `PATH`. It then
  uploads two artifacts:
  - `SmartCantoneseTranslator-Setup-<version>.exe`
  - `SmartCantoneseTranslator-<version>-win64-portable.zip`
- `.github/workflows/linux-ci.yml` builds and tests on Ubuntu 24.04 with the
  distro Qt, as a quick check.

## Making a release

1. Bump `VERSION` in `project(...)` in the top-level `CMakeLists.txt` (for
   example `1.1.0`) and commit.
2. Tag the commit and push the tag:
   `git tag v1.1.0 && git push origin v1.1.0`.
3. The Windows workflow builds the release and publishes a GitHub Release with
   the installer and the portable zip. If the tag doesn't match the CMake
   version, the workflow stops with an error.

## App icon

`resources/icons/app.svg` is the master icon. `packaging/icons/app-small.svg` is
a simplified version for 16 and 24 px. After editing either one, regenerate the
PNG and ICO:

```bash
pip install cairosvg pillow
python packaging/icons/render_icons.py
```
