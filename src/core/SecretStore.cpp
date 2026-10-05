#include "core/SecretStore.h"

#include <QStringDecoder>
#include <QtGlobal>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <wincrypt.h>
#endif

namespace sct::SecretStore {

namespace {

constexpr char kDpapiPrefix[] = "dpapi:";
constexpr char kObfPrefix[] = "obf:";

// Header prepended to the plaintext before obfuscation so unprotect() can tell
// a real secret from random base64.
constexpr char kObfMagic[] = "SCT1";

// Fixed key for the non-Windows fallback. This is obfuscation, NOT encryption:
// anyone with the settings file and this source code can recover the secret.
// The app only ships for Windows, where DPAPI is used instead.
constexpr char kObfKey[] = "SmartCantoneseTranslator/obfuscation-key/not-a-real-secret";

QByteArray xorWithKey(const QByteArray &in)
{
    QByteArray out = in;
    const qsizetype keyLen = qsizetype(sizeof(kObfKey) - 1);
    for (qsizetype i = 0; i < out.size(); ++i)
        out[i] = char(out.at(i) ^ kObfKey[i % keyLen]);
    return out;
}

bool decodeBase64(const QByteArray &in, QByteArray *out)
{
    const auto res = QByteArray::fromBase64Encoding(in, QByteArray::AbortOnBase64DecodingErrors);
    if (!res)
        return false;
    *out = res.decoded;
    return true;
}

// Strict UTF-8 decode; returns false on invalid sequences.
bool decodeUtf8(const QByteArray &bytes, QString *out)
{
    QStringDecoder decoder(QStringDecoder::Utf8, QStringDecoder::Flag::Stateless);
    QString s = decoder.decode(bytes);
    if (decoder.hasError())
        return false;
    *out = s;
    return true;
}

#ifndef Q_OS_WIN
QByteArray obfuscate(const QString &plain)
{
    QByteArray data = QByteArray(kObfMagic) + plain.toUtf8();
    return QByteArray(kObfPrefix) + xorWithKey(data).toBase64();
}
#endif

QString deobfuscate(const QByteArray &payload)
{
    QByteArray raw;
    if (!decodeBase64(payload, &raw))
        return {};
    const QByteArray data = xorWithKey(raw);
    const QByteArray magic(kObfMagic);
    if (!data.startsWith(magic))
        return {};
    QString s;
    if (!decodeUtf8(data.mid(magic.size()), &s))
        return {};
    return s;
}

#ifdef Q_OS_WIN
// Optional entropy ties the blob to this application in addition to the
// Windows user account.
constexpr char kDpapiEntropy[] = "SmartCantoneseTranslator.SecretStore.v1";

QByteArray dpapiProtect(const QString &plain)
{
    QByteArray utf8 = plain.toUtf8();

    DATA_BLOB input;
    input.pbData = reinterpret_cast<BYTE *>(utf8.data());
    input.cbData = static_cast<DWORD>(utf8.size());

    QByteArray entropyBytes(kDpapiEntropy);
    DATA_BLOB entropy;
    entropy.pbData = reinterpret_cast<BYTE *>(entropyBytes.data());
    entropy.cbData = static_cast<DWORD>(entropyBytes.size());

    DATA_BLOB output;
    output.pbData = nullptr;
    output.cbData = 0;

    const BOOL ok = CryptProtectData(&input, L"Smart Cantonese Translator secret", &entropy,
                                     nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output);
    SecureZeroMemory(utf8.data(), static_cast<SIZE_T>(utf8.size()));
    if (!ok || output.pbData == nullptr)
        return {};

    const QByteArray encrypted(reinterpret_cast<const char *>(output.pbData),
                               static_cast<qsizetype>(output.cbData));
    LocalFree(output.pbData);
    return QByteArray(kDpapiPrefix) + encrypted.toBase64();
}

QString dpapiUnprotect(const QByteArray &payload)
{
    QByteArray encrypted;
    if (!decodeBase64(payload, &encrypted) || encrypted.isEmpty())
        return {};

    DATA_BLOB input;
    input.pbData = reinterpret_cast<BYTE *>(encrypted.data());
    input.cbData = static_cast<DWORD>(encrypted.size());

    QByteArray entropyBytes(kDpapiEntropy);
    DATA_BLOB entropy;
    entropy.pbData = reinterpret_cast<BYTE *>(entropyBytes.data());
    entropy.cbData = static_cast<DWORD>(entropyBytes.size());

    DATA_BLOB output;
    output.pbData = nullptr;
    output.cbData = 0;

    const BOOL ok = CryptUnprotectData(&input, nullptr, &entropy, nullptr, nullptr,
                                       CRYPTPROTECT_UI_FORBIDDEN, &output);
    if (!ok || output.pbData == nullptr)
        return {};

    const QByteArray decrypted(reinterpret_cast<const char *>(output.pbData),
                               static_cast<qsizetype>(output.cbData));
    SecureZeroMemory(output.pbData, output.cbData);
    LocalFree(output.pbData);

    QString s;
    if (!decodeUtf8(decrypted, &s))
        return {};
    return s;
}
#endif

} // namespace

QByteArray protect(const QString &plain)
{
    if (plain.isEmpty())
        return {};
#ifdef Q_OS_WIN
    return dpapiProtect(plain);
#else
    return obfuscate(plain);
#endif
}

QString unprotect(const QByteArray &stored)
{
    const QByteArray data = stored.trimmed();
    if (data.isEmpty())
        return {};

    if (data.startsWith(kObfPrefix))
        return deobfuscate(data.mid(qsizetype(sizeof(kObfPrefix) - 1)));

    if (data.startsWith(kDpapiPrefix)) {
#ifdef Q_OS_WIN
        return dpapiUnprotect(data.mid(qsizetype(sizeof(kDpapiPrefix) - 1)));
#else
        return {};  // DPAPI blobs can only be opened on Windows by the same user.
#endif
    }
    return {};
}

} // namespace sct::SecretStore
