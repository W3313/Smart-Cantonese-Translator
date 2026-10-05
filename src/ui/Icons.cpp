#include "ui/Icons.h"

#include "ui/Theme.h"

#include <QFile>
#include <QHash>
#include <QIconEngine>
#include <QPainter>
#include <QPainterPath>
#include <QPixmapCache>
#include <QSvgRenderer>

namespace sct::ui {

namespace {

QByteArray svgTemplate(const QString &name)
{
    static QHash<QString, QByteArray> cache;
    auto it = cache.constFind(name);
    if (it != cache.constEnd())
        return it.value();
    QByteArray data;
    QFile f(QStringLiteral(":/icons/%1.svg").arg(name));
    if (f.open(QIODevice::ReadOnly))
        data = f.readAll();
    cache.insert(name, data);
    return data;
}

QPixmap renderSvg(const QString &name, const QColor &color, const QSize &deviceSize, qreal dpr)
{
    if (deviceSize.isEmpty())
        return {};
    // QPixmapCache (not a static container): Qt releases it before the
    // application object goes away.
    const QString key = QStringLiteral("sct-icon|%1|%2|%3x%4|%5")
                            .arg(name, color.name(QColor::HexArgb))
                            .arg(deviceSize.width())
                            .arg(deviceSize.height())
                            .arg(dpr);
    QPixmap cached;
    if (QPixmapCache::find(key, &cached))
        return cached;

    QByteArray svg = svgTemplate(name);
    QPixmap pm(deviceSize);
    pm.fill(Qt::transparent);
    if (!svg.isEmpty()) {
        svg.replace("currentColor", color.name(QColor::HexRgb).toLatin1());
        QSvgRenderer renderer(svg);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        // Keep the icon square and centred.
        const int side = qMin(deviceSize.width(), deviceSize.height());
        const QRectF target((deviceSize.width() - side) / 2.0, (deviceSize.height() - side) / 2.0, side, side);
        renderer.render(&p, target);
        if (color.alpha() < 255) {
            p.setCompositionMode(QPainter::CompositionMode_DestinationIn);
            p.fillRect(pm.rect(), QColor(0, 0, 0, color.alpha()));
        }
    }
    pm.setDevicePixelRatio(dpr);
    QPixmapCache::insert(key, pm);
    return pm;
}

class TintedSvgIconEngine : public QIconEngine
{
public:
    TintedSvgIconEngine(const QString &name, IconTone tone)
        : m_name(name)
        , m_tone(tone)
    {
    }

    QColor colorFor(QIcon::Mode mode, QIcon::State state) const
    {
        const ThemeColors &c = Theme::colors();
        switch (mode) {
        case QIcon::Disabled:
            return c.textDisabled;
        case QIcon::Selected:
            return m_tone == IconTone::Star ? c.star : c.onAccent;
        default:
            break;
        }
        if (state == QIcon::On && m_tone == IconTone::Text)
            return c.accent;
        return toneColor(m_tone);
    }

    void paint(QPainter *painter, const QRect &rect, QIcon::Mode mode, QIcon::State state) override
    {
        const qreal dpr = painter->device() ? painter->device()->devicePixelRatioF() : 1.0;
        const QPixmap pm = renderSvg(m_name, colorFor(mode, state), rect.size() * dpr, dpr);
        painter->drawPixmap(rect, pm);
    }

    QPixmap pixmap(const QSize &size, QIcon::Mode mode, QIcon::State state) override
    {
        QPixmap pm = renderSvg(m_name, colorFor(mode, state), size, 1.0);
        pm.setDevicePixelRatio(1.0);
        return pm;
    }

    QPixmap scaledPixmap(const QSize &size, QIcon::Mode mode, QIcon::State state, qreal scale) override
    {
        if (scale <= 0)
            scale = 1.0;
        return renderSvg(m_name, colorFor(mode, state), size * scale, scale);
    }

    QSize actualSize(const QSize &size, QIcon::Mode, QIcon::State) override { return size; }

    QIconEngine *clone() const override { return new TintedSvgIconEngine(m_name, m_tone); }

    QString key() const override { return QStringLiteral("sct-tinted-svg"); }

    bool isNull() override { return svgTemplate(m_name).isEmpty(); }

private:
    QString m_name;
    IconTone m_tone;
};

void paintFallbackLogo(QPainter *p, const QRectF &r)
{
    p->setRenderHint(QPainter::Antialiasing);
    QLinearGradient g(r.topLeft(), r.bottomRight());
    g.setColorAt(0, QColor(0xFF, 0x7A, 0x45));
    g.setColorAt(1, QColor(0xB0, 0x12, 0x4A));
    QPainterPath path;
    path.addRoundedRect(r, r.width() * 0.22, r.height() * 0.22);
    p->fillPath(path, g);
    QFont f = Theme::textFont(sct::Language::Cantonese, 10);
    f.setPixelSize(qMax(6, int(r.height() * 0.62)));
    f.setWeight(QFont::Bold);
    p->setFont(f);
    p->setPen(Qt::white);
    p->drawText(r, Qt::AlignCenter, QStringLiteral("粵"));
}

} // namespace

QColor toneColor(IconTone tone)
{
    const ThemeColors &c = Theme::colors();
    switch (tone) {
    case IconTone::Text:
        return c.text;
    case IconTone::Muted:
        return c.textMuted;
    case IconTone::Accent:
        return c.accent;
    case IconTone::OnAccent:
        return c.onAccent;
    case IconTone::Star:
        return c.star;
    case IconTone::Warning:
        return c.warnText;
    case IconTone::Error:
        return c.errorText;
    case IconTone::Info:
        return c.infoText;
    case IconTone::Success:
        return c.successText;
    }
    return c.text;
}

QIcon icon(const QString &name, IconTone tone)
{
    return QIcon(new TintedSvgIconEngine(name, tone));
}

QPixmap iconPixmap(const QString &name, IconTone tone, int size, qreal dpr)
{
    return iconPixmap(name, toneColor(tone), size, dpr);
}

QPixmap iconPixmap(const QString &name, const QColor &color, int size, qreal dpr)
{
    if (dpr <= 0)
        dpr = 1.0;
    const int device = qRound(size * dpr);
    return renderSvg(name, color, QSize(device, device), dpr);
}

QIcon appIcon()
{
    QIcon i;
    // The .ico has hand-tuned small sizes (a downscaled SVG smudges the 粵
    // glyph at 16 px). Loading it needs the "ico" image-format plugin.
    if (QFile::exists(QStringLiteral(":/icons/app.ico")))
        i.addFile(QStringLiteral(":/icons/app.ico"));
    if (QFile::exists(QStringLiteral(":/icons/app.png")))
        i.addFile(QStringLiteral(":/icons/app.png"));
    if (i.availableSizes().isEmpty()) {
        // No plugin or no files: rasterise the SVG (or the built-in badge).
        i = QIcon();
        for (int s : {16, 24, 32, 48, 64, 128, 256}) {
            QPixmap pm(s, s);
            pm.fill(Qt::transparent);
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            if (QFile::exists(QStringLiteral(":/icons/app.svg")))
                QSvgRenderer(QStringLiteral(":/icons/app.svg")).render(&p, QRectF(0, 0, s, s));
            else
                paintFallbackLogo(&p, QRectF(0, 0, s, s).adjusted(s * 0.04, s * 0.04, -s * 0.04, -s * 0.04));
            p.end();
            i.addPixmap(pm);
        }
    }
    return i;
}

QPixmap appLogo(int size, qreal dpr)
{
    if (dpr <= 0)
        dpr = 1.0;
    const int device = qRound(size * dpr);
    // Large sizes: the SVG is crisp. Small sizes: the hand-tuned icon images.
    if (device >= 96 && QFile::exists(QStringLiteral(":/icons/app.svg"))) {
        QPixmap pm(device, device);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        QSvgRenderer(QStringLiteral(":/icons/app.svg")).render(&p, QRectF(0, 0, device, device));
        p.end();
        pm.setDevicePixelRatio(dpr);
        return pm;
    }
    QPixmap pm = appIcon().pixmap(QSize(size, size), dpr);
    if (pm.width() != device) {
        pm = pm.scaled(device, device, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        pm.setDevicePixelRatio(dpr);
    }
    return pm;
}

void paintSpinner(QPainter *p, const QRectF &rect, int angle, const QColor &color, qreal penWidth)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    const QRectF r = rect.adjusted(penWidth / 2, penWidth / 2, -penWidth / 2, -penWidth / 2);
    QColor track = color;
    track.setAlphaF(0.18);
    p->setPen(QPen(track, penWidth));
    p->drawEllipse(r);
    p->setPen(QPen(color, penWidth, Qt::SolidLine, Qt::RoundCap));
    // Qt angles are in 1/16 degree, counter-clockwise; spin clockwise.
    p->drawArc(r, (90 - angle) * 16, -100 * 16);
    p->restore();
}

} // namespace sct::ui
