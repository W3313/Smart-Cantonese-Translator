#include "tts/AzureSupport.h"

#include <QCoreApplication>

namespace sct::azure {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("AzureSpeech", text);
}

VoiceInfo voice(const char *id, const QString &name, const char *locale, const char *gender)
{
    return VoiceInfo{QString::fromLatin1(id), name, QString::fromLatin1(locale),
                     QString::fromLatin1(gender)};
}

} // namespace

QString normalizeRegion(const QString &region)
{
    QString r = region.trimmed().toLower();
    if (r.contains(QLatin1String("://"))) {
        const QUrl url(r);
        r = url.host();
    }
    if (r.contains(QLatin1Char('.'))) {
        // A host such as "eastasia.api.cognitive.microsoft.com" or
        // "eastasia.tts.speech.microsoft.com". Custom-domain endpoints
        // ("<resource>.cognitiveservices.azure.com") do not name a region.
        if (r.endsWith(QLatin1String(".cognitiveservices.azure.com")))
            return QString();
        r = r.section(QLatin1Char('.'), 0, 0);
    }
    r.remove(QLatin1Char(' '));
    r.remove(QLatin1Char('-'));
    r.remove(QLatin1Char('_'));
    if (r.isEmpty())
        return QString();
    for (const QChar c : std::as_const(r)) {
        const bool ok = (c >= QLatin1Char('a') && c <= QLatin1Char('z'))
                        || (c >= QLatin1Char('0') && c <= QLatin1Char('9'));
        if (!ok)
            return QString();
    }
    return r;
}

QUrl synthesisUrl(const QString &region)
{
    const QString r = normalizeRegion(region);
    if (r.isEmpty())
        return QUrl();
    return QUrl(QStringLiteral("https://%1.tts.speech.microsoft.com/cognitiveservices/v1").arg(r));
}

QUrl voicesListUrl(const QString &region)
{
    const QString r = normalizeRegion(region);
    if (r.isEmpty())
        return QUrl();
    return QUrl(
        QStringLiteral("https://%1.tts.speech.microsoft.com/cognitiveservices/voices/list").arg(r));
}

QList<VoiceInfo> voiceCatalog(Language lang)
{
    if (lang == Language::Cantonese) {
        return {
            voice("zh-HK-HiuMaanNeural", tr("HiuMaan (female)"), "zh-HK", "Female"),
            voice("zh-HK-HiuGaaiNeural", tr("HiuGaai (female)"), "zh-HK", "Female"),
            voice("zh-HK-WanLungNeural", tr("WanLung (male)"), "zh-HK", "Male"),
        };
    }
    return {
        voice("en-US-AvaMultilingualNeural", tr("Ava (US, female)"), "en-US", "Female"),
        voice("en-US-AndrewMultilingualNeural", tr("Andrew (US, male)"), "en-US", "Male"),
        voice("en-US-EmmaMultilingualNeural", tr("Emma (US, female)"), "en-US", "Female"),
        voice("en-US-BrianMultilingualNeural", tr("Brian (US, male)"), "en-US", "Male"),
        voice("en-US-JennyNeural", tr("Jenny (US, female)"), "en-US", "Female"),
        voice("en-US-GuyNeural", tr("Guy (US, male)"), "en-US", "Male"),
        voice("en-US-AriaNeural", tr("Aria (US, female)"), "en-US", "Female"),
        voice("en-GB-SoniaNeural", tr("Sonia (UK, female)"), "en-GB", "Female"),
        voice("en-GB-RyanNeural", tr("Ryan (UK, male)"), "en-GB", "Male"),
    };
}

QString defaultVoice(Language lang)
{
    return lang == Language::Cantonese ? QStringLiteral("zh-HK-HiuMaanNeural")
                                       : QStringLiteral("en-US-AvaMultilingualNeural");
}

QString resolveVoice(Language lang, const QString &requested)
{
    const QString v = requested.trimmed();
    if (v.isEmpty() || v.contains(QLatin1Char('<')) || v.contains(QLatin1Char('"')))
        return defaultVoice(lang);
    const bool ok = lang == Language::Cantonese
                        ? (v.startsWith(QLatin1String("zh-HK-"), Qt::CaseInsensitive)
                           || v.startsWith(QLatin1String("yue-"), Qt::CaseInsensitive))
                        : v.startsWith(QLatin1String("en-"), Qt::CaseInsensitive);
    return ok ? v : defaultVoice(lang);
}

QString sampleText(const QString &voiceName)
{
    const QString v = voiceName.trimmed();
    if (v.startsWith(QLatin1String("zh-"), Qt::CaseInsensitive)
        || v.startsWith(QLatin1String("yue-"), Qt::CaseInsensitive))
        return QStringLiteral("你好，歡迎使用智能廣東話翻譯！");
    return QStringLiteral("Hello! Welcome to Smart Cantonese Translator.");
}

Failure describeFailure(int httpStatus, QNetworkReply::NetworkError error, const QString &region,
                        const QString &errorString)
{
    const QString r = normalizeRegion(region).isEmpty() ? region.trimmed() : normalizeRegion(region);

    if (httpStatus == 401 || httpStatus == 403) {
        return {FailureKind::Auth,
                tr("Azure rejected the Speech key (HTTP %1). Check that the key is correct and that "
                   "the region (“%2”) matches your Speech resource.")
                    .arg(QString::number(httpStatus), r)};
    }
    if (httpStatus == 429) {
        return {FailureKind::RateLimited,
                tr("Azure Speech is busy or your quota is used up (HTTP 429). Wait a moment and try "
                   "again; the free tier allows a limited number of characters per month.")};
    }
    if (httpStatus == 400 || httpStatus == 415) {
        return {FailureKind::BadRequest,
                tr("Azure Speech could not process the request (HTTP %1). The selected voice may not "
                   "be available in region “%2”.")
                    .arg(QString::number(httpStatus), r)};
    }
    if (httpStatus == 404) {
        return {FailureKind::BadRequest,
                tr("Azure Speech endpoint not found (HTTP 404). Check the region name (“%1”).")
                    .arg(r)};
    }
    if (httpStatus == 408) {
        return {FailureKind::Timeout,
                tr("Azure Speech took too long to respond. Check your internet connection and try "
                   "again.")};
    }
    if (httpStatus >= 500) {
        return {FailureKind::Server,
                tr("Azure Speech service error (HTTP %1). Please try again later.").arg(httpStatus)};
    }
    if (httpStatus >= 300) {
        return {FailureKind::Server, tr("Azure Speech request failed (HTTP %1).").arg(httpStatus)};
    }

    switch (error) {
    case QNetworkReply::NoError:
        return {};
    case QNetworkReply::OperationCanceledError:  // transfer timeout aborts the reply
    case QNetworkReply::TimeoutError:
        return {FailureKind::Timeout,
                tr("Azure Speech did not respond within %1 seconds. Check your internet connection "
                   "and try again.")
                    .arg(TransferTimeoutMs / 1000)};
    case QNetworkReply::HostNotFoundError:
        return {FailureKind::Network,
                tr("Can't reach Azure Speech. You may be offline, or “%1” is not a valid Azure "
                   "region.")
                    .arg(r)};
    case QNetworkReply::SslHandshakeFailedError:
    case QNetworkReply::TooManyRedirectsError:
    case QNetworkReply::InsecureRedirectError:
        return {FailureKind::Network,
                tr("Could not open a secure connection to Azure Speech (%1).").arg(errorString)};
    default:
        break;
    }
    const QString detail = errorString.isEmpty() ? QString() : QStringLiteral(" (%1)").arg(errorString);
    return {FailureKind::Network,
            tr("Can't connect to Azure Speech - you appear to be offline%1.").arg(detail)};
}

} // namespace sct::azure
