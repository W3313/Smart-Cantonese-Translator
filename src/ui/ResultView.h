#pragma once

#include "core/TranslationTypes.h"
#include "ui/Surfaces.h"

#include <QList>

class QHBoxLayout;
class QLabel;
class QScrollArea;
class QStackedWidget;
class QVBoxLayout;

namespace sct {

class SpeechController;

// Right card: the translation with Jyutping, alternatives and notes, plus
// empty / loading (shimmer skeleton) / error states. New results are revealed
// with a short staggered fade + upward slide.
class ResultView : public ui::Card
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
    void showResult(const TranslationResult &result, bool fromHistory = false, bool animated = true);
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
    ui::Disclosure *alternativesSection() const { return m_altSection; }
    ui::Disclosure *notesSection() const { return m_notesSection; }

    void copyTranslation();
    void copyWithJyutping();

signals:
    void retryRequested();
    void openSettingsRequested();
    void starToggled(bool starred);
    void exampleChosen(const QString &text);
    void statusMessage(const QString &message, const QString &iconName);

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
    void reveal();
    void flashCopied(ui::IconButton *button);

    Page m_page = Page::Empty;
    Direction m_direction = Direction::EnglishToCantonese;
    TranslationResult m_result;
    bool m_fromHistory = false;
    bool m_showJyutping = true;
    bool m_showAlternatives = true;
    bool m_showNotes = true;
    bool m_altExpanded = false;    // user's last choice, kept across results
    bool m_notesExpanded = false;
    int m_pointSize = 13;
    SpeechController *m_speech = nullptr;

    QStackedWidget *m_stack = nullptr;

    // Empty page
    QHBoxLayout *m_examplesRow = nullptr;
    QList<ui::Button *> m_exampleButtons;

    // Result page
    QScrollArea *m_scroll = nullptr;
    QVBoxLayout *m_contentLayout = nullptr;
    QLabel *m_translation = nullptr;
    QLabel *m_jyutpingCaption = nullptr;
    QLabel *m_jyutping = nullptr;
    QLabel *m_literal = nullptr;
    QWidget *m_actions = nullptr;
    ui::SpeakButton *m_speak = nullptr;
    ui::IconButton *m_copy = nullptr;
    ui::IconButton *m_copyJyutping = nullptr;
    ui::IconButton *m_star = nullptr;
    ui::Disclosure *m_altSection = nullptr;
    ui::Disclosure *m_notesSection = nullptr;
    QLabel *m_footer = nullptr;
    QList<QWidget *> m_altCards;
    QList<QLabel *> m_altTextLabels;
    QList<QLabel *> m_altJyutpingLabels;
    QList<QLabel *> m_altNoteLabels;
    QList<QLabel *> m_noteLabels;

    // Error page
    ui::Banner *m_errorBanner = nullptr;
};

} // namespace sct
