#include "ui/AboutDialog.h"

#include "core/Version.h"
#include "ui/Controls.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QVBoxLayout>

namespace sct::ui {

namespace {

QLabel *paragraph(const QString &html, QWidget *parent, bool muted = false)
{
    auto *l = new QLabel(html, parent);
    l->setWordWrap(true);
    l->setTextFormat(Qt::RichText);
    l->setOpenExternalLinks(true);
    l->setTextInteractionFlags(Qt::TextBrowserInteraction);
    if (muted)
        l->setProperty("role", QStringLiteral("muted"));
    return l;
}

QLabel *heading(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setFont(Theme::uiFont(10, QFont::DemiBold));
    return l;
}

// Key-cap styled labels for one shortcut ("Ctrl" "Enter").
QWidget *keyCaps(const QKeySequence &seq, QWidget *parent)
{
    auto *w = new QWidget(parent);
    auto *h = new QHBoxLayout(w);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(4);
    const QString text = seq.toString(QKeySequence::NativeText);
    const QStringList parts = text == QLatin1String("+") ? QStringList{text} : text.split(QLatin1Char('+'));
    for (const QString &part : parts) {
        auto *cap = new QLabel(part.isEmpty() ? QStringLiteral("+") : part, w);
        cap->setProperty("role", QStringLiteral("keycap"));
        cap->setFont(Theme::uiFont(9, QFont::Medium));
        cap->setAlignment(Qt::AlignCenter);
        h->addWidget(cap);
    }
    h->addStretch(1);
    return w;
}

} // namespace

void showAboutDialog(QWidget *parent)
{
    QDialog dlg(parent);
    dlg.setObjectName(QStringLiteral("aboutDialog"));
    dlg.setWindowTitle(QObject::tr("About %1").arg(QStringLiteral(SCT_APP_NAME)));
    dlg.setMinimumWidth(480);
    auto *v = new QVBoxLayout(&dlg);
    v->setContentsMargins(32, 28, 32, 22);
    v->setSpacing(8);

    auto *logo = new QLabel(&dlg);
    logo->setPixmap(appLogo(80, dlg.devicePixelRatioF()));
    logo->setFixedSize(80, 80);
    v->addWidget(logo, 0, Qt::AlignHCenter);
    v->addSpacing(6);

    auto *name = new QLabel(QStringLiteral(SCT_APP_NAME), &dlg);
    name->setFont(Theme::uiFont(17, QFont::DemiBold));
    name->setAlignment(Qt::AlignCenter);
    v->addWidget(name);
    auto *version = new QLabel(QObject::tr("Version %1").arg(QStringLiteral(SCT_VERSION_STRING)), &dlg);
    version->setProperty("role", QStringLiteral("muted"));
    version->setAlignment(Qt::AlignCenter);
    version->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->addWidget(version);
    auto *tagline = paragraph(QObject::tr("Natural English ↔ Cantonese translation with Jyutping, alternatives, "
                                          "usage notes and read-aloud."),
                              &dlg);
    tagline->setAlignment(Qt::AlignCenter);
    v->addWidget(tagline);
    v->addSpacing(14);

    v->addWidget(heading(QObject::tr("Privacy"), &dlg));
    v->addWidget(paragraph(QObject::tr("The text you translate is sent to the AI provider you choose (Anthropic or "
                                       "OpenAI). If you use Azure voices, the text being read aloud is sent to "
                                       "Microsoft Azure. Nothing else leaves your computer. API keys are stored "
                                       "encrypted with Windows DPAPI, so only your Windows account can read them."),
                           &dlg, true));
    v->addSpacing(8);
    v->addWidget(heading(QObject::tr("Credits"), &dlg));
    v->addWidget(paragraph(QObject::tr("Translations by Claude (Anthropic) or OpenAI models. Built with "
                                       "<a href=\"https://www.qt.io\">Qt</a> %1, used under the GNU LGPL v3. "
                                       "Fonts and voices are provided by your operating system.")
                               .arg(QString::fromLatin1(qVersion())),
                           &dlg, true));
    if (QStringLiteral(SCT_HOMEPAGE_URL).startsWith(QLatin1String("http"))) {
        v->addSpacing(4);
        v->addWidget(paragraph(QStringLiteral("<a href=\"%1\">%1</a>").arg(QStringLiteral(SCT_HOMEPAGE_URL)), &dlg));
    }

    v->addSpacing(14);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto *close = new Button(QObject::tr("Close"), Button::Variant::Primary, &dlg);
    QObject::connect(close, &QAbstractButton::clicked, &dlg, &QDialog::accept);
    buttons->addWidget(close);
    v->addLayout(buttons);
    close->setFocus();
    dlg.exec();
}

void showShortcutsDialog(QWidget *parent)
{
    QDialog dlg(parent);
    dlg.setObjectName(QStringLiteral("shortcutsDialog"));
    dlg.setWindowTitle(QObject::tr("Keyboard shortcuts"));
    auto *v = new QVBoxLayout(&dlg);
    v->setContentsMargins(28, 24, 28, 20);
    v->setSpacing(14);
    auto *title = new QLabel(QObject::tr("Keyboard shortcuts"), &dlg);
    title->setFont(Theme::uiFont(15, QFont::DemiBold));
    v->addWidget(title);

    const QList<QPair<QString, QKeySequence>> rows = {
        {QObject::tr("Translate"), QKeySequence(Qt::CTRL | Qt::Key_Return)},
        {QObject::tr("Cancel / stop reading aloud"), QKeySequence(Qt::Key_Escape)},
        {QObject::tr("Swap languages"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S)},
        {QObject::tr("Listen to the translation"), QKeySequence(Qt::CTRL | Qt::Key_R)},
        {QObject::tr("Copy the translation"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_C)},
        {QObject::tr("Show / hide history"), QKeySequence(Qt::CTRL | Qt::Key_H)},
        {QObject::tr("Search history"), QKeySequence(Qt::CTRL | Qt::Key_F)},
        {QObject::tr("Focus the text box"), QKeySequence(Qt::CTRL | Qt::Key_L)},
        {QObject::tr("Settings"), QKeySequence(Qt::CTRL | Qt::Key_Comma)},
        {QObject::tr("This list"), QKeySequence(Qt::Key_F1)},
    };
    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(32);
    grid->setVerticalSpacing(10);
    int r = 0;
    for (const auto &row : rows) {
        grid->addWidget(new QLabel(row.first, &dlg), r, 0);
        grid->addWidget(keyCaps(row.second, &dlg), r, 1);
        ++r;
    }
    v->addLayout(grid);
    v->addSpacing(6);
    auto *buttons = new QHBoxLayout;
    buttons->addStretch(1);
    auto *close = new Button(QObject::tr("Close"), Button::Variant::Primary, &dlg);
    QObject::connect(close, &QAbstractButton::clicked, &dlg, &QDialog::accept);
    buttons->addWidget(close);
    v->addLayout(buttons);
    close->setFocus();
    dlg.exec();
}

} // namespace sct::ui
