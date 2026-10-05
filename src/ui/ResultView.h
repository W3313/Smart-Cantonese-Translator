#pragma once

#include "core/TranslationTypes.h"

#include <QFrame>
#include <QList>

class QLabel;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QToolButton;
class QVBoxLayout;
class QHBoxLayout;

namespace sct {

class SpeechController;

namespace ui {
class Banner;
class CollapsibleSection;
class SpeakButton;
}

// Right-hand pane: shows the translation with Jyutping, alternatives and
// notes, plus empty / loading / error states.
class ResultView : public QFrame
{
    Q_OBJECT

public:
    enum class Page { Empty, Loading, Result, Error };

    explicit ResultView(QWidget *parent = nullptr);

    void setSpeechController(SpeechController *controller);
    void setDirection(Direction direction);
    void setDisplayOptions(bool showJyutping, bool showAlternatives, bool showNotes);
    void setTextPointSize(int pointSize);

    void showEmpty();
    void showLoading();
    // fromHistory marks results restored from history (shown in the footer).
    void showResult(const TranslationResult &result, bool fromHistory = false);
    void showError(const TranslationError &error);
    void setStarred(bool starred);

    Page page() const { return m_page; }
    bool hasResult() const { return m_result.isValid(); }
    const TranslationResult &result() const { return m_result; }

    // Introspection (tests, shortcuts).
    QString translationText() const;
    QString jyutpingText() const;
    QString literalText() const;
    bool isJyutpingShown() const;
    int alternativeCount() const;
    int noteCount() const;
    bool isStarred() const;
    ui::Banner *errorBanner() const { return m_errorBanner; }
    ui::SpeakButton *speakButton() const { return m_speak; }

    // Clipboard helpers (also used by the Ctrl+Shift+C shortcut).
    void copyTranslation();
    void copyWithJyutping();

signals:
    void cancelRequested();
    void retryRequested();
    void openSettingsRequested();
    void starToggled(bool starred);
    void exampleChosen(const QString &text);
    void statusMessage(const QString &message);

protected:
    void changeEvent(QEvent *event) override;

private:
    QWidget *buildEmptyPage();
    QWidget *buildLoadingPage();
    QWidget *buildResultPage();
    QWidget *buildErrorPage();
    void setPage(Page page);
    void rebuildExamples();
    void rebuildAlternatives();
    void rebuildNotes();
    void applyFonts();
    void applyVisibility();
    void updateStarButton();
    void updateFooter();

    Page m_page = Page::Empty;
    Direction m_direction = Direction::EnglishToCantonese;
    TranslationResult m_result;
    bool m_fromHistory = false;
    bool m_showJyutping = true;
    bool m_showAlternatives = true;
    bool m_showNotes = true;
    int m_pointSize = 13;
    SpeechController *m_speech = nullptr;

    QLabel *m_paneTitle = nullptr;
    QLabel *m_badge = nullptr;
    QStackedWidget *m_stack = nullptr;

    // Empty page
    QLabel *m_emptyTitle = nullptr;
    QHBoxLayout *m_examplesRow = nullptr;
    QList<QPushButton *> m_exampleButtons;

    // Loading page
    QPushButton *m_cancel = nullptr;

    // Result page
    QScrollArea *m_scroll = nullptr;
    QLabel *m_translation = nullptr;
    QLabel *m_jyutpingCaption = nullptr;
    QLabel *m_jyutping = nullptr;
    QLabel *m_literal = nullptr;
    ui::SpeakButton *m_speak = nullptr;
    QToolButton *m_copy = nullptr;
    QToolButton *m_copyJyutping = nullptr;
    QToolButton *m_star = nullptr;
    ui::CollapsibleSection *m_altSection = nullptr;
    ui::CollapsibleSection *m_notesSection = nullptr;
    QLabel *m_footer = nullptr;
    QList<QWidget *> m_altCards;
    QList<QLabel *> m_altTextLabels;
    QList<QLabel *> m_altJyutpingLabels;
    QList<QLabel *> m_noteLabels;

    // Error page
    ui::Banner *m_errorBanner = nullptr;
};

} // namespace sct
