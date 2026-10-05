#include "ui/Theme.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QSvgRenderer>
#include <QTemporaryDir>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QSettings>
#include <QStyle>
#include <QStyleHints>
#include <QWidget>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#include <dwmapi.h>
#endif

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
    c.infoBg = QColor(0xFD, 0xF1, 0xF2);
    c.infoBorder = QColor(0xF5, 0xD5, 0xDA);
    c.infoText = QColor(0x1B, 0x1F, 0x27);
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
    c.infoBg = QColor(0x2A, 0x1E, 0x22);
    c.infoBorder = QColor(0x4A, 0x2C, 0x33);
    c.infoText = QColor(0xE7, 0xE9, 0xED);
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
QWidget#centralArea { background: transparent; }

QLabel { color: {text}; }
QLabel[role="muted"] { color: {textMuted}; }
QLabel[role="caption"] { color: {textMuted}; }
QLabel[role="jyutping"] { color: {jyutping}; }
QLabel[role="warning"] { color: {warnText}; }
QLabel[role="error"] { color: {errorText}; }
QLabel[role="success"] { color: {successText}; }
QLabel[role="keycap"] {
    background: {surface};
    border: 1px solid {borderStrong};
    border-bottom-width: 2px;
    border-radius: 6px;
    padding: 2px 8px;
    color: {text};
}
QLabel a { color: {accent}; }

QPlainTextEdit#sourceEdit {
    background: transparent;
    border: none;
    color: {text};
    selection-background-color: {selection};
    selection-color: {text};
}
QScrollArea { background: transparent; border: none; }

QLineEdit, QComboBox {
    background: {surface};
    color: {text};
    border: 1px solid {border};
    border-radius: 8px;
    padding: 6px 10px;
    min-height: 20px;
    selection-background-color: {selection};
    selection-color: {text};
}
QLineEdit:hover, QComboBox:hover { border-color: {borderStrong}; }
QLineEdit:focus, QComboBox:focus, QComboBox:on { border: 1px solid {accent}; }
QLineEdit:disabled, QComboBox:disabled { color: {textDisabled}; background: {surfaceAlt}; }
QComboBox QLineEdit { border: none; padding: 0px; background: transparent; min-height: 0px; }
QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: center right; width: 26px; border: none; }
QComboBox::down-arrow { image: url("{chevronPath}"); width: 12px; height: 12px; }
QComboBox QAbstractItemView {
    background: {surface};
    color: {text};
    border: 1px solid {border};
    padding: 4px;
    outline: 0px;
    selection-background-color: {hoverSolid};
    selection-color: {text};
}
QComboBox QAbstractItemView::item { min-height: 28px; padding: 0px 8px; border-radius: 6px; }
QComboBox QAbstractItemView::item:selected { background: {hoverSolid}; color: {text}; }

QSlider { min-height: 22px; }
QSlider::groove:horizontal { height: 4px; background: {switchOff}; border-radius: 2px; }
QSlider::sub-page:horizontal { background: {accent}; border-radius: 2px; }
QSlider::handle:horizontal {
    background: {knob};
    border: 1px solid {borderStrong};
    width: 16px;
    height: 16px;
    margin: -7px 0px;
    border-radius: 9px;
}
QSlider::handle:horizontal:hover, QSlider::handle:horizontal:focus { border: 1px solid {accent}; }

QPushButton {
    background: {surface};
    color: {text};
    border: 1px solid {border};
    border-radius: 8px;
    padding: 6px 16px;
    min-height: 20px;
}
QPushButton:hover { background: {surfaceAlt}; border-color: {borderStrong}; }
QPushButton:pressed { background: {pressedSolid}; }
QPushButton:default { background: {accent}; color: {onAccent}; border-color: {accent}; }
QPushButton:default:hover { background: {accentHover}; border-color: {accentHover}; }

QFrame[banner="info"] { background: {infoBg}; border: 1px solid {infoBorder}; border-radius: 12px; }
QFrame[banner="info"] QLabel { color: {infoText}; }
QFrame[banner="info"] QLabel#bannerText { color: {textMuted}; }
QFrame[banner="warning"] { background: {warnBg}; border: 1px solid {warnBorder}; border-radius: 12px; }
QFrame[banner="warning"] QLabel { color: {warnText}; }
QFrame[banner="error"] { background: {errorBg}; border: 1px solid {errorBorder}; border-radius: 12px; }
QFrame[banner="error"] QLabel { color: {errorText}; }
QFrame[banner] QLabel#bannerDetails {
    background: {surface};
    border: 1px solid {border};
    border-radius: 8px;
    padding: 8px 10px;
    color: {textMuted};
}
QFrame#divider { background: {border}; border: none; }

QToolTip {
    color: {tooltipText};
    background: {tooltipBg};
    border: 1px solid {tooltipBg};
    padding: 5px 8px;
}

QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: {scrollHandle}; border-radius: 3px; min-height: 32px; margin: 0px 2px; }
QScrollBar::handle:vertical:hover { background: {scrollHandleHover}; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: {scrollHandle}; border-radius: 3px; min-width: 32px; margin: 2px 0px; }
QScrollBar::handle:horizontal:hover { background: {scrollHandleHover}; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0px; height: 0px; }
QScrollBar::add-page, QScrollBar::sub-page { background: none; }

QMenu {
    background: {surface};
    border: 1px solid {border};
    padding: 5px;
}
QMenu::item { padding: 7px 28px 7px 12px; border-radius: 6px; color: {text}; }
QMenu::item:selected { background: {hoverSolid}; }
QMenu::item:disabled { color: {textDisabled}; }
QMenu::separator { height: 1px; background: {border}; margin: 4px 8px; }
QMenu::icon { padding-left: 10px; }
QCheckBox { color: {text}; spacing: 8px; }
)QSS";

// Style sheets can't tint images, so write the combo-box chevron in the
// current muted colour (1x and @2x) to a private temp directory.
QString chevronImagePath(const ThemeColors &c)
{
    static QTemporaryDir *dir = nullptr;
    if (!dir) {
        dir = new QTemporaryDir(QDir::tempPath() + QStringLiteral("/sct-theme-XXXXXX"));
        QObject::connect(qApp, &QObject::destroyed, [] {
            delete dir;
            dir = nullptr;
        });
    }
    if (!dir->isValid())
        return QString();
    const QString base = dir->filePath(c.dark ? QStringLiteral("chevron-dark") : QStringLiteral("chevron-light"));
    const QString path = base + QStringLiteral(".png");
    if (!QFile::exists(path)) {
        QFile f(QStringLiteral(":/icons/chevron-down.svg"));
        QByteArray svg = f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
        svg.replace("currentColor", c.textMuted.name(QColor::HexRgb).toLatin1());
        for (int scale : {1, 2}) {
            QImage img(12 * scale, 12 * scale, QImage::Format_ARGB32_Premultiplied);
            img.fill(Qt::transparent);
            QPainter p(&img);
            p.setRenderHint(QPainter::Antialiasing);
            QSvgRenderer(svg).render(&p, QRectF(0, 0, img.width(), img.height()));
            p.end();
            img.save(scale == 1 ? path : base + QStringLiteral("@2x.png"));
        }
    }
    return QDir::fromNativeSeparators(path);
}

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
#ifdef Q_OS_WIN
        static bool filterInstalled = false;
        if (!filterInstalled) {
            qApp->installEventFilter(t);  // dark title bars for windows shown later
            filterInstalled = true;
        }
        for (QWidget *w : QApplication::topLevelWidgets()) {
            if (w->isVisible())
                applyWindowFrame(w);
        }
#endif
    }
    applying = false;
    if (changed)
        emit t->changed();
}

void Theme::applyWindowFrame(QWidget *window)
{
#ifdef Q_OS_WIN
    // Real HWNDs only (not e.g. the offscreen platform used by tests).
    if (!window || !window->isWindow() || QGuiApplication::platformName() != QLatin1String("windows"))
        return;
    const Qt::WindowType type = window->windowType();
    if (type != Qt::Window && type != Qt::Dialog)
        return;
    // DWMWA_USE_IMMERSIVE_DARK_MODE is 20 on Windows 10 20H1+ and 11 (19 before).
    const BOOL dark = isDark() ? TRUE : FALSE;
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    if (FAILED(DwmSetWindowAttribute(hwnd, 20, &dark, sizeof(dark))))
        DwmSetWindowAttribute(hwnd, 19, &dark, sizeof(dark));
    // Repaint the non-client area so a visible window updates immediately.
    SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
#else
    Q_UNUSED(window);
#endif
}

bool Theme::eventFilter(QObject *watched, QEvent *event)
{
#ifdef Q_OS_WIN
    if (event->type() == QEvent::Show && watched->isWidgetType()) {
        auto *w = static_cast<QWidget *>(watched);
        if (w->isWindow())
            applyWindowFrame(w);
    }
#else
    Q_UNUSED(watched);
    Q_UNUSED(event);
#endif
    return false;
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
        {QStringLiteral("switchOff"), c.switchOff},
        {QStringLiteral("knob"), c.knob},
    };
    for (const auto &token : tokens)
        qss.replace(QLatin1Char('{') + token.first + QLatin1Char('}'), css(token.second));
    // Quoted in the style sheet: the temp path contains the user name, and an
    // unquoted url() breaks on e.g. C:/Users/O'Brien/AppData/Local/Temp/...
    qss.replace(QStringLiteral("{chevronPath}"), chevronImagePath(c));
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
