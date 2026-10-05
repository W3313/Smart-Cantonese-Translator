#pragma once

#include "core/TranslationTypes.h"

#include <QColor>
#include <QFont>
#include <QObject>
#include <QString>

class QWidget;

namespace sct {

// Colour tokens for the current theme. Custom-painted widgets and the icon
// engine read these at paint time, so a theme switch recolours everything.
struct ThemeColors
{
    bool dark = false;

    QColor window;         // app background
    QColor surface;        // panes, cards, inputs
    QColor surfaceAlt;     // subtle fills: segmented control, chips, alt cards
    QColor hover;          // hover fill for flat buttons / list rows
    QColor pressed;        // pressed fill
    QColor border;         // hairlines around panes and controls
    QColor borderStrong;   // hovered borders
    QColor text;
    QColor textMuted;
    QColor textDisabled;
    QColor accent;         // primary action colour (jade)
    QColor accentHover;
    QColor accentPressed;
    QColor accentSoft;     // tinted fill behind checked/selected things
    QColor onAccent;       // text/icons drawn on accent
    QColor jyutping;       // romanisation line
    QColor star;
    QColor selection;      // text selection background
    QColor scrollHandle;
    QColor scrollHandleHover;
    QColor tooltipBg;
    QColor tooltipText;

    QColor infoBg, infoBorder, infoText;
    QColor warnBg, warnBorder, warnText;
    QColor errorBg, errorBorder, errorText;
    QColor successText;
};

// Application-wide theme (Fusion + palette + small style sheet) and fonts.
class Theme : public QObject
{
    Q_OBJECT

public:
    static Theme *instance();

    // mode: "system" | "light" | "dark". Applies palette, style sheet and
    // (Qt 6.8+) the window frame colour scheme. Safe to call repeatedly.
    static void apply(const QString &mode);
    static QString mode();
    static bool isApplied();
    static bool isDark();
    static const ThemeColors &colors();

    // Whether the operating system currently prefers a dark appearance.
    static bool systemPrefersDark();

    // Fonts ----------------------------------------------------------------
    // UI font (Segoe UI on Windows) with CJK fallbacks; pointSize <= 0 keeps
    // the application default size.
    static QFont uiFont(qreal pointSize = -1, int weight = -1);
    // Font for user text in the given language. Chinese prefers Microsoft
    // JhengHei UI / Noto Sans CJK TC (or the Simplified equivalents).
    static QFont textFont(Language lang, qreal pointSize,
                          ChineseScript script = ChineseScript::Traditional);
    // Font for the Jyutping romanisation line.
    static QFont jyutpingFont(qreal pointSize);
    // Monospace font for technical error details.
    static QFont monoFont(qreal pointSize);

    // "English" / "廣東話 Cantonese".
    static QString languageLabel(Language lang);

    static QString styleSheet(const ThemeColors &c);

signals:
    void changed();

private:
    explicit Theme(QObject *parent = nullptr);
    void onSystemSchemeChanged();

    QString m_mode = QStringLiteral("system");
    ThemeColors m_colors;
    bool m_applied = false;
};

// Small helpers shared by the UI widgets.
namespace ui {

// Sets a dynamic property and re-polishes the widget so style sheet
// selectors like QLabel[role="muted"] update immediately.
void setStyleProperty(QWidget *w, const char *name, const QVariant &value);

// Hex colour without alpha, e.g. "#1d2129".
QString hex(const QColor &c);

} // namespace ui

} // namespace sct
