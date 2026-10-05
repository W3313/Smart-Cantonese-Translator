#pragma once

#include <QEasingCurve>
#include <QObject>
#include <QPixmap>
#include <QString>
#include <QVariant>
#include <QWidget>

#include <functional>

class QSettings;
class QVariantAnimation;

namespace sct {

// UI-only preferences (AppSettings belongs to the core module). Stored next to
// AppSettings: the registry on Windows, or an INI file when useIniFile() is
// called (tests, portable mode).
class UiPrefs : public QObject
{
    Q_OBJECT

public:
    static UiPrefs *instance();
    static void useIniFile(const QString &path);

    // "ui/reduceMotion" - defaults to the OS "show animations" setting.
    bool reduceMotion() const;
    void setReduceMotion(bool on);
    // "ui/historyVisible" - the history panel is hidden until opened.
    bool historyVisible() const;
    void setHistoryVisible(bool on);

    static bool systemPrefersReducedMotion();

signals:
    void changed();

private:
    explicit UiPrefs(QObject *parent = nullptr);
    QSettings *settings() const;

    mutable QSettings *m_settings = nullptr;
    mutable int m_reduceMotionCache = -1;  // -1 unknown, 0/1
};

// Centralised motion: every duration and easing curve in the UI comes from
// here so "Reduce motion" can turn them all off at once.
namespace motion {

constexpr int kFast = 150;    // hover / press colour transitions
constexpr int kNormal = 220;  // reveals, toggles, sliding highlights
constexpr int kSlow = 320;    // panels, page transitions

bool reduced();
int ms(int base);  // base, or 0 when motion is reduced
QEasingCurve outCubic();
QEasingCurve inOutCubic();

// Runs a QVariantAnimation owned by owner and identified by key (a running
// animation with the same owner+key is stopped first). With reduced motion
// onValue(to) and onFinished() run synchronously.
QVariantAnimation *animate(QObject *owner, const QString &key, const QVariant &from, const QVariant &to,
                           int durationMs, const std::function<void(const QVariant &)> &onValue,
                           const std::function<void()> &onFinished = {}, const QEasingCurve &curve = outCubic(),
                           int delayMs = 0);
// Stops (without finishing) / jumps to the end of the owner+key animation.
void stop(QObject *owner, const QString &key);
void finish(QObject *owner, const QString &key);
bool isRunning(QObject *owner, const QString &key);

// Fades w in through a temporary QGraphicsOpacityEffect (removed afterwards).
// Never call on a widget whose ancestor is fading at the same time.
void fadeIn(QWidget *w, int durationMs = kNormal, int delayMs = 0);

// Snapshots target and fades the snapshot out over the (already changed)
// live content: a cheap cross-fade for theme switches and page changes.
void crossFade(QWidget *target, int durationMs = kNormal);

// Small horizontal shake by animating a layout's left/right margins.
void shake(QWidget *w);

} // namespace motion

// Paints a pixmap at a given opacity; used for cross-fades.
class SnapshotOverlay : public QWidget
{
    Q_OBJECT

public:
    SnapshotOverlay(const QPixmap &pixmap, QWidget *parent);
    void setOpacity(qreal opacity);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QPixmap m_pixmap;
    qreal m_opacity = 1.0;
};

} // namespace sct
