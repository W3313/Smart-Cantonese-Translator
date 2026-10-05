<p align="center">
  <img src="resources/icons/app.png" width="128" height="128" alt="Smart Cantonese Translator icon">
</p>

<h1 align="center">Smart Cantonese Translator</h1>

<p align="center">
  An English ↔ Cantonese translator for Windows that writes the way Hong Kong people
  actually <em>speak</em>, and reads it aloud for you.
</p>

<p align="center">
  <a href="https://github.com/w3313/smart-cantonese-translator/releases/latest"><b>Download for Windows</b></a>
  ·
  <a href="#getting-an-ai-api-key">Get an API key</a>
  ·
  <a href="#getting-a-cantonese-voice">Get a Cantonese voice</a>
</p>

---

## What it does

Type or paste English and get natural, spoken Cantonese, or go the other way and
turn Cantonese into English. Every translation can include:

- **Real spoken Cantonese.** You get the words people really use, like 嘅, 咗, 喺, 佢, 唔, 冇 and 嘢. You don't get the formal written Chinese most translators produce.
- **Jyutping** romanization under the Chinese, so you can pronounce it even if you can't read the characters.
- **Tone control.** Choose *casual* (friends and family), *neutral* or *polite* (work, elders, customers).
- **Alternatives and notes.** See other ways to say the same thing, plus short notes on slang, politeness and how the words are used.
- **Read aloud.** Hear the translation spoken by a Cantonese voice from Windows or from Microsoft Azure.
- **Traditional or Simplified** characters, and a **history** of your recent translations.

### Why it's different

Most translation apps turn English into *written* Chinese. That's close to
Mandarin, and it sounds stiff or odd when read aloud in Cantonese. This app asks
a state-of-the-art AI model (Claude or ChatGPT) for colloquial Hong Kong Cantonese:

| English | Typical translator (written Chinese) | Smart Cantonese Translator |
|---|---|---|
| Where are you going? | 你要去哪裡？ | 你去邊度呀？ *(nei5 heoi3 bin1 dou6 aa3)* |
| I've already eaten. | 我已經吃過飯了。 | 我食咗飯喇。 *(ngo5 sik6 zo2 faan6 laa3)* |
| He's at home. | 他在家。 | 佢喺屋企。 *(keoi5 hai2 uk1 kei2)* |
| This one is mine. | 這個是我的。 | 呢個係我嘅。 *(ni1 go3 hai6 ngo5 ge3)* |

## Download & install

**Requirements:** Windows 10 (version 1809 or newer) or Windows 11, 64-bit, and an
internet connection.

1. Go to the [**Releases page**](https://github.com/w3313/smart-cantonese-translator/releases/latest).
2. Under **Assets**, download **`SmartCantoneseTranslator-Setup-<version>.exe`**.
3. Double-click it and follow the steps. You don't need administrator rights,
   because by default it installs just for you. It adds a Start-menu entry and,
   if you tick the box, a desktop shortcut.

**"Windows protected your PC"?** The app isn't code-signed yet (a signing
certificate costs money), so Microsoft Defender SmartScreen may warn you the
first time. Click **More info**, then **Run anyway**. It only asks once.

**Portable version:** if you'd rather not install anything, download
`SmartCantoneseTranslator-<version>-win64-portable.zip`, unzip it anywhere (for
example to a USB stick), and run `SmartCantoneseTranslator.exe`.

**Latest test build:** every change is built automatically. On the
[Actions page](https://github.com/w3313/smart-cantonese-translator/actions/workflows/windows.yml),
open the most recent green run and download the installer from **Artifacts**.
This needs a GitHub login. These builds are not as well tested as releases.

To **uninstall**, open Windows *Settings → Apps*, find **Smart Cantonese
Translator** and choose **Uninstall**.

## Getting an AI API key

The translations come from an AI model, so you need your own API key from
**Anthropic (Claude)** or **OpenAI (ChatGPT)**. Both are pay-as-you-go: you
buy a small amount of credit, and each translation uses a small part of it
(typically a few cents or less, depending on the model and the length of the text). Claude is the default and is recommended for
the most natural Cantonese.

### Claude (Anthropic), recommended

1. Go to <https://console.anthropic.com> and create an account.
2. Open **Settings → Billing** and add some credit, for example US$5.
3. Open **API keys** and click **Create Key**. Give it a name like
   "Cantonese translator".
4. Copy the key. It starts with `sk-ant-`. You can only see it once, so paste
   it straight into the app.

### ChatGPT (OpenAI)

1. Go to <https://platform.openai.com/api-keys> and sign in or create an account.
2. Add credit under **Settings → Billing**.
3. Click **Create new secret key** and copy it. It starts with `sk-`.

### Add the key to the app

Open the app's **Settings**, choose your AI provider, paste the key, and click
**OK**. Treat the key like a password. You can set a monthly spending limit in
the Anthropic or OpenAI console.

## Getting a Cantonese voice

To hear translations read aloud, you need a Cantonese voice. There are two options.

### Option 1: Free Windows voice (works offline)

**Windows 11:**

1. Open **Settings → Time & language → Speech**.
2. Next to **Manage voices**, click **Add voices**.
3. Search for **Chinese (Traditional, Hong Kong SAR)** and click **Add**.

**Windows 10:**

1. Open **Settings → Time & Language → Speech → Add voices**.
2. Choose **Chinese (Traditional, Hong Kong SAR)**.

Then restart Smart Cantonese Translator. Windows adds the Hong Kong voices
*Microsoft Tracy* and *Microsoft Danny*. Choose one under the app's **Settings →
Speech**.

### Option 2: Microsoft Azure neural voices (most natural)

Azure's neural voices sound much more human, and the free tier is generous:
about half a million characters per month at the time of writing.

1. Sign in to the [Azure portal](https://portal.azure.com). A free account works.
2. Click **Create a resource**, search for **Speech**, and create a *Speech service*.
   - **Region:** pick one near you, for example *East Asia* (Hong Kong).
   - **Pricing tier:** **Free F0**.
3. When it's ready, open the resource and go to **Keys and Endpoint**. Copy
   **KEY 1** and the **Location/Region** (for example `eastasia`).
4. In the app, open **Settings → Speech**, choose **Azure**, and paste the key
   and region.

Good Cantonese voices include **zh-HK-HiuMaanNeural** (female),
**zh-HK-WanLungNeural** (male) and **zh-HK-HiuGaaiNeural** (female).

## Keyboard shortcuts

<!-- Filled in by the maintainers once the UI is final. -->

| Action | Shortcut |
|---|---|
| Translate | *TBD* |
| Swap direction | *TBD* |
| Read translation aloud | *TBD* |
| Copy translation | *TBD* |
| Settings | *TBD* |

## Privacy

- **Your text goes to the AI provider you choose.** When you translate, the text
  is sent over an encrypted HTTPS connection to Anthropic or OpenAI. Their API
  privacy policies apply. Don't translate anything you wouldn't want to share
  with them.
- **Read-aloud:** Windows voices run entirely on your PC. Azure voices send the
  text being read to Microsoft Azure.
- **API keys are encrypted** with Windows DPAPI. Only your Windows user account
  on this PC can decrypt them. They are never sent anywhere except to the
  service they belong to.
- **Your settings and translation history stay on your PC**, in your Windows
  user profile. The app has no accounts, analytics or telemetry. Uninstalling
  the app leaves your settings and history in place, so they're still there if
  you reinstall.

## Troubleshooting

- **No Cantonese voice in the list:** install the Hong Kong voice (see
  [Getting a Cantonese voice](#getting-a-cantonese-voice)), then restart the app.
- **"Invalid API key" or "401" errors:** copy the key again with no extra spaces,
  and check that your account has credit.
- **Network or SSL errors at work or school:** a firewall or proxy may block
  `api.anthropic.com`, `api.openai.com` or Azure. Try another network.

## Build from source

Developers: see [docs/BUILDING.md](docs/BUILDING.md). In short, on Windows you need
Qt 6.8 (MSVC 2022 64-bit, with the Qt Multimedia and Qt Speech add-ons), Visual
Studio 2022 and CMake + Ninja. On Linux, a development build works with the
distribution's Qt 6 packages.

## License & credits

Smart Cantonese Translator is built with [Qt](https://www.qt.io) (LGPLv3),
and audio playback uses [FFmpeg](https://ffmpeg.org) (LGPL), as shipped with Qt.
See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for details and license
texts.
