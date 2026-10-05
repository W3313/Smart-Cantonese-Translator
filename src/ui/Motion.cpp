#include "ui/Motion.h"

#include "core/Version.h"

#include <QCoreApplication>
#include <QGraphicsOpacityEffect>
#include <QLayout>
#include <QPainter>
#include <QPointer>
#include <QSettings>
#include <QTimer>
#include <QVariantAnimation>

#include <cmath>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace sct {

namespace {

QString g_iniPath;

QString animName(const QString &key) { return QStringLiteral("sct-anim:") + key; }

} // namespace

// ---- UiPrefs -------------------------------------------------------------------

UiPrefs::UiPrefs(QObject *parent)
    : QObject(parent)
{
}

UiPrefs *UiPrefs::instance()
{
    static UiPrefs *s_prefs = nullptr;
    if (!s_prefs)
        s_prefs = new UiPrefs(QCoreApplication::instance());
    return s_prefs;
}

void UiPrefs::useIniFile(const QString &path)
{
    g_iniPath = path;
    UiPrefs *p = instance();
    delete p->m_settings;
    p->m_settings = nullptr;
    p->m_reduceMotionCache = -1;
}

QSettings *UiPrefs::settings() const
{
    if (!m_settings) {
        if (!g_iniPath.isEmpty())
            m_settings = new QSettings(g_iniPath, QSettings::IniFormat, const_cast<UiPrefs *>(this));
        else
            m_settings = new QSettings(QSettings::NativeFormat, QSettings::UserScope, QStringLiteral(SCT_ORG_NAME),
                                       QStringLiteral(SCT_APP_ID), const_cast<UiPrefs *>(this));
    }
    return m_settings;
}

bool UiPrefs::reduceMotion() const
{
    if (m_reduceMotionCache < 0) {
        const QVariant v = settings()->value(QStringLiteral("ui/reduceMotion"));
        m_reduceMotionCache = v.isValid() ? (v.toBool() ? 1 : 0) : (systemPrefersReducedMotion() ? 1 : 0);
    }
    return m_reduceMotionCache == 1;
}

void UiPrefs::setReduceMotion(bool on)
{
    const bool old = reduceMotion();
    settings()->setValue(QStringLiteral("ui/reduceMotion"), on);
    m_reduceMotionCache = on ? 1 : 0;
    if (old != on)
        emit changed();
}

bool UiPrefs::historyVisible() const { return settings()->value(QStringLiteral("ui/historyVisible"), false).toBool(); }

void UiPrefs::setHistoryVisible(bool on)
{
    if (historyVisible() == on)
        return;
    settings()->setValue(QStringLiteral("ui/historyVisible"), on);
    emit changed();
}

bool UiPrefs::systemPrefersReducedMotion()
{
#ifdef Q_OS_WIN
    // Settings > Accessibility > Visual effects > Animation effects.
    BOOL animations = TRUE;
    if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0))
        return animations == FALSE;
#endif
    return false;
}

// ---- motion ------------------------------------------------------------------------

namespace motion {

bool reduced() { return UiPrefs::instance()->reduceMotion(); }

int ms(int base) { return reduced() ? 0 : base; }

QEasingCurve outCubic() { return QEasingCurve(QEasingCurve::OutCubic); }

QEasingCurve inOutCubic() { return QEasingCurve(QEasingCurve::InOutCubic); }

void stop(QObject *owner, const QString &key)
{
    if (!owner)
        return;
    const auto anims = owner->findChildren<QVariantAnimation *>(animName(key), Qt::FindDirectChildrenOnly);
    for (QVariantAnimation *a : anims) {
        a->setObjectName(QString());
        a->disconnect();
        if (a->state() == QAbstractAnimation::Stopped)
            delete a;
        else
            a->stop();  // DeleteWhenStopped
    }
}

void finish(QObject *owner, const QString &key)
{
    if (!owner)
        return;
    const auto anims = owner->findChildren<QVariantAnimation *>(animName(key), Qt::FindDirectChildrenOnly);
    for (QVariantAnimation *a : anims) {
        a->setObjectName(QString());
        if (a->state() == QAbstractAnimation::Stopped)
            a->start(QAbstractAnimation::DeleteWhenStopped);  // delayed: start, then jump
        a->setCurrentTime(a->totalDuration());
    }
}

bool isRunning(QObject *owner, const QString &key)
{
    return owner && !owner->findChildren<QVariantAnimation *>(animName(key), Qt::FindDirectChildrenOnly).isEmpty();
}

QVariantAnimation *animate(QObject *owner, const QString &key, const QVariant &from, const QVariant &to,
                           int durationMs, const std::function<void(const QVariant &)> &onValue,
                           const std::function<void()> &onFinished, const QEasingCurve &curve, int delayMs)
{
    stop(owner, key);
    if (!owner || reduced() || durationMs <= 0) {
        if (onValue)
            onValue(to);
        if (onFinished)
            onFinished();
        return nullptr;
    }
    auto *a = new QVariantAnimation(owner);
    a->setObjectName(animName(key));
    a->setStartValue(from);
    a->setEndValue(to);
    a->setDuration(durationMs);
    a->setEasingCurve(curve);
    if (onValue)
        QObject::connect(a, &QVariantAnimation::valueChanged, owner, onValue);
    QObject::connect(a, &QAbstractAnimation::finished, owner, [a, onFinished] {
        a->setObjectName(QString());
        if (onFinished)
            onFinished();
    });
    if (delayMs > 0) {
        if (onValue)
            onValue(from);
        QTimer::singleShot(delayMs, a, [a] {
            if (!a->objectName().isEmpty())  // cleared by stop()/finish()
                a->start(QAbstractAnimation::DeleteWhenStopped);
        });
    } else {
        a->start(QAbstractAnimation::DeleteWhenStopped);
    }
    return a;
}

void fadeIn(QWidget *w, int durationMs, int delayMs)
{
    if (!w)
        return;
    if (reduced() || durationMs <= 0) {
        stop(w, QStringLiteral("fade"));
        w->setGraphicsEffect(nullptr);
        return;
    }
    auto *effect = new QGraphicsOpacityEffect(w);
    effect->setOpacity(0.0);
    w->setGraphicsEffect(effect);
    QPointer<QGraphicsOpacityEffect> guard(effect);
    QPointer<QWidget> wGuard(w);
    animate(
        w, QStringLiteral("fade"), 0.0, 1.0, durationMs,
        [guard](const QVariant &v) {
            if (guard)
                guard->setOpacity(v.toReal());
        },
        [guard, wGuard] {
            if (guard && wGuard && wGuard->graphicsEffect() == guard)
                wGuard->setGraphicsEffect(nullptr);
        },
        outCubic(), delayMs);
}

void crossFade(QWidget *target, int durationMs)
{
    if (!target || reduced() || durationMs <= 0 || !target->isVisible() || target->size().isEmpty())
        return;
    auto *overlay = new SnapshotOverlay(target->grab(), target);
    overlay->setGeometry(target->rect());
    overlay->show();
    overlay->raise();
    QPointer<SnapshotOverlay> guard(overlay);
    animate(
        overlay, QStringLiteral("crossfade"), 1.0, 0.0, durationMs,
        [guard](const QVariant &v) {
            if (guard)
                guard->setOpacity(v.toReal());
        },
        [guard] {
            if (guard)
                guard->deleteLater();
        },
        inOutCubic());
}

void shake(QWidget *w)
{
    if (!w || reduced() || !w->parentWidget() || !w->parentWidget()->layout())
        return;
    QLayout *layout = w->parentWidget()->layout();
    // Remember the resting margins (packed into a QRect) across overlapping shakes.
    const QVariant stored = layout->property("sct-base-margins");
    const QMargins cur = layout->contentsMargins();
    const QRect packed = stored.isValid() ? stored.toRect() : QRect(cur.left(), cur.top(), cur.right(), cur.bottom());
    layout->setProperty("sct-base-margins", packed);
    const QMargins base(packed.x(), packed.y(), packed.width(), packed.height());
    QPointer<QLayout> guard(layout);
    animate(
        w, QStringLiteral("shake"), 0.0, 1.0, 420,
        [guard, base](const QVariant &v) {
            if (!guard)
                return;
            const qreal t = v.toReal();
            // Damped sine: three small swings.
            const int dx = qRound(std::sin(t * 3.14159265 * 6) * 6.0 * (1.0 - t));
            guard->setContentsMargins(base.left() + dx, base.top(), base.right() - dx, base.bottom());
        },
        [guard, base] {
            if (guard)
                guard->setContentsMargins(base);
        },
        QEasingCurve(QEasingCurve::Linear));
}

} // namespace motion

// ---- SnapshotOverlay ---------------------------------------------------------------

SnapshotOverlay::SnapshotOverlay(const QPixmap &pixmap, QWidget *parent)
    : QWidget(parent)
    , m_pixmap(pixmap)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
}

void SnapshotOverlay::setOpacity(qreal opacity)
{
    m_opacity = opacity;
    update();
}

void SnapshotOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setOpacity(m_opacity);
    p.drawPixmap(0, 0, m_pixmap);
}

} // namespace sct
