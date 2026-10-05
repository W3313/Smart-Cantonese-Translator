#pragma once

#include "core/TranslationTypes.h"

#include <QDialog>
#include <QHash>
#include <QMap>

class QAbstractButton;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QSlider;
class QStackedWidget;
class QTabWidget;

namespace sct {

class AppSettings;
class SpeechService;
class TranslationService;

namespace ui {
class PasswordLineEdit;
class SegmentedControl;
}

// Preferences: AI provider + keys, translation display, speech, appearance.
// Changes are written on OK/Apply inside AppSettings::beginBatch()/endBatch().
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

private:
    struct ProviderWidgets
    {
        ui::PasswordLineEdit *key = nullptr;
        QComboBox *model = nullptr;
        QPushButton *test = nullptr;
        QLabel *status = nullptr;
    };

    QWidget *buildAiTab();
    QWidget *buildProviderPage(const QString &providerId);
    QWidget *buildTranslationTab();
    QWidget *buildSpeechTab();
    QWidget *buildAppearanceTab();
    void load();
    void markDirty();
    void watch(QWidget *w);
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

    QTabWidget *m_tabs = nullptr;
    QDialogButtonBox *m_buttons = nullptr;

    // AI
    QButtonGroup *m_providerGroup = nullptr;
    QStackedWidget *m_providerStack = nullptr;
    QStringList m_providerIds;
    QHash<QString, ProviderWidgets> m_providerWidgets;
    ui::SegmentedControl *m_quality = nullptr;
    QLabel *m_qualityNote = nullptr;

    // Translation
    QButtonGroup *m_scriptGroup = nullptr;
    QCheckBox *m_showJyutping = nullptr;
    QCheckBox *m_showAlternatives = nullptr;
    QCheckBox *m_showNotes = nullptr;

    // Speech
    QButtonGroup *m_engineGroup = nullptr;
    QStringList m_engineIds;
    QComboBox *m_englishVoice = nullptr;
    QComboBox *m_cantoneseVoice = nullptr;
    QGroupBox *m_voicesBox = nullptr;
    QSlider *m_rate = nullptr;
    QLabel *m_rateValue = nullptr;
    QCheckBox *m_autoSpeak = nullptr;
    QGroupBox *m_azureBox = nullptr;
    ui::PasswordLineEdit *m_azureKey = nullptr;
    QComboBox *m_azureRegion = nullptr;
    QPushButton *m_azureTest = nullptr;
    QLabel *m_azureStatus = nullptr;
    QLabel *m_windowsVoiceStatus = nullptr;
    QLabel *m_windowsVoiceHelp = nullptr;
    // engineId -> language -> voice id ("" = automatic)
    QMap<QString, QMap<int, QString>> m_voiceSelection;

    // Appearance
    ui::SegmentedControl *m_theme = nullptr;
    QSlider *m_fontSize = nullptr;
    QLabel *m_fontSizeValue = nullptr;
    QLabel *m_fontPreview = nullptr;
};

} // namespace sct
