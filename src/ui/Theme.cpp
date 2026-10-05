#include "ui/Theme.h"

#include <QApplication>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QSettings>
#include <QStyle>
#include <QStyleHints>
#include <QWidget>

namespace sct {

namespace {

QColor rgba(int r, int g, int b, int a) { return QColor(r, g, b, a); }

ThemeColors lightColors()
{
    ThemeColors c;
    c.dark = false;
    c.window = QColor(0xF3, 0xF4, 0xF7);
    c.surface = QColor(0xFF, 0xFF, 0xFF);
    c.surfaceAlt = QColor(0xF0, 0xF2, 0xF5);
    c.hover = rgba(16, 24, 40, 15);
    c.pressed = rgba(16, 24, 40, 28);
    c.border = QColor(0xDF, 0xE2, 0xE8);
    c.borderStrong = QColor(0xC5, 0xCA, 0xD3);
    c.text = QColor(0x1B, 0x1F, 0x27);
    c.textMuted = QColor(0x5F, 0x68, 0x76);
    c.textDisabled = QColor(0xA3, 0xA9, 0xB3);
    c.accent = QColor(0xD3, 0x2F, 0x45);
    c.accentHover = QColor(0xBD, 0x25, 0x3B);
    c.accentPressed = QColor(0xA5, 0x1E, 0x33);
    c.accentSoft = QColor(0xFC, 0xE9, 0xEB);
    c.onAccent = QColor(0xFF, 0xFF, 0xFF);
    c.jyutping = QColor(0xA3, 0x45, 0x55);
    c.star = QColor(0xE8, 0xA0, 0x0C);
    c.selection = QColor(0xF8, 0xCF, 0xD5);
    c.scrollHandle = rgba(0, 0, 0, 56);
    c.scrollHandleHover = rgba(0, 0, 0, 96);
    c.tooltipBg = QColor(0x24, 0x28, 0x2F);
    c.tooltipText = QColor(0xF3, 0xF4, 0xF6);
    c.segmentPill = QColor(0xFF, 0xFF, 0xFF);
    c.switchOff = QColor(0xD3, 0xD7, 0xDE);
    c.knob = QColor(0xFF, 0xFF, 0xFF);
    c.shadow = QColor(16, 24, 40);
    c.skeleton = QColor(0xEC, 0xEE, 0xF2);
    c.skeletonShine = QColor(0xF8, 0xF9, 0xFB);
    c.infoBg = QColor(0xEA, 0xF2, 0xFD);
    c.infoBorder = QColor(0xC8, 0xDB, 0xF7);
    c.infoText = QColor(0x1E, 0x4F, 0x8F);
    c.warnBg = QColor(0xFF, 0xF7, 0xE3);
    c.warnBorder = QColor(0xF1, 0xDC, 0xA0);
    c.warnText = QColor(0x73, 0x4D, 0x00);
    c.errorBg = QColor(0xFD, 0xED, 0xEC);
    c.errorBorder = QColor(0xF4, 0xC7, 0xC3);
    c.errorText = QColor(0x9F, 0x24, 0x1B);
    c.successText = QColor(0x1E, 0x7F, 0x4F);
    return c;
}

ThemeColors darkColors()
{
    ThemeColors c;
    c.dark = true;
    c.window = QColor(0x16, 0x17, 0x1B);
    c.surface = QColor(0x1F, 0x21, 0x26);
    c.surfaceAlt = QColor(0x28, 0x2B, 0x31);
    c.hover = rgba(255, 255, 255, 18);
    c.pressed = rgba(255, 255, 255, 31);
    c.border = QColor(0x32, 0x35, 0x3C);
    c.borderStrong = QColor(0x48, 0x4C, 0x55);
    c.text = QColor(0xE7, 0xE9, 0xED);
    c.textMuted = QColor(0x9C, 0xA3, 0xAF);
    c.textDisabled = QColor(0x60, 0x66, 0x71);
    c.accent = QColor(0xFF, 0x6E, 0x7A);
    c.accentHover = QColor(0xFF, 0x87, 0x91);
    c.accentPressed = QColor(0xF0, 0x5A, 0x68);
    c.accentSoft = QColor(0x3D, 0x22, 0x28);
    c.onAccent = QColor(0x2A, 0x08, 0x0E);
    c.jyutping = QColor(0xEE, 0xA7, 0xB0);
    c.star = QColor(0xF7, 0xBE, 0x45);
    c.selection = QColor(0x6A, 0x2C, 0x37);
    c.scrollHandle = rgba(255, 255, 255, 52);
    c.scrollHandleHover = rgba(255, 255, 255, 88);
    c.tooltipBg = QColor(0x3A, 0x3D, 0x45);
    c.tooltipText = QColor(0xF0, 0xF1, 0xF3);
    c.segmentPill = QColor(0x3A, 0x3D, 0x45);
    c.switchOff = QColor(0x4A, 0x4E, 0x57);
    c.knob = QColor(0xF4, 0xF5, 0xF7);
    c.shadow = QColor(0, 0, 0);
    c.skeleton = QColor(0x2A, 0x2D, 0x33);
    c.skeletonShine = QColor(0x36, 0x39, 0x41);
    c.infoBg = QColor(0x1C, 0x27, 0x35);
    c.infoBorder = QColor(0x2C, 0x44, 0x66);
    c.infoText = QColor(0xA8, 0xC7, 0xF0);
    c.warnBg = QColor(0x2F, 0x29, 0x18);
    c.warnBorder = QColor(0x5A, 0x4A, 0x1F);
    c.warnText = QColor(0xF0, 0xD0, 0x7A);
    c.errorBg = QColor(0x33, 0x19, 0x1B);
    c.errorBorder = QColor(0x64, 0x30, 0x35);
    c.errorText = QColor(0xFF, 0xB3, 0xAD);
    c.successText = QColor(0x5F, 0xD3, 0x9A);
    return c;
}

QPalette makePalette(const ThemeColors &c)
{
    QPalette p;
    const QColor button = c.dark ? QColor(0x2A, 0x2D, 0x33) : c.surface;
    for (auto group : {QPalette::Active, QPalette::Inactive}) {
        p.setColor(group, QPalette::Window, c.window);
        p.setColor(group, QPalette::WindowText, c.text);
        p.setColor(group, QPalette::Base, c.surface);
        p.setColor(group, QPalette::AlternateBase, c.surfaceAlt);
        p.setColor(group, QPalette::ToolTipBase, c.tooltipBg);
        p.setColor(group, QPalette::ToolTipText, c.tooltipText);
        p.setColor(group, QPalette::PlaceholderText, c.textMuted);
        p.setColor(group, QPalette::Text, c.text);
        p.setColor(group, QPalette::Button, button);
        p.setColor(group, QPalette::ButtonText, c.text);
        p.setColor(group, QPalette::BrightText, Qt::white);
        p.setColor(group, QPalette::Highlight, c.accent);
        p.setColor(group, QPalette::HighlightedText, c.onAccent);
        p.setColor(group, QPalette::Link, c.accent);
        p.setColor(group, QPalette::LinkVisited, c.accent);
        if (c.dark) {
            p.setColor(group, QPalette::Light, QColor(0x3A, 0x3D, 0x45));
            p.setColor(group, QPalette::Midlight, QColor(0x30, 0x33, 0x3A));
            p.setColor(group, QPalette::Mid, QColor(0x24, 0x26, 0x2B));
            p.setColor(group, QPalette::Dark, QColor(0x12, 0x13, 0x16));
            p.setColor(group, QPalette::Shadow, QColor(0, 0, 0));
        } else {
            p.setColor(group, QPalette::Light, QColor(0xFF, 0xFF, 0xFF));
            p.setColor(group, QPalette::Midlight, QColor(0xF0, 0xF2, 0xF5));
            p.setColor(group, QPalette::Mid, QColor(0xC5, 0xCA, 0xD3));
            p.setColor(group, QPalette::Dark, QColor(0x9A, 0xA1, 0xAD));
            p.setColor(group, QPalette::Shadow, QColor(0x6B, 0x72, 0x80));
        }
    }
    // Disabled: same surfaces, faded text.
    p.setColor(QPalette::Disabled, QPalette::Window, c.window);
    p.setColor(QPalette::Disabled, QPalette::Base, c.surfaceAlt);
    p.setColor(QPalette::Disabled, QPalette::AlternateBase, c.surfaceAlt);
    p.setColor(QPalette::Disabled, QPalette::Button, c.dark ? QColor(0x24, 0x26, 0x2B) : c.surfaceAlt);
    p.setColor(QPalette::Disabled, QPalette::WindowText, c.textDisabled);
    p.setColor(QPalette::Disabled, QPalette::Text, c.textDisabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, c.textDisabled);
    p.setColor(QPalette::Disabled, QPalette::PlaceholderText, c.textDisabled);
    p.setColor(QPalette::Disabled, QPalette::Highlight, c.border);
    p.setColor(QPalette::Disabled, QPalette::HighlightedText, c.textDisabled);
    p.setColor(QPalette::Disabled, QPalette::Link, c.textDisabled);
    p.setColor(QPalette::Disabled, QPalette::ToolTipBase, c.tooltipBg);
    p.setColor(QPalette::Disabled, QPalette::ToolTipText, c.tooltipText);
    p.setColor(QPalette::Disabled, QPalette::Light, p.color(QPalette::Active, QPalette::Light));
    p.setColor(QPalette::Disabled, QPalette::Midlight, p.color(QPalette::Active, QPalette::Midlight));
    p.setColor(QPalette::Disabled, QPalette::Mid, p.color(QPalette::Active, QPalette::Mid));
    p.setColor(QPalette::Disabled, QPalette::Dark, p.color(QPalette::Active, QPalette::Dark));
    p.setColor(QPalette::Disabled, QPalette::Shadow, p.color(QPalette::Active, QPalette::Shadow));
    return p;
}

// QSS colour literal (keeps alpha).
QString css(const QColor &c)
{
    if (c.alpha() == 255)
        return c.name(QColor::HexRgb);
    return QStringLiteral("rgba(%1, %2, %3, %4)").arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha());
}

// Captured once, before the first apply() replaces the application palette.
bool g_initialPaletteDark = false;
bool g_initialPaletteCaptured = false;

void captureInitialPalette()
{
    if (g_initialPaletteCaptured)
        return;
    g_initialPaletteCaptured = true;
    const QPalette pal = QGuiApplication::palette();
    g_initialPaletteDark = pal.color(QPalette::Window).lightness() < 128;
}

const char kQss[] = R"QSS(
QMainWindow, QDialog { background: {window}; }
QWidget#centralArea, QWidget#headerBar, QWidget#historyPanel, QWidget#dockTitle { background: transparent; }

QFrame#pane {
    background: {surface};
    border: 1px solid {border};
    border-radius: 14px;
}
QFrame#pane QPlainTextEdit, QFrame#pane QScrollArea {
    background: transparent;
    border: none;
}
QWidget#resultContent, QWidget#emptyPage, QWidget#loadingPage, QWidget#errorPage { background: transparent; }

QLabel[role="muted"] { color: {textMuted}; }
QLabel[role="caption"] { color: {textMuted}; }
QLabel[role="paneTitle"] { color: {textMuted}; }
QLabel[role="jyutping"] { color: {jyutping}; }
QLabel[role="warning"] { color: {warnText}; }
QLabel[role="error"] { color: {errorText}; }
QLabel[role="success"] { color: {successText}; }
QLabel[role="langPill"] {
    background: {surface};
    border: 1px solid {border};
    border-radius: 10px;
    padding: 5px 12px;
    color: {text};
}
QLabel#appTitle { color: {text}; }

QPushButton {
    background: {surface};
    color: {text};
    border: 1px solid {border};
    border-radius: 8px;
    padding: 6px 14px;
}
QPushButton:hover { background: {surfaceAlt}; border-color: {borderStrong}; }
QPushButton:pressed { background: {pressedSolid}; }
QPushButton:disabled { color: {textDisabled}; background: {surfaceAlt}; border-color: {border}; }
QPushButton:default { border-color: {accent}; }
QPushButton[primary="true"] {
    background: {accent};
    color: {onAccent};
    border: 1px solid {accent};
    padding: 7px 18px;
}
QPushButton[primary="true"]:hover { background: {accentHover}; border-color: {accentHover}; }
QPushButton[primary="true"]:pressed { background: {accentPressed}; border-color: {accentPressed}; }
QPushButton[primary="true"]:disabled { background: {accentSoft}; border-color: {accentSoft}; color: {textDisabled}; }
QPushButton[chip="true"] {
    background: {surfaceAlt};
    border: 1px solid {border};
    border-radius: 15px;
    padding: 6px 14px;
    color: {text};
}
QPushButton[chip="true"]:hover { border-color: {accent}; color: {accent}; }
QPushButton[link="true"] {
    background: transparent;
    border: none;
    padding: 2px 4px;
    color: {accent};
    text-decoration: underline;
}

QToolButton {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 8px;
    padding: 5px;
    color: {text};
}
QToolButton:hover { background: {hover}; }
QToolButton:pressed { background: {pressed}; }
QToolButton:checked { background: {accentSoft}; color: {accent}; }
QToolButton:disabled { color: {textDisabled}; }
QToolButton::menu-indicator { image: none; width: 0px; }
QToolButton[subtle="true"] { color: {textMuted}; }
QToolButton[subtle="true"]:hover { color: {text}; }

QWidget#segmented {
    background: {surfaceAlt};
    border: 1px solid {border};
    border-radius: 10px;
}
QWidget#segmented QToolButton {
    border-radius: 7px;
    padding: 4px 12px;
    color: {textMuted};
    border: 1px solid transparent;
}
QWidget#segmented QToolButton:hover { color: {text}; background: transparent; }
QWidget#segmented QToolButton:checked {
    background: {surface};
    color: {text};
    border: 1px solid {border};
}

QToolButton#providerBadge {
    background: {surface};
    border: 1px solid {border};
    border-radius: 14px;
    padding: 4px 12px 4px 8px;
    color: {textMuted};
}
QToolButton#providerBadge:hover { border-color: {accent}; color: {text}; }
QToolButton#providerBadge[warning="true"] { color: {warnText}; border-color: {warnBorder}; background: {warnBg}; }

QToolButton#sectionHeader {
    border: none;
    padding: 4px 2px;
    color: {textMuted};
    background: transparent;
}
QToolButton#sectionHeader:hover { color: {text}; }

QFrame[banner="info"] { background: {infoBg}; border: 1px solid {infoBorder}; border-radius: 12px; }
QFrame[banner="info"] QLabel { color: {infoText}; }
QFrame[banner="warning"] { background: {warnBg}; border: 1px solid {warnBorder}; border-radius: 12px; }
QFrame[banner="warning"] QLabel { color: {warnText}; }
QFrame[banner="error"] { background: {errorBg}; border: 1px solid {errorBorder}; border-radius: 12px; }
QFrame[banner="error"] QLabel { color: {errorText}; }
QFrame[banner] QLabel#bannerDetails {
    background: {surface};
    border: 1px solid {border};
    border-radius: 6px;
    padding: 8px;
    color: {textMuted};
}
QFrame[banner] QPushButton[link="true"] { color: {text}; }

QFrame#altCard {
    background: {surfaceAlt};
    border: 1px solid transparent;
    border-radius: 10px;
}
QFrame#altCard:hover { border-color: {border}; }
QFrame#divider { background: {border}; border: none; min-height: 1px; max-height: 1px; }

QLabel#charCounter[over="true"] { color: {warnText}; }
QLabel#directionHint { color: {accent}; }

QTabWidget::pane {
    border: 1px solid {border};
    border-radius: 12px;
    background: {surface};
    top: -1px;
}
QTabBar::tab {
    padding: 8px 16px;
    margin-right: 2px;
    color: {textMuted};
    background: transparent;
    border: none;
    border-bottom: 2px solid transparent;
}
QTabBar::tab:selected { color: {text}; border-bottom: 2px solid {accent}; }
QTabBar::tab:hover { color: {text}; }

QGroupBox {
    border: 1px solid {border};
    border-radius: 10px;
    margin-top: 18px;
    padding: 14px 12px 10px 12px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    subcontrol-position: top left;
    left: 10px;
    padding: 0 4px;
    color: {textMuted};
}

QToolTip {
    color: {tooltipText};
    background: {tooltipBg};
    border: 1px solid {tooltipBg};
    padding: 5px 8px;
}

QScrollBar:vertical { background: transparent; width: 12px; margin: 2px; }
QScrollBar::handle:vertical { background: {scrollHandle}; border-radius: 4px; min-height: 32px; margin: 0 2px; }
QScrollBar::handle:vertical:hover { background: {scrollHandleHover}; }
QScrollBar:horizontal { background: transparent; height: 12px; margin: 2px; }
QScrollBar::handle:horizontal { background: {scrollHandle}; border-radius: 4px; min-width: 32px; margin: 2px 0; }
QScrollBar::handle:horizontal:hover { background: {scrollHandleHover}; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0px; height: 0px; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }

QStatusBar { background: {window}; color: {textMuted}; }
QStatusBar::item { border: none; }
QStatusBar QLabel { color: {textMuted}; }

QSplitter::handle { background: transparent; }
QMainWindow::separator { background: transparent; width: 8px; height: 8px; }

QListView#historyList {
    background: {surface};
    border: 1px solid {border};
    border-radius: 12px;
    padding: 4px;
    outline: 0;
}

QMenu {
    background: {surface};
    border: 1px solid {border};
    padding: 4px;
}
QMenu::item { padding: 6px 24px 6px 12px; border-radius: 6px; color: {text}; }
QMenu::item:selected { background: {hoverSolid}; }
QMenu::item:disabled { color: {textDisabled}; }
QMenu::separator { height: 1px; background: {border}; margin: 4px 8px; }
QMenu::icon { padding-left: 8px; }
)QSS";

} // namespace

Theme::Theme(QObject *parent)
    : QObject(parent)
{
    captureInitialPalette();
    m_colors = lightColors();
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (auto *hints = QGuiApplication::styleHints())
        connect(hints, &QStyleHints::colorSchemeChanged, this, &Theme::onSystemSchemeChanged);
#endif
}

Theme *Theme::instance()
{
    static Theme *s_theme = nullptr;
    if (!s_theme)
        s_theme = new Theme(qApp);
    return s_theme;
}

QString Theme::mode() { return instance()->m_mode; }
bool Theme::isApplied() { return instance()->m_applied; }
bool Theme::isDark() { return instance()->m_colors.dark; }
const ThemeColors &Theme::colors() { return instance()->m_colors; }

bool Theme::systemPrefersDark()
{
    captureInitialPalette();
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    if (auto *hints = QGuiApplication::styleHints()) {
        const Qt::ColorScheme scheme = hints->colorScheme();
        if (scheme != Qt::ColorScheme::Unknown)
            return scheme == Qt::ColorScheme::Dark;
    }
#endif
#ifdef Q_OS_WIN
    QSettings reg(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
                  QSettings::NativeFormat);
    if (reg.contains(QStringLiteral("AppsUseLightTheme")))
        return reg.value(QStringLiteral("AppsUseLightTheme")).toInt() == 0;
#endif
    return g_initialPaletteDark;
}

void Theme::apply(const QString &requestedMode)
{
    Theme *t = instance();
    static bool applying = false;
    if (applying)
        return;
    applying = true;

    QString mode = requestedMode.trimmed().toLower();
    if (mode != QLatin1String("light") && mode != QLatin1String("dark"))
        mode = QStringLiteral("system");
    t->m_mode = mode;

#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    // Let Qt follow the OS again before asking which scheme it prefers.
    if (mode == QLatin1String("system"))
        QGuiApplication::styleHints()->setColorScheme(Qt::ColorScheme::Unknown);
#endif
    const bool dark = mode == QLatin1String("dark")
                      || (mode == QLatin1String("system") && systemPrefersDark());
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
    // Also makes the native title bar match on Windows.
    if (mode != QLatin1String("system"))
        QGuiApplication::styleHints()->setColorScheme(dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
#endif

    const bool changed = !t->m_applied || t->m_colors.dark != dark;
    t->m_colors = dark ? darkColors() : lightColors();
    t->m_applied = true;

    if (qobject_cast<QApplication *>(QCoreApplication::instance())) {
        QApplication::setPalette(makePalette(t->m_colors));
        qApp->setStyleSheet(styleSheet(t->m_colors));
    }
    applying = false;
    if (changed)
        emit t->changed();
}

void Theme::onSystemSchemeChanged()
{
    if (m_mode == QLatin1String("system") && systemPrefersDark() != m_colors.dark)
        apply(m_mode);
}

QString Theme::styleSheet(const ThemeColors &c)
{
    // Opaque versions of the translucent hover/pressed colours, for widgets
    // (menus, push buttons) whose background must not show through.
    auto blend = [](const QColor &base, const QColor &over) {
        const qreal a = over.alphaF();
        return QColor::fromRgbF(base.redF() * (1 - a) + over.redF() * a,
                                base.greenF() * (1 - a) + over.greenF() * a,
                                base.blueF() * (1 - a) + over.blueF() * a);
    };
    QString qss = QString::fromUtf8(kQss);
    const QList<QPair<QString, QColor>> tokens = {
        {QStringLiteral("window"), c.window},
        {QStringLiteral("surface"), c.surface},
        {QStringLiteral("surfaceAlt"), c.surfaceAlt},
        {QStringLiteral("hover"), c.hover},
        {QStringLiteral("hoverSolid"), blend(c.surface, c.hover)},
        {QStringLiteral("pressed"), c.pressed},
        {QStringLiteral("pressedSolid"), blend(c.surface, c.pressed)},
        {QStringLiteral("border"), c.border},
        {QStringLiteral("borderStrong"), c.borderStrong},
        {QStringLiteral("text"), c.text},
        {QStringLiteral("textMuted"), c.textMuted},
        {QStringLiteral("textDisabled"), c.textDisabled},
        {QStringLiteral("accent"), c.accent},
        {QStringLiteral("accentHover"), c.accentHover},
        {QStringLiteral("accentPressed"), c.accentPressed},
        {QStringLiteral("accentSoft"), c.accentSoft},
        {QStringLiteral("onAccent"), c.onAccent},
        {QStringLiteral("jyutping"), c.jyutping},
        {QStringLiteral("scrollHandle"), c.scrollHandle},
        {QStringLiteral("scrollHandleHover"), c.scrollHandleHover},
        {QStringLiteral("tooltipBg"), c.tooltipBg},
        {QStringLiteral("tooltipText"), c.tooltipText},
        {QStringLiteral("infoBg"), c.infoBg},
        {QStringLiteral("infoBorder"), c.infoBorder},
        {QStringLiteral("infoText"), c.infoText},
        {QStringLiteral("warnBg"), c.warnBg},
        {QStringLiteral("warnBorder"), c.warnBorder},
        {QStringLiteral("warnText"), c.warnText},
        {QStringLiteral("errorBg"), c.errorBg},
        {QStringLiteral("errorBorder"), c.errorBorder},
        {QStringLiteral("errorText"), c.errorText},
        {QStringLiteral("successText"), c.successText},
    };
    for (const auto &token : tokens)
        qss.replace(QLatin1Char('{') + token.first + QLatin1Char('}'), css(token.second));
    return qss;
}

// ---- Fonts -------------------------------------------------------------------

namespace {

QStringList uiFamilies()
{
    return {QStringLiteral("Segoe UI"),           QStringLiteral("Microsoft JhengHei UI"),
            QStringLiteral("Noto Sans"),          QStringLiteral("Noto Sans CJK TC"),
            QStringLiteral("PingFang HK"),        QStringLiteral("Helvetica Neue"),
            QStringLiteral("DejaVu Sans")};
}

QStringList chineseFamilies(ChineseScript script)
{
    const QStringList traditional = {QStringLiteral("Microsoft JhengHei UI"), QStringLiteral("Microsoft JhengHei"),
                                     QStringLiteral("Noto Sans CJK TC"),      QStringLiteral("Noto Sans CJK HK"),
                                     QStringLiteral("PingFang HK"),           QStringLiteral("Source Han Sans TC")};
    if (script == ChineseScript::Simplified) {
        QStringList simplified = {QStringLiteral("Microsoft YaHei UI"), QStringLiteral("Microsoft YaHei"),
                                  QStringLiteral("Noto Sans CJK SC"),   QStringLiteral("PingFang SC"),
                                  QStringLiteral("Source Han Sans SC")};
        return simplified + traditional + QStringList{QStringLiteral("Segoe UI")};
    }
    return traditional + QStringList{QStringLiteral("Segoe UI")};
}

} // namespace

QFont Theme::uiFont(qreal pointSize, int weight)
{
    QFont f = QApplication::font();
    f.setFamilies(uiFamilies());
    if (pointSize > 0)
        f.setPointSizeF(pointSize);
    if (weight > 0)
        f.setWeight(static_cast<QFont::Weight>(weight));
    return f;
}

QFont Theme::textFont(Language lang, qreal pointSize, ChineseScript script)
{
    QFont f = QApplication::font();
    if (lang == Language::Cantonese)
        f.setFamilies(chineseFamilies(script));
    else
        f.setFamilies(uiFamilies());
    f.setPointSizeF(pointSize);
    return f;
}

QFont Theme::jyutpingFont(qreal pointSize)
{
    QFont f = uiFont(pointSize);
    f.setLetterSpacing(QFont::AbsoluteSpacing, 0.4);
    return f;
}

QFont Theme::monoFont(qreal pointSize)
{
    QFont f = QApplication::font();
    f.setFamilies({QStringLiteral("Cascadia Mono"), QStringLiteral("Consolas"), QStringLiteral("DejaVu Sans Mono"),
                   QStringLiteral("Noto Sans Mono"), QStringLiteral("Menlo")});
    f.setStyleHint(QFont::Monospace);
    f.setPointSizeF(pointSize);
    return f;
}

QString Theme::languageLabel(Language lang)
{
    return lang == Language::Cantonese ? QStringLiteral("廣東話 Cantonese") : QStringLiteral("English");
}

namespace ui {

void setStyleProperty(QWidget *w, const char *name, const QVariant &value)
{
    if (!w || w->property(name) == value)
        return;
    w->setProperty(name, value);
    w->style()->unpolish(w);
    w->style()->polish(w);
    w->update();
}

QString hex(const QColor &c) { return c.name(QColor::HexRgb); }

QColor mix(const QColor &a, const QColor &b, qreal t)
{
    t = qBound(0.0, t, 1.0);
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * t, a.greenF() + (b.greenF() - a.greenF()) * t,
                            a.blueF() + (b.blueF() - a.blueF()) * t, a.alphaF() + (b.alphaF() - a.alphaF()) * t);
}

void paintSoftShadow(QPainter *p, const QRectF &r, qreal radius, qreal strength)
{
    if (strength <= 0)
        return;
    const QColor base = Theme::colors().shadow;
    const qreal k = Theme::colors().dark ? 2.2 : 1.0;  // dark surfaces need a stronger shadow to read
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    p->setPen(Qt::NoPen);
    for (int i = 4; i >= 1; --i) {
        QColor c = base;
        c.setAlphaF(qMin(1.0, strength * k * 0.035 / i));
        QPainterPath path;
        const QRectF layer = r.adjusted(-i * 0.6, i * 0.5, i * 0.6, i * 1.2);
        path.addRoundedRect(layer, radius + i * 0.6, radius + i * 0.6);
        p->fillPath(path, c);
    }
    p->restore();
}

QColor withAlpha(const QColor &c, qreal factor)
{
    QColor r = c;
    r.setAlphaF(qBound(0.0, c.alphaF() * factor, 1.0));
    return r;
}

} // namespace ui

} // namespace sct
