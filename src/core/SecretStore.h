#pragma once

#include <QByteArray>
#include <QString>

namespace sct::SecretStore {

// Encrypts a secret (API key) for storage in settings.
// Windows: DPAPI (CryptProtectData, current-user scope) -> only this Windows
// account can decrypt it. Other platforms: reversible obfuscation (documented
// as not secure; the app only ships for Windows).
// Returned bytes are base64 text prefixed with a scheme tag ("dpapi:" / "obf:").
QByteArray protect(const QString &plain);

// Reverses protect(). Returns an empty string on failure or empty input.
QString unprotect(const QByteArray &stored);

} // namespace sct::SecretStore
