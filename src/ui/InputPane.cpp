#include "ui/InputPane.h"

#include "ui/Motion.h"
#include "ui/SpeechController.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QTextCursor>
#include <QTimer>
#include <QVBoxLayout>

namespace sct {

using ui::IconTone;

namespace {

constexpr int kSoftLimit = 5000;

} // namespace

InputPane::InputPane(QWidget *parent)
    : ui::Card(parent)
    , m_languageCheck(new QTimer(this))
{
    setObjectName(QStringLiteral("inputCard"));
    setFocusHighlight(true);

    auto *layout = new QVBoxLayout(this);
    const QMargins sm = shadowMargins();
    layout->setContentsMargins(sm.left() + 8, sm.top() + 10, sm.right() + 14, sm.bottom() + 12);
    layout->setSpacing(6);

    m_edit = new QPlainTextEdit(this);
    m_edit->setObjectName(QStringLiteral("sourceEdit"));
    m_edit->setFrameShape(QFrame::NoFrame);
    m_edit->setTabChangesFocus(true);
    m_edit->setAccessibleName(tr("Text to translate"));
    m_edit->document()->setDocumentMargin(12);
    m_edit->viewport()->setAutoFillBackground(false);
    m_edit->setAutoFillBackground(false);
    layout->addWidget(m_edit, 1);

    // "This looks like Chinese - translate from Cantonese instead"
    m_hintRow = new QWidget(this);
    auto *hint = new QHBoxLayout(m_hintRow);
    hint->setContentsMargins(12, 0, 0, 0);
    hint->setSpacing(6);
    hint->addWidget(new ui::IconLabel(QStringLiteral("info"), IconTone::Accent, 16, m_hintRow));
    m_hintText = new QLabel(m_hintRow);
    m_hintText->setProperty("role", QStringLiteral("muted"));
    m_hintText->setWordWrap(true);
    hint->addWidget(m_hintText, 1);
    m_hintButton = new ui::Button(QString(), ui::Button::Variant::Ghost, m_hintRow);
    m_hintButton->setObjectName(QStringLiteral("directionHintButton"));
    connect(m_hintButton, &QAbstractButton::clicked, this, &InputPane::switchDirectionRequested);
    hint->addWidget(m_hintButton);
    m_hintRow->hide();
    layout->addWidget(m_hintRow);

    auto *bottom = new QHBoxLayout;
    bottom->setContentsMargins(12, 0, 0, 0);
    bottom->setSpacing(2);
    m_counter = new QLabel(this);
    m_counter->setObjectName(QStringLiteral("charCounter"));
    m_counter->setProperty("role", QStringLiteral("muted"));
    m_counter->setFont(Theme::uiFont(9));
    bottom->addWidget(m_counter);
    bottom->addStretch(1);

    m_paste = new ui::IconButton(QStringLiteral("paste"), tr("Paste"), this, IconTone::Muted);
    m_paste->setObjectName(QStringLiteral("pasteButton"));
    connect(m_paste, &QAbstractButton::clicked, this, [this] {
        const QString clip = QApplication::clipboard()->text();
        if (clip.isEmpty())
            return;
        m_edit->insertPlainText(clip);
        m_edit->setFocus();
    });
    bottom->addWidget(m_paste);

    m_clear = new ui::IconButton(QStringLiteral("clear"), tr("Clear"), this, IconTone::Muted);
    m_clear->setObjectName(QStringLiteral("clearButton"));
    connect(m_clear, &QAbstractButton::clicked, this, [this] {
        motion::crossFade(m_edit, motion::kFast);
        m_edit->clear();
        m_edit->setFocus();
        emit cleared();
    });
    bottom->addWidget(m_clear);

    m_speak = new ui::SpeakButton(this);
    m_speak->setObjectName(QStringLiteral("speakInputButton"));
    m_speak->setIdleToolTip(tr("Listen to your text"));
    m_speak->setEnabled(false);
    bottom->addWidget(m_speak);
    bottom->addSpacing(10);

    for (ui::IconButton *b : {static_cast<ui::IconButton *>(m_paste), static_cast<ui::IconButton *>(m_clear),
                              static_cast<ui::IconButton *>(m_speak)})
        addRevealWidget(b);

    const QKeySequence translateKey(Qt::CTRL | Qt::Key_Return);
    m_translate = new ui::Button(tr("Translate"), ui::Button::Variant::Primary, this);
    m_translate->setObjectName(QStringLiteral("translateButton"));
    m_translate->setTrailingIcon(QStringLiteral("translate"));
    m_translate->setBusy(false, tr("Cancel"));
    m_translate->setToolTip(ui::withShortcut(tr("Translate"), translateKey));
    connect(m_translate, &QAbstractButton::clicked, this, [this] {
        if (m_translate->isBusy())
            emit cancelRequested();
        else
            emit translateRequested();
    });
    bottom->addWidget(m_translate);
    layout->addLayout(bottom);

    m_languageCheck->setSingleShot(true);
    m_languageCheck->setInterval(400);
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

void InputPane::setBusy(bool busy)
{
    m_translate->setBusy(busy, tr("Cancel"));
    m_translate->setToolTip(busy ? ui::withShortcut(tr("Cancel translation"), QKeySequence(Qt::Key_Escape))
                                 : ui::withShortcut(tr("Translate"), QKeySequence(Qt::CTRL | Qt::Key_Return)));
    m_translate->setAccessibleName(busy ? tr("Cancel translation") : tr("Translate"));
}

bool InputPane::isBusy() const { return m_translate->isBusy(); }

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
    m_counter->setText(n == 0 ? QString() : n == 1 ? tr("1 character") : tr("%L1 characters").arg(n));
    const bool over = n > kSoftLimit;
    ui::setStyleProperty(m_counter, "role", over ? QStringLiteral("warning") : QStringLiteral("muted"));
    m_counter->setToolTip(over ? tr("Long texts take longer and cost more to translate.") : QString());
    m_clear->setEnabled(n > 0);
}

void InputPane::updatePlaceholder()
{
    m_edit->setPlaceholderText(m_direction == Direction::EnglishToCantonese
                                   ? tr("Type or paste English…")
                                   : QStringLiteral("輸入廣東話…  ") + tr("Type or paste Cantonese"));
}

void InputPane::updateFont()
{
    m_edit->setFont(Theme::textFont(sourceLanguage(m_direction), m_pointSize * 1.15, m_script));
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
        m_hintText->setText(tr("This looks like Cantonese."));
        m_hintButton->setText(tr("Switch languages"));
        m_hintButton->setToolTip(tr("Translate from Cantonese to English instead"));
        show = true;
    } else if (m_direction == Direction::CantoneseToEnglish && han == 0 && latin >= 8) {
        // Jyutping input ("nei5 hou2") is fine as Cantonese.
        static const QRegularExpression jyutping(QStringLiteral("\\b[a-z]+[1-6]\\b"),
                                                 QRegularExpression::CaseInsensitiveOption);
        static const QRegularExpression spaces(QStringLiteral("\\s+"));
        const qsizetype syllables = text.count(jyutping);
        const qsizetype words = text.split(spaces, Qt::SkipEmptyParts).size();
        if (syllables * 2 < words) {
            m_hintText->setText(tr("This looks like English."));
            m_hintButton->setText(tr("Switch languages"));
            m_hintButton->setToolTip(tr("Translate from English to Cantonese instead"));
            show = true;
        }
    }
    m_hintButton->updateGeometry();
    if (show && m_hintRow->isHidden()) {
        m_hintRow->show();
        motion::fadeIn(m_hintRow, motion::kNormal);
    } else if (!show) {
        m_hintRow->hide();
    }
}

} // namespace sct
