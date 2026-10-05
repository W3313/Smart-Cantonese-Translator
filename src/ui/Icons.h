#pragma once

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

class QPainter;
class QRectF;

namespace sct::ui {

// Which theme colour an icon is drawn in. Resolved at paint time, so icons
// follow light/dark theme switches without being recreated.
enum class IconTone { Text, Muted, Accent, OnAccent, Star, Warning, Error, Info, Success };

QColor toneColor(IconTone tone);

// Monochrome icon from :/icons/<name>.svg, tinted with the tone colour.
// The SVGs use the literal "currentColor" which is replaced at render time.
// Disabled mode uses the disabled text colour; checked (On) state of a Text
// icon uses the accent colour.
QIcon icon(const QString &name, IconTone tone = IconTone::Text);

// Same, rendered to a pixmap of size x size logical pixels at dpr.
QPixmap iconPixmap(const QString &name, IconTone tone, int size, qreal dpr);
QPixmap iconPixmap(const QString &name, const QColor &color, int size, qreal dpr);

// Application icon: :/icons/app.svg or app.png when present, otherwise a
// generated jade badge with 粵.
QIcon appIcon();
QPixmap appLogo(int size, qreal dpr);

// Draws an indeterminate spinner arc (used by BusyIndicator and SpeakButton).
void paintSpinner(QPainter *p, const QRectF &rect, int angle, const QColor &color, qreal penWidth);

} // namespace sct::ui
