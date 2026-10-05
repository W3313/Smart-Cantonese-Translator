#include "ui/InputPane.h"

#include "ui/SpeechController.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QTextCursor>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace sct {

using ui::IconTone;

namespace {

constexpr int kSoftLimit = 5000;

} // namespace

InputPane::InputPane(QWidget *parent)
    : QFrame(parent)
    , m_languageCheck(new QTimer(this))
{
    setObjectName(QStringLiteral("pane"));
    setFrameShape(QFrame::NoFrame);
    setAttribute(Qt::WA_StyledBackground, true);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 12, 12, 12);
    layout->setSpacing(4);

    auto *header = new QHBoxLayout;
    header->setContentsMargins(16, 0, 0, 0);
    m_title = new QLabel(this);
    m_title->setProperty("role", QStringLiteral("paneTitle"));
    m_title->setFont(Theme::uiFont(-1, QFont::DemiBold));
    header->addWidget(m_title);
    header->addStretch(1);
    m_speak = new ui::SpeakButton(this);
    m_speak->setIdleToolTip(tr("Listen to your text"));
    m_speak->setEnabled(false);
    header->addWidget(m_speak);
    layout->addLayout(header);

    m_edit = new QPlainTextEdit(this);
    m_edit->setObjectName(QStringLiteral("sourceEdit"));
    m_edit->setFrameShape(QFrame::NoFrame);
    m_edit->setTabChangesFocus(true);
    m_edit->setAccessibleName(tr("Text to translate"));
    m_edit->document()->setDocumentMargin(12);
    layout->addWidget(m_edit, 1);

    // "Looks like Cantonese - switch?" hint.
    m_hintRow = new QWidget(this);
    auto *hint = new QHBoxLayout(m_hintRow);
    hint->setContentsMargins(16, 0, 0, 0);
    hint->setSpacing(6);
    hint->addWidget(new ui::IconLabel(QStringLiteral("info"), IconTone::Accent, 16, m_hintRow));
    m_hintText = new QLabel(m_hintRow);
    m_hintText->setObjectName(QStringLiteral("directionHint"));
    hint->addWidget(m_hintText);
    m_hintButton = new QPushButton(m_hintRow);
    m_hintButton->setProperty("link", true);
    m_hintButton->setCursor(Qt::PointingHandCursor);
    m_hintButton->setFlat(true);
    connect(m_hintButton, &QPushButton::clicked, this, &InputPane::switchDirectionRequested);
    hint->addWidget(m_hintButton);
    hint->addStretch(1);
    m_hintRow->hide();
    layout->addWidget(m_hintRow);

    auto *bottom = new QHBoxLayout;
    bottom->setContentsMargins(16, 4, 0, 0);
    bottom->setSpacing(4);
    m_counter = new QLabel(this);
    m_counter->setObjectName(QStringLiteral("charCounter"));
    m_counter->setProperty("role", QStringLiteral("muted"));
    bottom->addWidget(m_counter);
    bottom->addStretch(1);

    m_paste = ui::makeIconButton(QStringLiteral("paste"), tr("Paste"), this);
    connect(m_paste, &QToolButton::clicked, this, [this] {
        const QString clip = QApplication::clipboard()->text();
        if (clip.isEmpty())
            return;
        m_edit->insertPlainText(clip);
        m_edit->setFocus();
    });
    bottom->addWidget(m_paste);

    m_clear = ui::makeIconButton(QStringLiteral("clear"), tr("Clear"), this);
    connect(m_clear, &QToolButton::clicked, this, [this] {
        m_edit->clear();
        m_edit->setFocus();
        emit cleared();
    });
    bottom->addWidget(m_clear);
    bottom->addSpacing(8);

    const QKeySequence translateKey(Qt::CTRL | Qt::Key_Return);
    m_shortcutHint = new QLabel(translateKey.toString(QKeySequence::NativeText), this);
    m_shortcutHint->setProperty("role", QStringLiteral("caption"));
    m_shortcutHint->setFont(Theme::uiFont(8.5));
    bottom->addWidget(m_shortcutHint);
    bottom->addSpacing(4);

    m_translate = new QPushButton(tr("Translate"), this);
    m_translate->setObjectName(QStringLiteral("translateButton"));
    m_translate->setProperty("primary", true);
    m_translate->setIcon(ui::icon(QStringLiteral("translate"), IconTone::OnAccent));
    m_translate->setIconSize(QSize(18, 18));
    m_translate->setLayoutDirection(Qt::RightToLeft);  // arrow after the label
    m_translate->setCursor(Qt::PointingHandCursor);
    m_translate->setFont(Theme::uiFont(10.5, QFont::DemiBold));
    m_translate->setToolTip(ui::withShortcut(tr("Translate"), translateKey));
    connect(m_translate, &QPushButton::clicked, this, &InputPane::translateRequested);
    bottom->addWidget(m_translate);
    layout->addLayout(bottom);

    m_languageCheck->setSingleShot(true);
    m_languageCheck->setInterval(350);
    connect(m_languageCheck, &QTimer::timeout, this, &InputPane::checkLanguage);
    connect(m_edit, &QPlainTextEdit::textChanged, this, [this] {
        updateCounter();
        m_languageCheck->start();
        emit textChanged();
    });

    setFocusProxy(m_edit);
    updatePlaceholder();
    updateFont();
    updateCounter();
}

void InputPane::setDirection(Direction direction)
{
    m_direction = direction;
    m_title->setText(Theme::languageLabel(sourceLanguage(direction)));
    updatePlaceholder();
    updateFont();
    checkLanguage();
}

void InputPane::setTextPointSize(int pointSize)
{
    m_pointSize = qBound(8, pointSize, 40);
    updateFont();
}

void InputPane::setScript(ChineseScript script)
{
    m_script = script;
    updateFont();
}

void InputPane::setSpeechController(SpeechController *controller)
{
    m_speak->setEnabled(controller != nullptr);
    if (controller) {
        controller->attach(m_speak, [this] {
            return SpeechController::Utterance{m_edit->toPlainText(), sourceLanguage(m_direction)};
        });
    }
}

QString InputPane::text() const { return m_edit->toPlainText(); }

void InputPane::setText(const QString &text)
{
    m_edit->setPlainText(text);
    QTextCursor c = m_edit->textCursor();
    c.movePosition(QTextCursor::End);
    m_edit->setTextCursor(c);
}

void InputPane::focusEditor() { m_edit->setFocus(); }

bool InputPane::isDirectionHintVisible() const { return !m_hintRow->isHidden(); }

void InputPane::updateCounter()
{
    const qsizetype n = m_edit->toPlainText().size();
    m_counter->setText(n == 1 ? tr("1 character") : tr("%L1 characters").arg(n));
    const bool over = n > kSoftLimit;
    ui::setStyleProperty(m_counter, "over", over);
    m_counter->setToolTip(over ? tr("Long texts take longer and cost more to translate.") : QString());
    m_clear->setEnabled(n > 0);
}

void InputPane::updatePlaceholder()
{
    m_edit->setPlaceholderText(m_direction == Direction::EnglishToCantonese
                                   ? tr("Type or paste English…")
                                   : QStringLiteral("輸入廣東話…  ") + tr("(type or paste Cantonese)"));
}

void InputPane::updateFont()
{
    m_edit->setFont(Theme::textFont(sourceLanguage(m_direction), m_pointSize * 1.08, m_script));
}

void InputPane::checkLanguage()
{
    const QString text = m_edit->toPlainText();
    int han = 0;
    int latin = 0;
    for (const QChar ch : text) {
        if (ch.script() == QChar::Script_Han)
            ++han;
        else if (ch.isLetter() && ch.script() == QChar::Script_Latin)
            ++latin;
    }
    bool show = false;
    if (m_direction == Direction::EnglishToCantonese && han >= 2 && han > latin) {
        m_hintText->setText(tr("This looks like Chinese."));
        m_hintButton->setText(tr("Translate from Cantonese instead"));
        show = true;
    } else if (m_direction == Direction::CantoneseToEnglish && han == 0 && latin >= 8) {
        // Jyutping input ("nei5 hou2") is fine as Cantonese.
        static const QRegularExpression jyutping(QStringLiteral("\\b[a-z]+[1-6]\\b"),
                                                 QRegularExpression::CaseInsensitiveOption);
        const qsizetype syllables = text.count(jyutping);
        const qsizetype words = text.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts).size();
        if (syllables * 2 < words) {
            m_hintText->setText(tr("This looks like English."));
            m_hintButton->setText(tr("Translate from English instead"));
            show = true;
        }
    }
    m_hintRow->setVisible(show);
}

} // namespace sct
