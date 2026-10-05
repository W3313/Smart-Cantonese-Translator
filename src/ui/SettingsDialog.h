#pragma once

#include "core/TranslationTypes.h"

#include <QDialog>
#include <QHash>
#include <QMap>

class QComboBox;
class QLabel;
class QSlider;
class QStackedWidget;
class QVBoxLayout;

namespace sct {

class AppSettings;
class SpeechService;
class TranslationService;

namespace ui {
class Button;
class Disclosure;
class IconLabel;
class PasswordLineEdit;
class SegmentedControl;
class ToggleSwitch;
}

class SettingsNav;

// Preferences with a sidebar (AI / Translation / Speech / Appearance).
// Changes are written on Save/Apply inside AppSettings::beginBatch()/endBatch();
// "Reduce motion" goes to UiPrefs.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Tab { AI = 0, Translation = 1, Speech = 2, Appearance = 3 };

    SettingsDialog(AppSettings *settings, TranslationService *translation, SpeechService *speech,
                   QWidget *parent = nullptr);

    void setCurrentTab(Tab tab);
    Tab currentTab() const;
    bool isDirty() const { return m_dirty; }

public slots:
    void apply();

protected:
    void accept() override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    struct ProviderWidgets
    {
        ui::PasswordLineEdit *key = nullptr;
        QComboBox *model = nullptr;
        ui::Button *test = nullptr;
        QLabel *status = nullptr;
    };

    QWidget *buildAiPage();
    QWidget *buildProviderGroup(const QString &providerId);
    QWidget *buildTranslationPage();
    QWidget *buildSpeechPage();
    QWidget *buildAppearancePage();
    void load();
    void markDirty();
    void watch(QObject *w);
    void setStatus(QLabel *label, const QString &text, const QString &role, const QString &toolTip = QString());

    QString selectedProvider() const;
    QString selectedEngine() const;
    void populateModels(const QString &providerId, const QStringList &models);
    void populateVoices();
    void updateEngineUi();
    void updateCantoneseVoiceStatus();
    void updateQualityNote();
    void updateRateLabel();
    void updateFontPreview();
    void testProvider(const QString &providerId);
    void testAzure();

    AppSettings *m_settings = nullptr;
    TranslationService *m_translation = nullptr;
    SpeechService *m_speech = nullptr;
    bool m_dirty = false;
    bool m_loading = false;

    SettingsNav *m_nav = nullptr;
    QLabel *m_pageTitle = nullptr;
    QStackedWidget *m_pages = nullptr;
    ui::Button *m_apply = nullptr;
    ui::Button *m_ok = nullptr;

    // AI
    ui::SegmentedControl *m_provider = nullptr;
    QStackedWidget *m_providerStack = nullptr;
    QStringList m_providerIds;
    QHash<QString, ProviderWidgets> m_providerWidgets;
    ui::SegmentedControl *m_quality = nullptr;
    QLabel *m_qualityNote = nullptr;

    // Translation
    ui::SegmentedControl *m_script = nullptr;
    ui::ToggleSwitch *m_showJyutping = nullptr;
    ui::ToggleSwitch *m_showAlternatives = nullptr;
    ui::ToggleSwitch *m_showNotes = nullptr;

    // Speech
    ui::SegmentedControl *m_engine = nullptr;
    QLabel *m_engineNote = nullptr;
    QStringList m_engineIds;
    QComboBox *m_englishVoice = nullptr;
    QComboBox *m_cantoneseVoice = nullptr;
    QSlider *m_rate = nullptr;
    QLabel *m_rateValue = nullptr;
    ui::ToggleSwitch *m_autoSpeak = nullptr;
    ui::PasswordLineEdit *m_azureKey = nullptr;
    QComboBox *m_azureRegion = nullptr;
    ui::Button *m_azureTest = nullptr;
    QLabel *m_azureStatus = nullptr;
    ui::IconLabel *m_windowsVoiceIcon = nullptr;
    QLabel *m_windowsVoiceStatus = nullptr;
    QLabel *m_windowsVoiceHelp = nullptr;
    ui::Disclosure *m_windowsVoiceHelpSection = nullptr;
    // engineId -> language -> voice id ("" = automatic)
    QMap<QString, QMap<int, QString>> m_voiceSelection;

    // Appearance
    ui::SegmentedControl *m_theme = nullptr;
    QSlider *m_fontSize = nullptr;
    QLabel *m_fontSizeValue = nullptr;
    QLabel *m_fontPreview = nullptr;
    ui::ToggleSwitch *m_reduceMotion = nullptr;
};

} // namespace sct
