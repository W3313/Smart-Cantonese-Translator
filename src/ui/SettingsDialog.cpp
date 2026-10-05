#include "ui/SettingsDialog.h"

#include "core/AppSettings.h"
#include "core/TranslationService.h"
#include "tts/SpeechService.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSlider>
#include <QStackedWidget>
#include <QTabWidget>
#include <QVBoxLayout>

namespace sct {

using ui::IconTone;

namespace {

const QString kClaude = QStringLiteral("claude");
const QString kOpenAi = QStringLiteral("openai");
const QString kSystem = QStringLiteral("system");
const QString kAzure = QStringLiteral("azure");
const QStringList kQualityIds = {QStringLiteral("fast"), QStringLiteral("balanced"), QStringLiteral("best")};
const QStringList kThemeIds = {QStringLiteral("system"), QStringLiteral("light"), QStringLiteral("dark")};

QLabel *caption(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setProperty("role", QStringLiteral("muted"));
    l->setWordWrap(true);
    l->setFont(Theme::uiFont(9));
    return l;
}

QLabel *sectionTitle(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setFont(Theme::uiFont(10.5, QFont::DemiBold));
    return l;
}

QLabel *linkLabel(const QString &text, const QString &url, QWidget *parent)
{
    auto *l = new QLabel(QStringLiteral("<a href=\"%1\">%2</a> ↗").arg(url.toHtmlEscaped(), text.toHtmlEscaped()), parent);
    l->setTextFormat(Qt::RichText);
    l->setOpenExternalLinks(true);
    l->setTextInteractionFlags(Qt::TextBrowserInteraction);
    l->setToolTip(url);
    return l;
}

// Each tab scrolls if the window is short; the tab pane paints the surface.
QWidget *scrollable(QWidget *content, QWidget *parent)
{
    auto *scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);
    content->setAutoFillBackground(false);
    scroll->setWidget(content);
    return scroll;
}

QString providerName(TranslationService *svc, const QString &id)
{
    QString name = svc ? svc->providerDisplayName(id) : QString();
    if (name.isEmpty())
        name = id == kClaude ? QStringLiteral("Claude (Anthropic)") : id == kOpenAi ? QStringLiteral("OpenAI") : id;
    return name;
}

QString keyUrl(const QString &providerId)
{
    return providerId == kOpenAi ? QStringLiteral("https://platform.openai.com/api-keys")
                                 : QStringLiteral("https://console.anthropic.com/settings/keys");
}

int langKey(Language lang) { return lang == Language::Cantonese ? 1 : 0; }

} // namespace

SettingsDialog::SettingsDialog(AppSettings *settings, TranslationService *translation, SpeechService *speech,
                               QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
    , m_translation(translation)
    , m_speech(speech)
{
    setObjectName(QStringLiteral("settingsDialog"));
    setWindowTitle(tr("Settings"));
    setMinimumSize(600, 540);
    resize(660, 620);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    m_tabs = new QTabWidget(this);
    m_tabs->setDocumentMode(false);
    m_tabs->addTab(buildAiTab(), ui::icon(QStringLiteral("key")), tr("AI"));
    m_tabs->addTab(buildTranslationTab(), ui::icon(QStringLiteral("globe")), tr("Translation"));
    m_tabs->addTab(buildSpeechTab(), ui::icon(QStringLiteral("speaker")), tr("Speech"));
    m_tabs->addTab(buildAppearanceTab(), ui::icon(QStringLiteral("palette")), tr("Appearance"));
    layout->addWidget(m_tabs, 1);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
    m_buttons->button(QDialogButtonBox::Ok)->setProperty("primary", true);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);
    connect(m_buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &SettingsDialog::apply);
    layout->addWidget(m_buttons);

    if (m_translation) {
        connect(m_translation, &TranslationService::modelsListed, this,
                [this](const QString &providerId, const QStringList &models) {
                    auto it = m_providerWidgets.find(providerId);
                    if (it == m_providerWidgets.end())
                        return;
                    it->test->setEnabled(true);
                    populateModels(providerId, models);
                    setStatus(it->status,
                              models.isEmpty() ? tr("✓ Connected")
                                               : tr("✓ Connected · %n model(s) available", nullptr, int(models.size())),
                              QStringLiteral("success"));
                });
        connect(m_translation, &TranslationService::modelsListFailed, this,
                [this](const QString &providerId, const TranslationError &error) {
                    auto it = m_providerWidgets.find(providerId);
                    if (it == m_providerWidgets.end())
                        return;
                    it->test->setEnabled(true);
                    const QString msg = error.message.isEmpty() ? tr("Connection failed") : error.message;
                    setStatus(it->status, tr("✗ %1").arg(msg), QStringLiteral("error"), error.detail);
                });
    }
    if (m_speech) {
        connect(m_speech, &SpeechService::azureTestFinished, this, [this](bool ok, const QString &message) {
            m_azureTest->setEnabled(true);
            const QString text = message.isEmpty() ? (ok ? tr("Azure voice works") : tr("Azure test failed")) : message;
            setStatus(m_azureStatus, (ok ? QStringLiteral("✓ ") : QStringLiteral("✗ ")) + text,
                      ok ? QStringLiteral("success") : QStringLiteral("error"));
        });
        connect(m_speech, &SpeechService::voicesChanged, this, [this] {
            populateVoices();
            updateCantoneseVoiceStatus();
        });
    }

    load();
}

// ---- Tabs --------------------------------------------------------------------------

QWidget *SettingsDialog::buildAiTab()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(20, 18, 20, 18);
    v->setSpacing(10);

    v->addWidget(sectionTitle(tr("AI provider"), page));
    v->addWidget(caption(tr("Translations are written by a large language model. Choose which service to use "
                            "and paste your API key."),
                         page));

    m_providerIds = m_translation ? m_translation->providerIds() : QStringList();
    if (m_providerIds.isEmpty())
        m_providerIds = {kClaude, kOpenAi};

    auto *radios = new QHBoxLayout;
    radios->setSpacing(18);
    m_providerGroup = new QButtonGroup(page);
    m_providerStack = new QStackedWidget(page);
    for (int i = 0; i < m_providerIds.size(); ++i) {
        const QString &id = m_providerIds.at(i);
        auto *radio = new QRadioButton(providerName(m_translation, id), page);
        radio->setObjectName(QStringLiteral("provider_%1").arg(id));
        m_providerGroup->addButton(radio, i);
        radios->addWidget(radio);
        m_providerStack->addWidget(buildProviderPage(id));
        watch(radio);
    }
    radios->addStretch(1);
    v->addLayout(radios);
    connect(m_providerGroup, &QButtonGroup::idToggled, this, [this](int id, bool on) {
        if (on)
            m_providerStack->setCurrentIndex(id);
    });
    v->addWidget(m_providerStack);

    v->addSpacing(6);
    v->addWidget(sectionTitle(tr("Quality"), page));
    m_quality = new ui::SegmentedControl(page);
    m_quality->setProperty("qualityControl", true);
    m_quality->addSegment(tr("Fast"), tr("Quickest answers - great for everyday phrases"));
    m_quality->addSegment(tr("Balanced"), tr("Good balance of speed and nuance (recommended)"));
    m_quality->addSegment(tr("Best"), tr("Most natural, nuanced Cantonese - slower and costs a bit more"));
    v->addWidget(m_quality, 0, Qt::AlignLeft);
    m_qualityNote = caption(QString(), page);
    v->addWidget(m_qualityNote);
    connect(m_quality, &ui::SegmentedControl::currentIndexChanged, this, [this] {
        updateQualityNote();
        markDirty();
    });

    v->addStretch(1);
    auto *privacy = caption(tr("Your text is sent to the selected provider to be translated. API keys are stored "
                               "encrypted on this computer (Windows DPAPI) and only sent to that provider."),
                            page);
    v->addWidget(privacy);
    return scrollable(page, this);
}

QWidget *SettingsDialog::buildProviderPage(const QString &providerId)
{
    auto *page = new QWidget;
    auto *form = new QFormLayout(page);
    form->setContentsMargins(0, 8, 0, 0);
    form->setHorizontalSpacing(14);
    form->setVerticalSpacing(8);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    ProviderWidgets w;
    auto *keyRow = new QHBoxLayout;
    keyRow->setSpacing(8);
    w.key = new ui::PasswordLineEdit(page);
    w.key->setObjectName(QStringLiteral("%1Key").arg(providerId));
    w.key->setPlaceholderText(providerId == kOpenAi ? QStringLiteral("sk-…") : QStringLiteral("sk-ant-…"));
    w.key->setMinimumWidth(260);
    keyRow->addWidget(w.key, 1);
    w.test = new QPushButton(tr("Test connection"), page);
    w.test->setObjectName(QStringLiteral("%1Test").arg(providerId));
    w.test->setCursor(Qt::PointingHandCursor);
    w.test->setToolTip(tr("Checks the key and loads the models available to it. Costs nothing."));
    keyRow->addWidget(w.test);
    form->addRow(tr("API key"), keyRow);

    w.status = caption(QString(), page);
    w.status->setObjectName(QStringLiteral("%1Status").arg(providerId));
    w.status->hide();
    form->addRow(QString(), w.status);

    w.model = new QComboBox(page);
    w.model->setObjectName(QStringLiteral("%1Model").arg(providerId));
    w.model->setEditable(true);
    w.model->setInsertPolicy(QComboBox::NoInsert);
    w.model->setMinimumWidth(260);
    form->addRow(tr("Model"), w.model);
    const QString def = m_translation ? m_translation->defaultModel(providerId) : QString();
    if (!def.isEmpty())
        form->addRow(QString(), caption(tr("Recommended: %1. Click Test connection to see every model your key can use.")
                                            .arg(def),
                                        page));

    form->addRow(QString(), linkLabel(providerId == kOpenAi ? tr("Get an OpenAI API key") : tr("Get a Claude API key"),
                                      keyUrl(providerId), page));

    connect(w.test, &QPushButton::clicked, this, [this, providerId] { testProvider(providerId); });
    watch(w.key);
    watch(w.model);
    m_providerWidgets.insert(providerId, w);
    return page;
}

QWidget *SettingsDialog::buildTranslationTab()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(20, 18, 20, 18);
    v->setSpacing(10);

    v->addWidget(sectionTitle(tr("Chinese characters"), page));
    m_scriptGroup = new QButtonGroup(page);
    auto *trad = new QRadioButton(tr("Traditional  繁體字  (as used in Hong Kong)"), page);
    trad->setObjectName(QStringLiteral("scriptTraditional"));
    auto *simp = new QRadioButton(tr("Simplified  简体字"), page);
    simp->setObjectName(QStringLiteral("scriptSimplified"));
    for (auto *r : {trad, simp}) {
        r->setFont(Theme::textFont(Language::Cantonese, Theme::uiFont().pointSizeF()));
        watch(r);
    }
    m_scriptGroup->addButton(trad, int(ChineseScript::Traditional));
    m_scriptGroup->addButton(simp, int(ChineseScript::Simplified));
    v->addWidget(trad);
    v->addWidget(simp);

    v->addSpacing(10);
    v->addWidget(sectionTitle(tr("Show with each translation"), page));
    m_showJyutping = new QCheckBox(tr("Jyutping pronunciation (e.g. nei5 hou2)"), page);
    m_showJyutping->setObjectName(QStringLiteral("showJyutping"));
    m_showAlternatives = new QCheckBox(tr("Other ways to say it"), page);
    m_showAlternatives->setObjectName(QStringLiteral("showAlternatives"));
    m_showNotes = new QCheckBox(tr("Notes on slang, particles and culture"), page);
    m_showNotes->setObjectName(QStringLiteral("showNotes"));
    for (auto *c : {m_showJyutping, m_showAlternatives, m_showNotes}) {
        v->addWidget(c);
        watch(c);
    }
    v->addWidget(caption(tr("Turning off alternatives and notes makes translations a little faster and cheaper."), page));
    v->addStretch(1);
    return scrollable(page, this);
}

QWidget *SettingsDialog::buildSpeechTab()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(20, 18, 20, 18);
    v->setSpacing(8);

    v->addWidget(sectionTitle(tr("Voice engine"), page));
    m_engineIds = m_speech ? m_speech->engineIds() : QStringList();
    if (m_engineIds.isEmpty())
        m_engineIds = {kSystem, kAzure};
    m_engineGroup = new QButtonGroup(page);
    for (int i = 0; i < m_engineIds.size(); ++i) {
        const QString &id = m_engineIds.at(i);
        QString name = m_speech ? m_speech->engineDisplayName(id) : QString();
        if (name.isEmpty())
            name = id == kAzure ? tr("Azure neural voices") : tr("Windows voices");
        auto *radio = new QRadioButton(name, page);
        radio->setObjectName(QStringLiteral("engine_%1").arg(id));
        m_engineGroup->addButton(radio, i);
        v->addWidget(radio);
        auto *desc = caption(id == kAzure ? tr("Very natural voices such as HiuMaan (Cantonese). Needs an Azure Speech "
                                               "key; the free tier covers about 500,000 characters a month.")
                                          : tr("Free and works offline. Cantonese needs the Windows "
                                               "\"Chinese (Traditional, Hong Kong SAR)\" speech pack."),
                             page);
        desc->setContentsMargins(26, 0, 0, 4);
        v->addWidget(desc);
        watch(radio);
    }
    connect(m_engineGroup, &QButtonGroup::idToggled, this, [this](int, bool on) {
        if (on && !m_loading)
            updateEngineUi();
    });

    // Windows Cantonese voice status.
    auto *statusRow = new QHBoxLayout;
    statusRow->setContentsMargins(26, 0, 0, 0);
    m_windowsVoiceStatus = caption(QString(), page);
    m_windowsVoiceStatus->setObjectName(QStringLiteral("windowsVoiceStatus"));
    statusRow->addWidget(m_windowsVoiceStatus, 1);
    v->addLayout(statusRow);
    m_windowsVoiceHelp = caption(QString(), page);
    m_windowsVoiceHelp->setContentsMargins(26, 0, 0, 0);
    m_windowsVoiceHelp->setTextInteractionFlags(Qt::TextSelectableByMouse);
    v->addWidget(m_windowsVoiceHelp);

    // Voices for the selected engine.
    m_voicesBox = new QGroupBox(tr("Voices"), page);
    auto *voices = new QFormLayout(m_voicesBox);
    voices->setHorizontalSpacing(14);
    voices->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_englishVoice = new QComboBox(m_voicesBox);
    m_englishVoice->setObjectName(QStringLiteral("englishVoice"));
    m_cantoneseVoice = new QComboBox(m_voicesBox);
    m_cantoneseVoice->setObjectName(QStringLiteral("cantoneseVoice"));
    voices->addRow(tr("English"), m_englishVoice);
    voices->addRow(tr("Cantonese"), m_cantoneseVoice);

    auto *rateRow = new QHBoxLayout;
    rateRow->setSpacing(8);
    rateRow->addWidget(caption(tr("Slower"), m_voicesBox));
    m_rate = new QSlider(Qt::Horizontal, m_voicesBox);
    m_rate->setObjectName(QStringLiteral("speechRate"));
    m_rate->setRange(-10, 10);
    m_rate->setPageStep(2);
    m_rate->setTickPosition(QSlider::TicksBelow);
    m_rate->setTickInterval(5);
    rateRow->addWidget(m_rate, 1);
    rateRow->addWidget(caption(tr("Faster"), m_voicesBox));
    m_rateValue = new QLabel(m_voicesBox);
    m_rateValue->setMinimumWidth(56);
    m_rateValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    rateRow->addWidget(m_rateValue);
    voices->addRow(tr("Speed"), rateRow);
    v->addSpacing(4);
    v->addWidget(m_voicesBox);

    for (auto *combo : {m_englishVoice, m_cantoneseVoice}) {
        watch(combo);
        connect(combo, &QComboBox::currentIndexChanged, this, [this, combo](int) {
            if (m_loading)
                return;
            const Language lang = combo == m_cantoneseVoice ? Language::Cantonese : Language::English;
            m_voiceSelection[selectedEngine()][langKey(lang)] = combo->currentData().toString();
        });
    }
    connect(m_rate, &QSlider::valueChanged, this, [this] {
        updateRateLabel();
        markDirty();
    });

    m_autoSpeak = new QCheckBox(tr("Read translations aloud automatically"), page);
    m_autoSpeak->setObjectName(QStringLiteral("autoSpeak"));
    watch(m_autoSpeak);
    v->addWidget(m_autoSpeak);

    // Azure credentials.
    m_azureBox = new QGroupBox(tr("Azure Speech"), page);
    auto *azure = new QFormLayout(m_azureBox);
    azure->setHorizontalSpacing(14);
    azure->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    m_azureKey = new ui::PasswordLineEdit(m_azureBox);
    m_azureKey->setObjectName(QStringLiteral("azureKey"));
    m_azureKey->setPlaceholderText(tr("Key 1 or Key 2 from your Speech resource"));
    azure->addRow(tr("Key"), m_azureKey);
    m_azureRegion = new QComboBox(m_azureBox);
    m_azureRegion->setObjectName(QStringLiteral("azureRegion"));
    m_azureRegion->setEditable(true);
    m_azureRegion->setInsertPolicy(QComboBox::NoInsert);
    m_azureRegion->addItems({QStringLiteral("eastasia"), QStringLiteral("southeastasia"), QStringLiteral("japaneast"),
                             QStringLiteral("koreacentral"), QStringLiteral("australiaeast"), QStringLiteral("centralindia"),
                             QStringLiteral("eastus"), QStringLiteral("eastus2"), QStringLiteral("westus"),
                             QStringLiteral("westus2"), QStringLiteral("westus3"), QStringLiteral("centralus"),
                             QStringLiteral("canadacentral"), QStringLiteral("brazilsouth"), QStringLiteral("northeurope"),
                             QStringLiteral("westeurope"), QStringLiteral("uksouth"), QStringLiteral("francecentral"),
                             QStringLiteral("germanywestcentral"), QStringLiteral("swedencentral"),
                             QStringLiteral("switzerlandnorth")});
    m_azureRegion->setToolTip(tr("eastasia is Hong Kong. Use the region shown on your resource's \"Keys and Endpoint\" page."));
    azure->addRow(tr("Region"), m_azureRegion);
    auto *testRow = new QHBoxLayout;
    m_azureTest = new QPushButton(ui::icon(QStringLiteral("speaker")), tr("Test voice"), m_azureBox);
    m_azureTest->setObjectName(QStringLiteral("azureTest"));
    m_azureTest->setCursor(Qt::PointingHandCursor);
    testRow->addWidget(m_azureTest);
    m_azureStatus = caption(QString(), m_azureBox);
    m_azureStatus->setObjectName(QStringLiteral("azureStatus"));
    testRow->addWidget(m_azureStatus, 1);
    azure->addRow(QString(), testRow);
    azure->addRow(QString(), linkLabel(tr("Create a free Azure Speech resource"),
                                       QStringLiteral("https://portal.azure.com/#create/Microsoft.CognitiveServicesSpeechServices"),
                                       m_azureBox));
    v->addSpacing(4);
    v->addWidget(m_azureBox);
    watch(m_azureKey);
    watch(m_azureRegion);
    connect(m_azureTest, &QPushButton::clicked, this, &SettingsDialog::testAzure);

    v->addStretch(1);
    return scrollable(page, this);
}

QWidget *SettingsDialog::buildAppearanceTab()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(20, 18, 20, 18);
    v->setSpacing(10);

    v->addWidget(sectionTitle(tr("Theme"), page));
    m_theme = new ui::SegmentedControl(page);
    m_theme->addSegment(tr("System"), tr("Follow the Windows light/dark setting"));
    m_theme->addSegment(tr("Light"));
    m_theme->addSegment(tr("Dark"));
    m_theme->setProperty("themeControl", true);
    v->addWidget(m_theme, 0, Qt::AlignLeft);
    connect(m_theme, &ui::SegmentedControl::currentIndexChanged, this, &SettingsDialog::markDirty);

    v->addSpacing(10);
    v->addWidget(sectionTitle(tr("Text size"), page));
    auto *row = new QHBoxLayout;
    auto *small = new QLabel(QStringLiteral("A"), page);
    small->setFont(Theme::uiFont(9));
    row->addWidget(small);
    m_fontSize = new QSlider(Qt::Horizontal, page);
    m_fontSize->setObjectName(QStringLiteral("fontSize"));
    m_fontSize->setRange(10, 24);
    m_fontSize->setPageStep(2);
    row->addWidget(m_fontSize, 1);
    auto *big = new QLabel(QStringLiteral("A"), page);
    big->setFont(Theme::uiFont(15));
    row->addWidget(big);
    m_fontSizeValue = new QLabel(page);
    m_fontSizeValue->setMinimumWidth(48);
    m_fontSizeValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    row->addWidget(m_fontSizeValue);
    v->addLayout(row);

    m_fontPreview = new QLabel(page);
    m_fontPreview->setObjectName(QStringLiteral("fontPreview"));
    m_fontPreview->setWordWrap(true);
    m_fontPreview->setText(QStringLiteral("好耐冇見！你最近點呀？\nLong time no see! How have you been?"));
    m_fontPreview->setMinimumHeight(90);
    m_fontPreview->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    v->addWidget(m_fontPreview);
    connect(m_fontSize, &QSlider::valueChanged, this, [this] {
        updateFontPreview();
        markDirty();
    });

    v->addStretch(1);
    return scrollable(page, this);
}

// ---- Load / apply ------------------------------------------------------------------------

void SettingsDialog::load()
{
    m_loading = true;
    if (m_settings) {
        // AI
        const int providerIndex = qMax(0, int(m_providerIds.indexOf(m_settings->aiProvider())));
        if (auto *b = m_providerGroup->button(providerIndex))
            b->setChecked(true);
        m_providerStack->setCurrentIndex(providerIndex);
        for (const QString &id : std::as_const(m_providerIds)) {
            const ProviderWidgets &w = m_providerWidgets[id];
            w.key->setText(id == kOpenAi ? m_settings->openAiApiKey() : m_settings->claudeApiKey());
            populateModels(id, m_translation ? m_translation->suggestedModels(id) : QStringList());
            QString model = id == kOpenAi ? m_settings->openAiModel() : m_settings->claudeModel();
            if (model.isEmpty() && m_translation)
                model = m_translation->defaultModel(id);
            w.model->setCurrentText(model);
        }
        int quality = int(kQualityIds.indexOf(m_settings->quality()));
        if (quality < 0)
            quality = 1;  // balanced
        m_quality->setCurrentIndex(quality);
        updateQualityNote();

        // Translation
        if (auto *b = m_scriptGroup->button(int(m_settings->script())))
            b->setChecked(true);
        m_showJyutping->setChecked(m_settings->showJyutping());
        m_showAlternatives->setChecked(m_settings->showAlternatives());
        m_showNotes->setChecked(m_settings->showNotes());

        // Speech
        for (const QString &engine : std::as_const(m_engineIds)) {
            for (Language lang : {Language::English, Language::Cantonese}) {
                m_voiceSelection[engine][langKey(lang)] =
                    engine == kAzure ? m_settings->azureVoice(lang) : m_settings->systemVoice(lang);
            }
        }
        const int engineIndex = qMax(0, int(m_engineIds.indexOf(m_settings->speechEngine())));
        if (auto *b = m_engineGroup->button(engineIndex))
            b->setChecked(true);
        m_rate->setValue(qRound(m_settings->speechRate() * 10));
        m_autoSpeak->setChecked(m_settings->autoSpeak());
        m_azureKey->setText(m_settings->azureKey());
        m_azureRegion->setCurrentText(m_settings->azureRegion());

        // Appearance
        m_theme->setCurrentIndex(qMax(0, int(kThemeIds.indexOf(m_settings->theme()))));
        m_fontSize->setValue(m_settings->fontPointSize());
    }
    updateRateLabel();
    updateFontPreview();
    m_loading = false;
    updateEngineUi();
    updateCantoneseVoiceStatus();
    m_dirty = false;
    m_buttons->button(QDialogButtonBox::Apply)->setEnabled(false);
}

void SettingsDialog::apply()
{
    if (!m_settings)
        return;
    m_settings->beginBatch();

    m_settings->setAiProvider(selectedProvider());
    for (const QString &id : std::as_const(m_providerIds)) {
        const ProviderWidgets &w = m_providerWidgets[id];
        const QString key = w.key->text().trimmed();
        QString model = w.model->currentText().trimmed();
        if (model.isEmpty() && m_translation)
            model = m_translation->defaultModel(id);
        if (id == kOpenAi) {
            m_settings->setOpenAiApiKey(key);
            m_settings->setOpenAiModel(model);
        } else if (id == kClaude) {
            m_settings->setClaudeApiKey(key);
            m_settings->setClaudeModel(model);
        }
    }
    m_settings->setQuality(kQualityIds.value(m_quality->currentIndex(), QStringLiteral("balanced")));

    m_settings->setScript(m_scriptGroup->checkedId() == int(ChineseScript::Simplified) ? ChineseScript::Simplified
                                                                                       : ChineseScript::Traditional);
    m_settings->setShowJyutping(m_showJyutping->isChecked());
    m_settings->setShowAlternatives(m_showAlternatives->isChecked());
    m_settings->setShowNotes(m_showNotes->isChecked());

    m_settings->setSpeechEngine(selectedEngine());
    for (Language lang : {Language::English, Language::Cantonese}) {
        if (m_voiceSelection.contains(kSystem))
            m_settings->setSystemVoice(lang, m_voiceSelection[kSystem].value(langKey(lang)));
        if (m_voiceSelection.contains(kAzure))
            m_settings->setAzureVoice(lang, m_voiceSelection[kAzure].value(langKey(lang)));
    }
    m_settings->setSpeechRate(m_rate->value() / 10.0);
    m_settings->setAutoSpeak(m_autoSpeak->isChecked());
    m_settings->setAzureKey(m_azureKey->text().trimmed());
    const QString region = m_azureRegion->currentText().trimmed().toLower();
    if (!region.isEmpty())
        m_settings->setAzureRegion(region);

    m_settings->setTheme(kThemeIds.value(m_theme->currentIndex(), QStringLiteral("system")));
    m_settings->setFontPointSize(m_fontSize->value());

    m_settings->endBatch();
    m_settings->sync();
    m_dirty = false;
    m_buttons->button(QDialogButtonBox::Apply)->setEnabled(false);
}

void SettingsDialog::accept()
{
    if (m_dirty)
        apply();
    QDialog::accept();
}

void SettingsDialog::setCurrentTab(Tab tab) { m_tabs->setCurrentIndex(int(tab)); }

SettingsDialog::Tab SettingsDialog::currentTab() const { return static_cast<Tab>(m_tabs->currentIndex()); }

// ---- Helpers ---------------------------------------------------------------------------------

void SettingsDialog::markDirty()
{
    if (m_loading)
        return;
    m_dirty = true;
    m_buttons->button(QDialogButtonBox::Apply)->setEnabled(true);
}

void SettingsDialog::watch(QWidget *w)
{
    if (auto *b = qobject_cast<QAbstractButton *>(w))
        connect(b, &QAbstractButton::toggled, this, &SettingsDialog::markDirty);
    else if (auto *c = qobject_cast<QComboBox *>(w))
        connect(c, &QComboBox::currentTextChanged, this, &SettingsDialog::markDirty);
    else if (auto *e = qobject_cast<QLineEdit *>(w))
        connect(e, &QLineEdit::textEdited, this, &SettingsDialog::markDirty);
}

void SettingsDialog::setStatus(QLabel *label, const QString &text, const QString &role, const QString &toolTip)
{
    label->setText(text);
    label->setToolTip(toolTip);
    ui::setStyleProperty(label, "role", role);
    label->setVisible(!text.isEmpty());
}

QString SettingsDialog::selectedProvider() const
{
    return m_providerIds.value(qMax(0, m_providerGroup->checkedId()), kClaude);
}

QString SettingsDialog::selectedEngine() const
{
    return m_engineIds.value(qMax(0, m_engineGroup->checkedId()), kSystem);
}

void SettingsDialog::populateModels(const QString &providerId, const QStringList &models)
{
    QComboBox *combo = m_providerWidgets[providerId].model;
    if (!combo)
        return;
    const QString current = combo->currentText();
    const bool wasLoading = m_loading;
    m_loading = true;
    combo->clear();
    combo->addItems(models);
    if (!current.isEmpty())
        combo->setCurrentText(current);
    m_loading = wasLoading;
}

void SettingsDialog::populateVoices()
{
    const bool wasLoading = m_loading;
    m_loading = true;
    const QString engine = selectedEngine();
    for (Language lang : {Language::English, Language::Cantonese}) {
        QComboBox *combo = lang == Language::Cantonese ? m_cantoneseVoice : m_englishVoice;
        const QString selected = m_voiceSelection[engine].value(langKey(lang));
        combo->clear();
        combo->addItem(tr("Automatic"), QString());
        const QList<VoiceInfo> voices = m_speech ? m_speech->voices(engine, lang) : QList<VoiceInfo>();
        for (const VoiceInfo &voice : voices) {
            QString label = voice.name.isEmpty() ? voice.id : voice.name;
            if (!voice.locale.isEmpty())
                label += QStringLiteral("  ·  %1").arg(voice.locale);
            combo->addItem(label, voice.id);
        }
        int index = selected.isEmpty() ? 0 : combo->findData(selected);
        if (index < 0) {
            // Keep a saved voice that is not currently installed/listed.
            combo->addItem(tr("%1 (not available)").arg(selected), selected);
            index = combo->count() - 1;
        }
        combo->setCurrentIndex(index);
        if (lang == Language::Cantonese && voices.isEmpty() && engine == kSystem)
            combo->setToolTip(tr("No Cantonese voice is installed in Windows."));
        else
            combo->setToolTip(QString());
    }
    m_loading = wasLoading;
}

void SettingsDialog::updateEngineUi()
{
    populateVoices();
    const bool azure = selectedEngine() == kAzure;
    m_voicesBox->setTitle(azure ? tr("Azure voices") : tr("Windows voices"));
    m_azureBox->setEnabled(true);
    m_azureBox->setToolTip(azure ? QString() : tr("Used when \"Azure neural voices\" is selected above."));
    updateCantoneseVoiceStatus();
}

void SettingsDialog::updateCantoneseVoiceStatus()
{
    const QList<VoiceInfo> voices = m_speech ? m_speech->voices(kSystem, Language::Cantonese) : QList<VoiceInfo>();
    if (!voices.isEmpty()) {
        const QString name = voices.first().name.isEmpty() ? voices.first().id : voices.first().name;
        setStatus(m_windowsVoiceStatus, tr("✓ Windows Cantonese voice installed: %1").arg(name), QStringLiteral("success"));
        m_windowsVoiceHelp->hide();
    } else {
        setStatus(m_windowsVoiceStatus, tr("⚠ No Cantonese voice is installed in Windows."), QStringLiteral("warning"));
        m_windowsVoiceHelp->setText(SpeechService::cantoneseVoiceHelpText());
        m_windowsVoiceHelp->setVisible(!m_windowsVoiceHelp->text().isEmpty() && selectedEngine() == kSystem);
    }
}

void SettingsDialog::updateQualityNote()
{
    switch (m_quality->currentIndex()) {
    case 0:
        m_qualityNote->setText(tr("Fast: quickest answers. Great for everyday phrases."));
        break;
    case 2:
        m_qualityNote->setText(tr("Best: the most natural and nuanced Cantonese, especially for slang and idioms. "
                                  "It is slower and costs a bit more per translation."));
        break;
    default:
        m_qualityNote->setText(tr("Balanced: a good mix of speed and nuance. Recommended for most people."));
        break;
    }
}

void SettingsDialog::updateRateLabel()
{
    const int v = m_rate->value();
    m_rateValue->setText(v == 0 ? tr("Normal") : QStringLiteral("%1%2%").arg(v > 0 ? QStringLiteral("+") : QStringLiteral("−")).arg(qAbs(v) * 10));
}

void SettingsDialog::updateFontPreview()
{
    const int pt = m_fontSize->value();
    m_fontSizeValue->setText(tr("%1 pt").arg(pt));
    m_fontPreview->setFont(Theme::textFont(Language::Cantonese, pt));
}

void SettingsDialog::testProvider(const QString &providerId)
{
    ProviderWidgets &w = m_providerWidgets[providerId];
    if (!m_translation)
        return;
    const QString key = w.key->text().trimmed();
    if (key.isEmpty()) {
        setStatus(w.status, tr("Paste an API key first."), QStringLiteral("warning"));
        return;
    }
    w.test->setEnabled(false);
    setStatus(w.status, tr("Testing…"), QStringLiteral("muted"));
    m_translation->listModels(providerId, key);
}

void SettingsDialog::testAzure()
{
    if (!m_speech)
        return;
    const QString key = m_azureKey->text().trimmed();
    const QString region = m_azureRegion->currentText().trimmed().toLower();
    if (key.isEmpty() || region.isEmpty()) {
        setStatus(m_azureStatus, tr("Enter the key and region first."), QStringLiteral("warning"));
        return;
    }
    m_azureTest->setEnabled(false);
    setStatus(m_azureStatus, tr("Playing a sample…"), QStringLiteral("muted"));
    m_speech->testAzure(key, region, m_voiceSelection[kAzure].value(langKey(Language::Cantonese)));
}

} // namespace sct
