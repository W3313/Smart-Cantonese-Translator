#pragma once

#include "core/TranslationTypes.h"

#include <QFrame>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTimer;
class QToolButton;

namespace sct {

class SpeechController;

namespace ui {
class SpeakButton;
}

// Left-hand pane: source text editor with character counter, paste/clear,
// read-aloud and the primary Translate button.
class InputPane : public QFrame
{
    Q_OBJECT

public:
    explicit InputPane(QWidget *parent = nullptr);

    void setDirection(Direction direction);
    Direction direction() const { return m_direction; }
    void setTextPointSize(int pointSize);
    void setScript(ChineseScript script);
    void setSpeechController(SpeechController *controller);

    QString text() const;
    void setText(const QString &text);
    void focusEditor();

    QPlainTextEdit *editor() const { return m_edit; }
    QPushButton *translateButton() const { return m_translate; }
    ui::SpeakButton *speakButton() const { return m_speak; }
    // True while the "looks like Cantonese/English - switch?" hint is shown.
    bool isDirectionHintVisible() const;

signals:
    void translateRequested();
    void switchDirectionRequested();
    void cleared();
    void textChanged();

private:
    void updateCounter();
    void updatePlaceholder();
    void updateFont();
    void checkLanguage();

    Direction m_direction = Direction::EnglishToCantonese;
    ChineseScript m_script = ChineseScript::Traditional;
    int m_pointSize = 13;

    QLabel *m_title = nullptr;
    ui::SpeakButton *m_speak = nullptr;
    QPlainTextEdit *m_edit = nullptr;
    QWidget *m_hintRow = nullptr;
    QLabel *m_hintText = nullptr;
    QPushButton *m_hintButton = nullptr;
    QLabel *m_counter = nullptr;
    QToolButton *m_paste = nullptr;
    QToolButton *m_clear = nullptr;
    QLabel *m_shortcutHint = nullptr;
    QPushButton *m_translate = nullptr;
    QTimer *m_languageCheck = nullptr;
};

} // namespace sct
