# Third-party notices

Smart Cantonese Translator for Windows ships with the third-party components
listed below. Each one stays under its own license. The license texts are in
the `licenses` folder next to the app (in the source tree: `packaging/licenses/`).

## Qt 6

- **What:** Qt Core, Gui, Widgets, Network, Svg, Multimedia and TextToSpeech
  (Qt Speech), their plugins, and the Qt translation files. Release builds use
  Qt 6.8.3.
- **Copyright:** The Qt Company Ltd. and other contributors.
- **License:** GNU Lesser General Public License v3
  ([`LGPL-3.0-only.txt`](packaging/licenses/LGPL-3.0-only.txt), which extends
  the GNU GPL v3, [`GPL-3.0-only.txt`](packaging/licenses/GPL-3.0-only.txt)).
- **How it's used:** Qt is linked **dynamically**. The Qt libraries are the
  unmodified `Qt6*.dll` files and the `plugins` folder next to
  `SmartCantoneseTranslator.exe`. You can swap them for your own build of the
  same Qt version.
- **Source code:** <https://download.qt.io/archive/qt/6.8/6.8.3/single/> and
  <https://code.qt.io/> (mirrored at <https://github.com/qt>).
- Qt itself contains third-party code (for example zlib, libpng, libjpeg,
  HarfBuzz, FreeType and PCRE2) under permissive licenses. The full list is in
  the Qt documentation under "Licenses Used in Qt"
  (<https://doc.qt.io/qt-6.8/licenses-used-in-qt.html>).

## FFmpeg

- **What:** The FFmpeg libraries (`avcodec-*.dll`, `avformat-*.dll`,
  `avutil-*.dll`, `swresample-*.dll` and `swscale-*.dll`) that Qt Multimedia uses
  to decode audio. The DLLs are the ones the Qt Company ships with Qt
  Multimedia 6.8 (FFmpeg 7.1), unmodified.
- **Copyright:** The FFmpeg developers.
- **License:** GNU Lesser General Public License v2.1 or later
  ([`LGPL-2.1-or-later.txt`](packaging/licenses/LGPL-2.1-or-later.txt)). The Qt
  build uses only LGPL parts of FFmpeg, with no GPL or "nonfree" parts.
- **How it's used:** Dynamically loaded by Qt's `ffmpegmediaplugin.dll`. You can
  replace the DLLs with your own compatible build.
- **Source code:** <https://ffmpeg.org/download.html> (release 7.1).

## Microsoft Visual C++ Runtime

`vcruntime140*.dll`, `msvcp140*.dll` and `concrt140.dll` are shipped unchanged
as "Distributable Code" under the Microsoft Visual Studio license terms.

## Noto Sans CJK (app icon)

The "粵" and "A" letterforms in the app icon (`resources/icons/app.svg`) are
outlines taken from **Noto Sans CJK HK Bold**, © 2014-2021 Adobe
(<http://www.adobe.com/>), with Reserved Font Name 'Source'. Noto Sans CJK is
licensed under the SIL Open Font License, Version 1.1
(<https://openfontlicense.org>). The font itself is not included.

## Online services (not bundled)

The app does not include any code from Anthropic, OpenAI or Microsoft Azure. It
sends requests to those services only when you set them up with your own API
key. Their own terms of service apply.

---

**Source code requests.** To get the exact source code of any LGPL component
in a specific release, open an issue at
<https://github.com/w3313/smart-cantonese-translator/issues>. We will provide it
for at least three years after that release.
