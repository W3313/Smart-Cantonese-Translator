#pragma once

#include "core/HistoryStore.h"
#include "core/TranslationTypes.h"
#include "ui/SettingsDialog.h"

#include <QMainWindow>
#include <QPointer>

class QBoxLayout;
class QLabel;

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
class ButtonBase;
class DirectionPill;
class IconButton;
class SegmentedControl;
class SidePanel;
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
    // Changes direction without moving text; a shown result for the other
    // direction is cleared.
    void setDirection(Direction direction);
    // Swaps direction; a shown result moves into the input and is translated back.
    void swapDirection();
    Tone tone() const { return m_tone; }
    void setTone(Tone tone);

    void setInputText(const QString &text);
    void translateNow();
    // Esc: cancels a running translation, else stops speech, else closes history.
    void cancelOrStop();
    void showResult(const TranslationResult &result, bool fromHistory = false);
    void showError(const TranslationError &error);
    void restoreHistoryEntry(const HistoryEntry &entry);
    void setHistoryVisible(bool visible, bool animated = true);
    bool isHistoryVisible() const;

    // Opens (or raises) the non-blocking, window-modal Settings dialog.
    SettingsDialog *openSettings(SettingsDialog::Tab tab = SettingsDialog::Tab::AI);
    void showAbout();
    void showShortcuts();
    // Transient toast at the bottom of the window.
    void showStatus(const QString &message, const QString &iconName = QString());

    InputPane *inputPane() const { return m_input; }
    ResultView *resultView() const { return m_result; }
    HistoryPanel *historyPanel() const { return m_historyPanel; }
    ui::Banner *welcomeBanner() const { return m_welcome; }
    ui::Banner *voiceHintBanner() const { return m_voiceHint; }
    ui::SegmentedControl *toneControl() const { return m_toneControl; }
    ui::DirectionPill *directionPill() const { return m_directionPill; }
    ui::ButtonBase *providerChip() const { return m_providerChip; }
    QString providerChipText() const;
    SpeechController *speechController() const { return m_speechController; }

protected:
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QWidget *buildHeader();
    void buildShortcuts();
    void connectServices();

    void applyDirection(Direction direction, bool animated);
    void updateProviderChip();
    void updateWelcomeBanner(bool animated);
    void updateLayoutForWidth();
    void applySettings();
    void refreshStarFromHistory();
    void onStarToggled(bool starred);
    void onFinished(const TranslationResult &result);
    void onFailed(const TranslationError &error);
    void onVoiceUnavailable(Language lang);
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
    int m_appliedFontSize = -1;
    ChineseScript m_appliedScript = ChineseScript::Traditional;
    int m_appliedDisplay = -1;

    // Header
    QLabel *m_logo = nullptr;
    QLabel *m_appTitle = nullptr;
    ui::DirectionPill *m_directionPill = nullptr;
    ui::SegmentedControl *m_toneControl = nullptr;
    ui::ButtonBase *m_providerChip = nullptr;
    ui::IconButton *m_historyButton = nullptr;
    ui::IconButton *m_settingsButton = nullptr;
    ui::IconButton *m_moreButton = nullptr;

    // Body
    ui::Banner *m_welcome = nullptr;
    ui::Banner *m_voiceHint = nullptr;
    QBoxLayout *m_panesLayout = nullptr;
    InputPane *m_input = nullptr;
    ResultView *m_result = nullptr;
    ui::SidePanel *m_historySide = nullptr;
    HistoryPanel *m_historyPanel = nullptr;

    QPointer<SettingsDialog> m_settingsDialog;
};

} // namespace sct
