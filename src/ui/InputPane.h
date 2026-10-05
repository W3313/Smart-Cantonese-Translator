#pragma once

#include "core/TranslationTypes.h"
#include "ui/Surfaces.h"

class QLabel;
class QPlainTextEdit;
class QTimer;

namespace sct {

class SpeechController;

// Left card: source text editor, character counter, hover-revealed paste /
// clear / listen actions and the primary Translate button (which morphs into
// "Cancel" while a translation runs).
class InputPane : public ui::Card
{
    Q_OBJECT

public:
    explicit InputPane(QWidget *parent = nullptr);

    void setDirection(Direction direction);
    Direction direction() const { return m_direction; }
    void setTextPointSize(int pointSize);
    void setScript(ChineseScript script);
    void setSpeechController(SpeechController *controller);
    void setBusy(bool busy);
    bool isBusy() const;

    QString text() const;
    void setText(const QString &text);
    void focusEditor();

    QPlainTextEdit *editor() const { return m_edit; }
    ui::Button *translateButton() const { return m_translate; }
    ui::SpeakButton *speakButton() const { return m_speak; }
    // True while the "looks like Cantonese/English - switch?" hint is shown.
    bool isDirectionHintVisible() const;

signals:
    void translateRequested();
    void cancelRequested();
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

    QPlainTextEdit *m_edit = nullptr;
    QWidget *m_hintRow = nullptr;
    QLabel *m_hintText = nullptr;
    ui::Button *m_hintButton = nullptr;
    QLabel *m_counter = nullptr;
    ui::IconButton *m_paste = nullptr;
    ui::IconButton *m_clear = nullptr;
    ui::SpeakButton *m_speak = nullptr;
    ui::Button *m_translate = nullptr;
    QTimer *m_languageCheck = nullptr;
};

} // namespace sct
