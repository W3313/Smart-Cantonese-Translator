#pragma once

#include "core/HistoryStore.h"
#include "core/TranslationTypes.h"
#include "ui/SettingsDialog.h"

#include <QMainWindow>
#include <QPointer>

class QDockWidget;
class QLabel;
class QSplitter;
class QToolButton;

namespace sct {

class AppSettings;
class HistoryPanel;
class InputPane;
class ResultView;
class SpeechController;
class SpeechService;
class TranslationService;

namespace ui {
class Banner;
class SegmentedControl;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(AppSettings *settings, TranslationService *translation, SpeechService *speech,
               QWidget *parent = nullptr);
    ~MainWindow() override;

    // ---- State & actions (also used by shortcuts and tests) -------------------
    Direction direction() const { return m_direction; }
    // Changes direction without moving text. A shown result for the other
    // direction is cleared.
    void setDirection(Direction direction);
    // Swaps direction; when a result is shown it moves into the input and is
    // translated back.
    void swapDirection();
    Tone tone() const { return m_tone; }
    void setTone(Tone tone);

    void setInputText(const QString &text);
    void translateNow();
    // Esc: cancels a running translation, otherwise stops speech.
    void cancelOrStop();
    void showResult(const TranslationResult &result, bool fromHistory = false);
    void showError(const TranslationError &error);
    void restoreHistoryEntry(const HistoryEntry &entry);
    void setHistoryVisible(bool visible);
    bool isHistoryVisible() const;

    // Opens (or raises) the non-blocking, window-modal Settings dialog.
    SettingsDialog *openSettings(SettingsDialog::Tab tab = SettingsDialog::Tab::AI);
    void showAbout();
    void showShortcuts();

    InputPane *inputPane() const { return m_input; }
    ResultView *resultView() const { return m_result; }
    HistoryPanel *historyPanel() const { return m_historyPanel; }
    QDockWidget *historyDock() const { return m_historyDock; }
    ui::Banner *welcomeBanner() const { return m_welcome; }
    ui::Banner *voiceHintBanner() const { return m_voiceHint; }
    ui::SegmentedControl *toneControl() const { return m_toneControl; }
    QToolButton *providerBadge() const { return m_providerBadge; }
    SpeechController *speechController() const { return m_speechController; }

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QWidget *buildHeader();
    void buildHistoryDock();
    void buildShortcuts();
    void connectServices();

    void updateDirectionUi();
    void updateProviderBadge();
    void updateWelcomeBanner();
    void updateLayoutForWidth();
    void applySettings();
    void refreshStarFromHistory();
    void onStarToggled(bool starred);
    void onFinished(const TranslationResult &result);
    void onFailed(const TranslationError &error);
    void onVoiceUnavailable(Language lang);
    void showStatus(const QString &message, int timeoutMs = 4000);
    QUuid findHistoryId(const TranslationResult &result) const;

    AppSettings *m_settings = nullptr;
    TranslationService *m_translation = nullptr;
    SpeechService *m_speech = nullptr;
    SpeechController *m_speechController = nullptr;

    Direction m_direction = Direction::EnglishToCantonese;
    Tone m_tone = Tone::Neutral;
    TranslationRequest m_lastRequest;
    bool m_voiceHintDismissed = false;
    bool m_welcomeDismissed = false;
    bool m_firstShow = true;
    QString m_appliedTheme;

    // Header
    QLabel *m_logo = nullptr;
    QLabel *m_appTitle = nullptr;
    QLabel *m_sourcePill = nullptr;
    QLabel *m_targetPill = nullptr;
    QToolButton *m_swap = nullptr;
    QLabel *m_toneLabel = nullptr;
    ui::SegmentedControl *m_toneControl = nullptr;
    QToolButton *m_providerBadge = nullptr;
    QToolButton *m_historyButton = nullptr;
    QToolButton *m_settingsButton = nullptr;
    QToolButton *m_helpButton = nullptr;

    // Body
    ui::Banner *m_welcome = nullptr;
    ui::Banner *m_voiceHint = nullptr;
    QSplitter *m_splitter = nullptr;
    InputPane *m_input = nullptr;
    ResultView *m_result = nullptr;
    QDockWidget *m_historyDock = nullptr;
    HistoryPanel *m_historyPanel = nullptr;

    QPointer<SettingsDialog> m_settingsDialog;
};

} // namespace sct
