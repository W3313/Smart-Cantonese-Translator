#include "core/TranslationTypes.h"

#include <QJsonArray>
#include <QJsonValue>

namespace sct {

QString languageTag(Language lang)
{
    return lang == Language::Cantonese ? QStringLiteral("zh-HK") : QStringLiteral("en-US");
}

QString languageDisplayName(Language lang)
{
    return lang == Language::Cantonese ? QStringLiteral("Cantonese (廣東話)") : QStringLiteral("English");
}

QString toString(Direction d)
{
    return d == Direction::CantoneseToEnglish ? QStringLiteral("yue2en") : QStringLiteral("en2yue");
}

QString toString(Tone t)
{
    switch (t) {
    case Tone::Casual:
        return QStringLiteral("casual");
    case Tone::Polite:
        return QStringLiteral("polite");
    case Tone::Neutral:
        break;
    }
    return QStringLiteral("neutral");
}

QString toString(ChineseScript s)
{
    return s == ChineseScript::Simplified ? QStringLiteral("simplified") : QStringLiteral("traditional");
}

Direction directionFromString(const QString &s, Direction fallback)
{
    const QString v = s.trimmed().toLower();
    if (v == QLatin1String("en2yue"))
        return Direction::EnglishToCantonese;
    if (v == QLatin1String("yue2en"))
        return Direction::CantoneseToEnglish;
    return fallback;
}

Tone toneFromString(const QString &s, Tone fallback)
{
    const QString v = s.trimmed().toLower();
    if (v == QLatin1String("casual"))
        return Tone::Casual;
    if (v == QLatin1String("neutral"))
        return Tone::Neutral;
    if (v == QLatin1String("polite"))
        return Tone::Polite;
    return fallback;
}

ChineseScript scriptFromString(const QString &s, ChineseScript fallback)
{
    const QString v = s.trimmed().toLower();
    if (v == QLatin1String("traditional"))
        return ChineseScript::Traditional;
    if (v == QLatin1String("simplified"))
        return ChineseScript::Simplified;
    return fallback;
}

// ---- TranslationRequest -----------------------------------------------------

QString TranslationRequest::cacheKey() const
{
    // Surrounding whitespace never changes the translation, so it is ignored.
    return toString(direction) + QLatin1Char('|') + toString(tone) + QLatin1Char('|')
           + toString(script) + QLatin1Char('|') + (wantAlternatives ? QLatin1Char('A') : QLatin1Char('-'))
           + (wantNotes ? QLatin1Char('N') : QLatin1Char('-')) + QLatin1Char('|') + text.trimmed();
}

QJsonObject TranslationRequest::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("text"), text);
    o.insert(QStringLiteral("direction"), toString(direction));
    o.insert(QStringLiteral("tone"), toString(tone));
    o.insert(QStringLiteral("script"), toString(script));
    o.insert(QStringLiteral("wantAlternatives"), wantAlternatives);
    o.insert(QStringLiteral("wantNotes"), wantNotes);
    return o;
}

TranslationRequest TranslationRequest::fromJson(const QJsonObject &obj)
{
    TranslationRequest r;
    r.text = obj.value(QStringLiteral("text")).toString();
    r.direction = directionFromString(obj.value(QStringLiteral("direction")).toString());
    r.tone = toneFromString(obj.value(QStringLiteral("tone")).toString());
    r.script = scriptFromString(obj.value(QStringLiteral("script")).toString());
    r.wantAlternatives = obj.value(QStringLiteral("wantAlternatives")).toBool(true);
    r.wantNotes = obj.value(QStringLiteral("wantNotes")).toBool(true);
    return r;
}

// ---- TranslationResult ------------------------------------------------------

QJsonObject TranslationResult::toJson() const
{
    QJsonObject o;
    o.insert(QStringLiteral("translation"), translation);
    o.insert(QStringLiteral("jyutping"), jyutping);
    o.insert(QStringLiteral("literal"), literal);

    QJsonArray alts;
    for (const Alternative &a : alternatives) {
        QJsonObject ao;
        ao.insert(QStringLiteral("text"), a.text);
        ao.insert(QStringLiteral("jyutping"), a.jyutping);
        ao.insert(QStringLiteral("note"), a.note);
        alts.append(ao);
    }
    o.insert(QStringLiteral("alternatives"), alts);
    o.insert(QStringLiteral("notes"), QJsonArray::fromStringList(notes));

    o.insert(QStringLiteral("providerId"), providerId);
    o.insert(QStringLiteral("model"), model);
    o.insert(QStringLiteral("request"), request.toJson());
    if (timestamp.isValid())
        o.insert(QStringLiteral("timestamp"), timestamp.toUTC().toString(Qt::ISODateWithMs));
    // fromCache is a transient, per-delivery flag and is not persisted.
    return o;
}

TranslationResult TranslationResult::fromJson(const QJsonObject &obj)
{
    TranslationResult r;
    r.translation = obj.value(QStringLiteral("translation")).toString();
    r.jyutping = obj.value(QStringLiteral("jyutping")).toString();
    r.literal = obj.value(QStringLiteral("literal")).toString();

    const QJsonArray alts = obj.value(QStringLiteral("alternatives")).toArray();
    for (const QJsonValue &v : alts) {
        const QJsonObject ao = v.toObject();
        Alternative a;
        a.text = ao.value(QStringLiteral("text")).toString();
        a.jyutping = ao.value(QStringLiteral("jyutping")).toString();
        a.note = ao.value(QStringLiteral("note")).toString();
        if (!a.text.isEmpty())
            r.alternatives.append(a);
    }
    const QJsonArray notes = obj.value(QStringLiteral("notes")).toArray();
    for (const QJsonValue &v : notes) {
        const QString n = v.toString();
        if (!n.isEmpty())
            r.notes.append(n);
    }

    r.providerId = obj.value(QStringLiteral("providerId")).toString();
    r.model = obj.value(QStringLiteral("model")).toString();
    r.request = TranslationRequest::fromJson(obj.value(QStringLiteral("request")).toObject());

    const QString ts = obj.value(QStringLiteral("timestamp")).toString();
    if (!ts.isEmpty()) {
        QDateTime dt = QDateTime::fromString(ts, Qt::ISODateWithMs);
        if (!dt.isValid())
            dt = QDateTime::fromString(ts, Qt::ISODate);
        if (dt.isValid())
            r.timestamp = dt.toUTC();
    }
    r.fromCache = false;
    return r;
}

void registerMetaTypes()
{
    qRegisterMetaType<sct::TranslationRequest>();
    qRegisterMetaType<sct::TranslationResult>();
    qRegisterMetaType<sct::TranslationError>();
}

} // namespace sct
