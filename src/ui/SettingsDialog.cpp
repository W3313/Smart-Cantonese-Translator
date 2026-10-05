#include "ui/SettingsDialog.h"

#include "core/AppSettings.h"
#include "core/TranslationService.h"
#include "tts/SpeechService.h"
#include "ui/Controls.h"
#include "ui/Motion.h"
#include "ui/Surfaces.h"
#include "ui/Theme.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSlider>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QVBoxLayout>

#include <functional>

namespace sct {

using ui::IconTone;

namespace {

const QString kClaude = QStringLiteral("claude");
const QString kOpenAi = QStringLiteral("openai");
const QString kSystem = QStringLiteral("system");
const QString kAzure = QStringLiteral("azure");
const QStringList kQualityIds = {QStringLiteral("fast"), QStringLiteral("balanced"), QStringLiteral("best")};
const QStringList kThemeIds = {QStringLiteral("system"), QStringLiteral("light"), QStringLiteral("dark")};

int langKey(Language lang) { return lang == Language::Cantonese ? 1 : 0; }

QLabel *caption(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setProperty("role", QStringLiteral("muted"));
    l->setWordWrap(true);
    l->setFont(Theme::uiFont(9));
    return l;
}

QLabel *linkLabel(const QString &text, const QString &url, QWidget *parent)
{
    auto *l = new QLabel(QStringLiteral("<a href=\"%1\" style=\"text-decoration:none\">%2&nbsp;↗</a>")
                             .arg(url.toHtmlEscaped(), text.toHtmlEscaped()),
                         parent);
    l->setTextFormat(Qt::RichText);
    l->setOpenExternalLinks(true);
    l->setTextInteractionFlags(Qt::TextBrowserInteraction);
    l->setToolTip(url);
    l->setFont(Theme::uiFont(9.5, QFont::Medium));
    return l;
}

QComboBox *makeCombo(QWidget *parent, bool editable)
{
    auto *c = new QComboBox(parent);
    c->setEditable(editable);
    if (editable)
        c->setInsertPolicy(QComboBox::NoInsert);
    c->setItemDelegate(new QStyledItemDelegate(c));  // lets the style sheet pad popup items
    c->setMinimumWidth(240);
    c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    return c;
}

// Rounded surface holding setting rows separated by hairlines.
class SettingsGroup : public QFrame
{
public:
    explicit SettingsGroup(QWidget *parent)
        : QFrame(parent)
        , m_layout(new QVBoxLayout(this))
    {
        m_layout->setContentsMargins(0, 0, 0, 0);
        m_layout->setSpacing(0);
    }

    void addRow(QWidget *row)
    {
        if (m_layout->count() > 0) {
            auto *line = ui::makeDivider(this);
            line->setContentsMargins(16, 0, 0, 0);
            m_layout->addWidget(line);
        }
        m_layout->addWidget(row);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        const ThemeColors &c = Theme::colors();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath path;
        path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 12, 12);
        p.fillPath(path, c.surface);
        p.setPen(QPen(c.border, 1));
        p.drawPath(path);
    }

private:
    QVBoxLayout *m_layout;
};

// Title + description on the left, control on the right (or below when
// stacked, e.g. for text fields).
QWidget *makeRow(const QString &title, const QString &description, QWidget *control, bool stacked = false,
                 QLabel **descriptionOut = nullptr)
{
    auto *row = new QWidget;
    auto *texts = new QVBoxLayout;
    texts->setSpacing(2);
    auto *t = new QLabel(title, row);
    t->setFont(Theme::uiFont(10, QFont::Medium));
    t->setWordWrap(true);
    texts->addWidget(t);
    QLabel *d = nullptr;
    if (!description.isNull()) {
        d = caption(description, row);
        texts->addWidget(d);
    }
    if (descriptionOut)
        *descriptionOut = d;
    if (control && !title.isEmpty())
        control->setAccessibleName(title);
    if (stacked) {
        auto *v = new QVBoxLayout(row);
        v->setContentsMargins(16, 14, 16, 14);
        v->setSpacing(10);
        v->addLayout(texts);
        if (control)
            v->addWidget(control);
    } else {
        auto *h = new QHBoxLayout(row);
        h->setContentsMargins(16, 12, 16, 12);
        h->setSpacing(16);
        h->addLayout(texts, 1);
        if (control)
            h->addWidget(control, 0, Qt::AlignVCenter | Qt::AlignRight);
    }
    return row;
}

QLabel *groupCaption(const QString &text, QWidget *parent)
{
    auto *l = new QLabel(text, parent);
    l->setProperty("role", QStringLiteral("muted"));
    l->setFont(Theme::uiFont(9, QFont::DemiBold));
    l->setContentsMargins(4, 10, 0, 2);
    return l;
}

QWidget *scrollPage(QWidget *content, QWidget *parent)
{
    auto *scroll = new QScrollArea(parent);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);
    scroll->setWidget(content);
    content->setAutoFillBackground(false);  // setWidget() switches it on
    return scroll;
}

} // namespace

// Sidebar navigation with an animated selection pill.
class SettingsNav : public QWidget
{
public:
    explicit SettingsNav(QWidget *parent)
        : QWidget(parent)
    {
        setFocusPolicy(Qt::StrongFocus);
        setMouseTracking(true);
        setAttribute(Qt::WA_Hover);
        setFixedWidth(196);
        setAccessibleName(QObject::tr("Settings sections"));
    }

    std::function<void(int)> onChanged;

    void addItem(const QString &icon, const QString &text)
    {
        m_items.append({icon, text});
        update();
    }
    int currentIndex() const { return m_current; }
    void setCurrentIndex(int index, bool animated)
    {
        if (index < 0 || index >= m_items.size())
            return;
        const qreal from = m_pillY;
        m_current = index;
        const qreal to = itemRect(index).top();
        motion::animate(this, QStringLiteral("pill"), from, to, animated ? motion::kNormal : 0,
                        [this](const QVariant &v) {
                            m_pillY = v.toReal();
                            update();
                        });
    }

protected:
    QRectF itemRect(int i) const { return QRectF(12, 16 + i * 40, width() - 24, 36); }

    int itemAt(const QPoint &pos) const
    {
        for (int i = 0; i < m_items.size(); ++i) {
            if (itemRect(i).contains(pos))
                return i;
        }
        return -1;
    }

    void select(int index)
    {
        if (index < 0 || index >= m_items.size() || index == m_current)
            return;
        setCurrentIndex(index, true);
        if (onChanged)
            onChanged(index);
    }

    void paintEvent(QPaintEvent *) override
    {
        const ThemeColors &c = Theme::colors();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        // Hairline separating the sidebar from the page.
        p.setPen(QPen(c.border, 1));
        p.drawLine(QPointF(width() - 0.5, 0), QPointF(width() - 0.5, height()));

        if (m_current >= 0) {
            const QRectF pill(12, m_pillY, width() - 24, 36);
            QPainterPath path;
            path.addRoundedRect(pill, 9, 9);
            p.fillPath(path, c.accentSoft);
            // Accent marker on the left edge of the pill.
            p.setPen(Qt::NoPen);
            p.setBrush(c.accent);
            p.drawRoundedRect(QRectF(pill.left() + 4, pill.center().y() - 8, 3, 16), 1.5, 1.5);
        }
        p.setFont(Theme::uiFont(10, QFont::Medium));
        for (int i = 0; i < m_items.size(); ++i) {
            const QRectF r = itemRect(i);
            const bool selected = i == m_current;
            if (i == m_hovered && !selected) {
                QPainterPath path;
                path.addRoundedRect(r, 9, 9);
                p.fillPath(path, c.hover);
            }
            const QColor fg = selected ? c.accent : c.text;
            p.drawPixmap(QPointF(r.left() + 16, r.center().y() - 9),
                         ui::iconPixmap(m_items.at(i).icon, fg, 18, devicePixelRatioF()));
            p.setPen(fg);
            p.drawText(r.adjusted(46, 0, -8, 0), Qt::AlignLeft | Qt::AlignVCenter, m_items.at(i).text);
            if (selected && hasFocus() && m_keyboard) {
                p.setPen(QPen(c.accent, 2));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(r.adjusted(1, 1, -1, -1), 9, 9);
            }
        }
    }

    void mouseMoveEvent(QMouseEvent *e) override
    {
        const int h = itemAt(e->position().toPoint());
        if (h != m_hovered) {
            m_hovered = h;
            setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor);
            update();
        }
    }
    void leaveEvent(QEvent *) override
    {
        m_hovered = -1;
        update();
    }
    void mousePressEvent(QMouseEvent *e) override
    {
        m_keyboard = false;
        select(itemAt(e->position().toPoint()));
    }
    void keyPressEvent(QKeyEvent *e) override
    {
        m_keyboard = true;
        if (e->key() == Qt::Key_Down)
            select(m_current + 1);
        else if (e->key() == Qt::Key_Up)
            select(m_current - 1);
        else
            QWidget::keyPressEvent(e);
        update();
    }
    void focusInEvent(QFocusEvent *e) override
    {
        m_keyboard = e->reason() == Qt::TabFocusReason || e->reason() == Qt::BacktabFocusReason;
        update();
    }
    void focusOutEvent(QFocusEvent *) override { update(); }
    void resizeEvent(QResizeEvent *) override { m_pillY = itemRect(qMax(0, m_current)).top(); }

private:
    struct Item
    {
        QString icon;
        QString text;
    };
    QList<Item> m_items;
    int m_current = 0;
    int m_hovered = -1;
    qreal m_pillY = 16;
    bool m_keyboard = false;
};

// ---- SettingsDialog ----------------------------------------------------------------

SettingsDialog::SettingsDialog(AppSettings *settings, TranslationService *translation, SpeechService *speech,
                               QWidget *parent)
    : QDialog(parent)
    , m_settings(settings)
    , m_translation(translation)
    , m_speech(speech)
{
    setObjectName(QStringLiteral("settingsDialog"));
    setWindowTitle(tr("Settings"));
    setMinimumSize(720, 540);
    resize(840, 680);

    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    m_nav = new SettingsNav(this);
    m_nav->setObjectName(QStringLiteral("settingsNav"));
    m_nav->addItem(QStringLiteral("key"), tr("AI"));
    m_nav->addItem(QStringLiteral("globe"), tr("Translation"));
    m_nav->addItem(QStringLiteral("speaker"), tr("Speech"));
    m_nav->addItem(QStringLiteral("palette"), tr("Appearance"));
    outer->addWidget(m_nav);

    auto *right = new QVBoxLayout;
    right->setContentsMargins(28, 22, 24, 18);
    right->setSpacing(10);
    m_pageTitle = new QLabel(this);
    m_pageTitle->setFont(Theme::uiFont(16, QFont::DemiBold));
    right->addWidget(m_pageTitle);

    m_pages = new QStackedWidget(this);
    m_pages->addWidget(buildAiPage());
    m_pages->addWidget(buildTranslationPage());
    m_pages->addWidget(buildSpeechPage());
    m_pages->addWidget(buildAppearancePage());
    right->addWidget(m_pages, 1);

    auto *footer = new QHBoxLayout;
    footer->setSpacing(8);
    footer->addStretch(1);
    auto *cancel = new ui::Button(tr("Cancel"), ui::Button::Variant::Secondary, this);
    cancel->setObjectName(QStringLiteral("cancelButton"));
    connect(cancel, &QAbstractButton::clicked, this, &SettingsDialog::reject);
    footer->addWidget(cancel);
    m_apply = new ui::Button(tr("Apply"), ui::Button::Variant::Secondary, this);
    m_apply->setObjectName(QStringLiteral("applyButton"));
    connect(m_apply, &QAbstractButton::clicked, this, &SettingsDialog::apply);
    footer->addWidget(m_apply);
    m_ok = new ui::Button(tr("Save"), ui::Button::Variant::Primary, this);
    m_ok->setObjectName(QStringLiteral("okButton"));
    m_ok->setToolTip(tr("Save and close"));
    connect(m_ok, &QAbstractButton::clicked, this, &SettingsDialog::accept);
    footer->addWidget(m_ok);
    right->addLayout(footer);
    outer->addLayout(right, 1);

    m_nav->onChanged = [this](int index) {
        motion::crossFade(m_pages, motion::kNormal);
        m_pages->setCurrentIndex(index);
        m_pageTitle->setText(m_pages->currentWidget()->property("pageTitle").toString());
        motion::fadeIn(m_pageTitle, motion::kNormal);
    };

    if (m_translation) {
        connect(m_translation, &TranslationService::modelsListed, this,
                [this](const QString &providerId, const QStringList &models) {
                    auto it = m_providerWidgets.find(providerId);
                    if (it == m_providerWidgets.end())
                        return;
                    it->test->setBusy(false);
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
                    it->test->setBusy(false);
                    const QString msg = error.message.isEmpty() ? tr("Connection failed") : error.message;
                    setStatus(it->status, tr("✗ %1").arg(msg), QStringLiteral("error"), error.detail);
                });
    }
    if (m_speech) {
        connect(m_speech, &SpeechService::azureTestFinished, this, [this](bool ok, const QString &message) {
            m_azureTest->setBusy(false);
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
    setCurrentTab(Tab::AI);
}

// ---- Pages -----------------------------------------------------------------------------

QWidget *SettingsDialog::buildAiPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 8, 8);
    v->setSpacing(8);

    m_providerIds = m_translation ? m_translation->providerIds() : QStringList();
    if (m_providerIds.isEmpty())
        m_providerIds = {kClaude, kOpenAi};

    auto *providerGroup = new SettingsGroup(page);
    m_provider = new ui::SegmentedControl(providerGroup);
    m_provider->setObjectName(QStringLiteral("providerControl"));
    for (const QString &id : std::as_const(m_providerIds))
        m_provider->addSegment(id == kOpenAi ? QStringLiteral("OpenAI") : id == kClaude ? QStringLiteral("Claude") : id,
                               m_translation ? m_translation->providerDisplayName(id) : QString());
    providerGroup->addRow(makeRow(tr("AI provider"), tr("The service that writes your translations."), m_provider));
    v->addWidget(providerGroup);

    m_providerStack = new QStackedWidget(page);
    for (const QString &id : std::as_const(m_providerIds))
        m_providerStack->addWidget(buildProviderGroup(id));
    v->addWidget(m_providerStack);
    connect(m_provider, &ui::SegmentedControl::currentIndexChanged, this, [this](int index) {
        motion::crossFade(m_providerStack, motion::kNormal);
        m_providerStack->setCurrentIndex(index);
        markDirty();
    });

    v->addWidget(groupCaption(tr("QUALITY"), page));
    auto *qualityGroup = new SettingsGroup(page);
    m_quality = new ui::SegmentedControl(qualityGroup);
    m_quality->setObjectName(QStringLiteral("qualityControl"));
    m_quality->addSegment(tr("Fast"), tr("Quickest answers - great for everyday phrases"));
    m_quality->addSegment(tr("Balanced"), tr("Good balance of speed and nuance (recommended)"));
    m_quality->addSegment(tr("Best"), tr("Most natural, nuanced Cantonese - slower and costs a bit more"));
    qualityGroup->addRow(makeRow(tr("Translation quality"), QString(""), m_quality, false, &m_qualityNote));
    v->addWidget(qualityGroup);
    connect(m_quality, &ui::SegmentedControl::currentIndexChanged, this, [this] {
        updateQualityNote();
        markDirty();
    });

    v->addSpacing(6);
    v->addWidget(caption(tr("Your text is sent to the selected provider to be translated. API keys are stored "
                            "encrypted on this computer (Windows DPAPI) and only sent to that provider."),
                         page));
    v->addStretch(1);
    QWidget *w = scrollPage(page, this);
    w->setProperty("pageTitle", tr("AI"));
    return w;
}

QWidget *SettingsDialog::buildProviderGroup(const QString &providerId)
{
    auto *group = new SettingsGroup(nullptr);
    ProviderWidgets w;

    auto *keyBox = new QWidget(group);
    auto *keyLayout = new QVBoxLayout(keyBox);
    keyLayout->setContentsMargins(0, 0, 0, 0);
    keyLayout->setSpacing(8);
    auto *keyRow = new QHBoxLayout;
    keyRow->setSpacing(8);
    w.key = new ui::PasswordLineEdit(keyBox);
    w.key->setObjectName(QStringLiteral("%1Key").arg(providerId));
    w.key->setPlaceholderText(providerId == kOpenAi ? QStringLiteral("sk-…") : QStringLiteral("sk-ant-…"));
    keyRow->addWidget(w.key, 1);
    w.test = new ui::Button(tr("Test connection"), ui::Button::Variant::Secondary, keyBox);
    w.test->setObjectName(QStringLiteral("%1Test").arg(providerId));
    w.test->setToolTip(tr("Checks the key and loads the models available to it. Costs nothing."));
    w.test->setBusy(false, tr("Testing…"));
    keyRow->addWidget(w.test);
    keyLayout->addLayout(keyRow);
    w.status = caption(QString(), keyBox);
    w.status->setObjectName(QStringLiteral("%1Status").arg(providerId));
    w.status->hide();
    keyLayout->addWidget(w.status);
    keyLayout->addWidget(linkLabel(providerId == kOpenAi ? tr("Get an OpenAI API key") : tr("Get a Claude API key"),
                                   providerId == kOpenAi ? QStringLiteral("https://platform.openai.com/api-keys")
                                                         : QStringLiteral("https://console.anthropic.com/settings/keys"),
                                   keyBox));
    const QString name = m_translation ? m_translation->providerDisplayName(providerId) : QString();
    group->addRow(makeRow(tr("API key"),
                          tr("Paste your %1 key. It's stored encrypted and only sent to %1.")
                              .arg(name.isEmpty() ? providerId : name),
                          keyBox, true));

    w.model = makeCombo(group, true);
    w.model->setObjectName(QStringLiteral("%1Model").arg(providerId));
    const QString def = m_translation ? m_translation->defaultModel(providerId) : QString();
    group->addRow(makeRow(tr("Model"),
                          def.isEmpty() ? tr("Test connection lists the models your key can use.")
                                        : tr("Recommended: %1").arg(def),
                          w.model));

    connect(w.test, &QAbstractButton::clicked, this, [this, providerId] { testProvider(providerId); });
    watch(w.key);
    watch(w.model);
    m_providerWidgets.insert(providerId, w);
    return group;
}

QWidget *SettingsDialog::buildTranslationPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 8, 8);
    v->setSpacing(8);

    auto *scriptGroup = new SettingsGroup(page);
    m_script = new ui::SegmentedControl(scriptGroup);
    m_script->setObjectName(QStringLiteral("scriptControl"));
    m_script->setSegmentFont(Theme::textFont(Language::Cantonese, 10));
    m_script->addSegment(QStringLiteral("繁體  ") + tr("Traditional"), tr("As used in Hong Kong"));
    m_script->addSegment(QStringLiteral("简体  ") + tr("Simplified"));
    connect(m_script, &ui::SegmentedControl::currentIndexChanged, this, &SettingsDialog::markDirty);
    scriptGroup->addRow(makeRow(tr("Chinese characters"), tr("Hong Kong Cantonese is usually written in Traditional."),
                                m_script));
    v->addWidget(scriptGroup);

    v->addWidget(groupCaption(tr("SHOW WITH EACH TRANSLATION"), page));
    auto *showGroup = new SettingsGroup(page);
    m_showJyutping = new ui::ToggleSwitch(showGroup);
    m_showJyutping->setObjectName(QStringLiteral("showJyutping"));
    m_showAlternatives = new ui::ToggleSwitch(showGroup);
    m_showAlternatives->setObjectName(QStringLiteral("showAlternatives"));
    m_showNotes = new ui::ToggleSwitch(showGroup);
    m_showNotes->setObjectName(QStringLiteral("showNotes"));
    showGroup->addRow(makeRow(tr("Jyutping pronunciation"), tr("Romanisation with tone numbers, e.g. nei5 hou2."),
                              m_showJyutping));
    showGroup->addRow(makeRow(tr("Other ways to say it"), tr("Alternative phrasings and when to use them."),
                              m_showAlternatives));
    showGroup->addRow(makeRow(tr("Usage notes"), tr("Slang, sentence particles and cultural context."), m_showNotes));
    for (auto *t : {m_showJyutping, m_showAlternatives, m_showNotes})
        watch(t);
    v->addWidget(showGroup);
    v->addSpacing(6);
    v->addWidget(caption(tr("Turning off alternatives and notes makes translations a little faster and cheaper."), page));
    v->addStretch(1);
    QWidget *w = scrollPage(page, this);
    w->setProperty("pageTitle", tr("Translation"));
    return w;
}

QWidget *SettingsDialog::buildSpeechPage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 8, 8);
    v->setSpacing(8);

    m_engineIds = m_speech ? m_speech->engineIds() : QStringList();
    if (m_engineIds.isEmpty())
        m_engineIds = {kSystem, kAzure};

    auto *engineGroup = new SettingsGroup(page);
    m_engine = new ui::SegmentedControl(engineGroup);
    m_engine->setObjectName(QStringLiteral("engineControl"));
    for (const QString &id : std::as_const(m_engineIds)) {
        QString name = m_speech ? m_speech->engineDisplayName(id) : QString();
        if (name.isEmpty())
            name = id == kAzure ? tr("Azure neural voices") : tr("Windows voices");
        // Short label in the control; the full name goes in the tooltip.
        m_engine->addSegment(name.section(QStringLiteral(" ("), 0, 0).trimmed(), name);
    }
    m_engine->setEqualWidths(false);
    engineGroup->addRow(makeRow(tr("Voice engine"), QString(""), m_engine, true, &m_engineNote));
    connect(m_engine, &ui::SegmentedControl::currentIndexChanged, this, [this] {
        updateEngineUi();
        markDirty();
    });

    // Windows Cantonese voice status.
    auto *statusBox = new QWidget(engineGroup);
    auto *statusLayout = new QVBoxLayout(statusBox);
    statusLayout->setContentsMargins(16, 12, 16, 12);
    statusLayout->setSpacing(6);
    auto *statusRow = new QHBoxLayout;
    statusRow->setSpacing(8);
    m_windowsVoiceIcon = new ui::IconLabel(QStringLiteral("check-circle"), IconTone::Success, 18, statusBox);
    statusRow->addWidget(m_windowsVoiceIcon);
    m_windowsVoiceStatus = new QLabel(statusBox);
    m_windowsVoiceStatus->setObjectName(QStringLiteral("windowsVoiceStatus"));
    m_windowsVoiceStatus->setWordWrap(true);
    m_windowsVoiceStatus->setFont(Theme::uiFont(9.5, QFont::Medium));
    statusRow->addWidget(m_windowsVoiceStatus, 1);
    auto *recheck = new ui::Button(tr("Check again"), ui::Button::Variant::Ghost, statusBox);
    recheck->setToolTip(tr("Look for newly installed Windows voices"));
    connect(recheck, &QAbstractButton::clicked, this, [this] {
        if (m_speech)
            m_speech->refreshVoices();
        populateVoices();
        updateCantoneseVoiceStatus();
    });
    statusRow->addWidget(recheck);
    statusLayout->addLayout(statusRow);
    // The install steps are long: keep them behind a disclosure.
    m_windowsVoiceHelpSection = new ui::Disclosure(statusBox);
    m_windowsVoiceHelpSection->setTitle(tr("How to install a Cantonese voice"));
    m_windowsVoiceHelp = caption(QString(), m_windowsVoiceHelpSection->body());
    m_windowsVoiceHelp->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_windowsVoiceHelp->setContentsMargins(6, 0, 0, 0);
    m_windowsVoiceHelpSection->contentLayout()->addWidget(m_windowsVoiceHelp);
    m_windowsVoiceHelpSection->setContentsMargins(20, 0, 0, 0);
    statusLayout->addWidget(m_windowsVoiceHelpSection);
    engineGroup->addRow(statusBox);
    v->addWidget(engineGroup);

    v->addWidget(groupCaption(tr("VOICES"), page));
    auto *voiceGroup = new SettingsGroup(page);
    m_englishVoice = makeCombo(voiceGroup, false);
    m_englishVoice->setObjectName(QStringLiteral("englishVoice"));
    m_cantoneseVoice = makeCombo(voiceGroup, false);
    m_cantoneseVoice->setObjectName(QStringLiteral("cantoneseVoice"));
    voiceGroup->addRow(makeRow(tr("English voice"), QString(), m_englishVoice));
    voiceGroup->addRow(makeRow(tr("Cantonese voice"), QString(), m_cantoneseVoice));
    for (QComboBox *combo : {m_englishVoice, m_cantoneseVoice}) {
        watch(combo);
        connect(combo, &QComboBox::currentIndexChanged, this, [this, combo](int) {
            if (m_loading)
                return;
            const Language lang = combo == m_cantoneseVoice ? Language::Cantonese : Language::English;
            m_voiceSelection[selectedEngine()][langKey(lang)] = combo->currentData().toString();
        });
    }

    auto *rateBox = new QWidget(voiceGroup);
    auto *rateRow = new QHBoxLayout(rateBox);
    rateRow->setContentsMargins(0, 0, 0, 0);
    rateRow->setSpacing(10);
    rateRow->addWidget(caption(tr("Slower"), rateBox));
    m_rate = new QSlider(Qt::Horizontal, rateBox);
    m_rate->setObjectName(QStringLiteral("speechRate"));
    m_rate->setRange(-10, 10);
    m_rate->setPageStep(2);
    m_rate->setMinimumWidth(180);
    rateRow->addWidget(m_rate, 1);
    rateRow->addWidget(caption(tr("Faster"), rateBox));
    m_rateValue = new QLabel(rateBox);
    m_rateValue->setMinimumWidth(52);
    m_rateValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_rateValue->setFont(Theme::uiFont(9.5, QFont::Medium));
    rateRow->addWidget(m_rateValue);
    voiceGroup->addRow(makeRow(tr("Speed"), QString(), rateBox));
    connect(m_rate, &QSlider::valueChanged, this, [this] {
        updateRateLabel();
        markDirty();
    });
    m_autoSpeak = new ui::ToggleSwitch(voiceGroup);
    m_autoSpeak->setObjectName(QStringLiteral("autoSpeak"));
    watch(m_autoSpeak);
    voiceGroup->addRow(makeRow(tr("Read translations aloud"), tr("Speak each new translation automatically."),
                               m_autoSpeak));
    v->addWidget(voiceGroup);

    v->addWidget(groupCaption(tr("AZURE SPEECH"), page));
    auto *azureGroup = new SettingsGroup(page);
    m_azureKey = new ui::PasswordLineEdit(azureGroup);
    m_azureKey->setObjectName(QStringLiteral("azureKey"));
    m_azureKey->setPlaceholderText(tr("Key 1 or Key 2 from your Speech resource"));
    m_azureKey->setMinimumWidth(260);
    azureGroup->addRow(makeRow(tr("Key"), tr("Stored encrypted on this computer."), m_azureKey));
    m_azureRegion = makeCombo(azureGroup, true);
    m_azureRegion->setObjectName(QStringLiteral("azureRegion"));
    m_azureRegion->addItems({QStringLiteral("eastasia"), QStringLiteral("southeastasia"), QStringLiteral("japaneast"),
                             QStringLiteral("koreacentral"), QStringLiteral("australiaeast"), QStringLiteral("centralindia"),
                             QStringLiteral("eastus"), QStringLiteral("eastus2"), QStringLiteral("westus"),
                             QStringLiteral("westus2"), QStringLiteral("westus3"), QStringLiteral("centralus"),
                             QStringLiteral("canadacentral"), QStringLiteral("brazilsouth"), QStringLiteral("northeurope"),
                             QStringLiteral("westeurope"), QStringLiteral("uksouth"), QStringLiteral("francecentral"),
                             QStringLiteral("germanywestcentral"), QStringLiteral("swedencentral"),
                             QStringLiteral("switzerlandnorth")});
    azureGroup->addRow(makeRow(tr("Region"), tr("eastasia is Hong Kong. Use the region shown on your resource."),
                               m_azureRegion));
    auto *testBox = new QWidget(azureGroup);
    auto *testRow = new QHBoxLayout(testBox);
    testRow->setContentsMargins(0, 0, 0, 0);
    testRow->setSpacing(10);
    m_azureTest = new ui::Button(tr("Test voice"), ui::Button::Variant::Secondary, testBox);
    m_azureTest->setObjectName(QStringLiteral("azureTest"));
    m_azureTest->setBusy(false, tr("Playing…"));
    testRow->addWidget(m_azureTest);
    m_azureStatus = caption(QString(), testBox);
    m_azureStatus->setObjectName(QStringLiteral("azureStatus"));
    testRow->addWidget(m_azureStatus, 1);
    testRow->addWidget(linkLabel(tr("Create a free Azure Speech resource"),
                                 QStringLiteral("https://portal.azure.com/#create/Microsoft.CognitiveServicesSpeechServices"),
                                 testBox));
    azureGroup->addRow(makeRow(QString(), QString(), testBox, true));
    v->addWidget(azureGroup);
    watch(m_azureKey);
    watch(m_azureRegion);
    connect(m_azureTest, &QAbstractButton::clicked, this, &SettingsDialog::testAzure);

    v->addStretch(1);
    QWidget *w = scrollPage(page, this);
    w->setProperty("pageTitle", tr("Speech"));
    return w;
}

QWidget *SettingsDialog::buildAppearancePage()
{
    auto *page = new QWidget;
    auto *v = new QVBoxLayout(page);
    v->setContentsMargins(0, 0, 8, 8);
    v->setSpacing(8);

    auto *group = new SettingsGroup(page);
    m_theme = new ui::SegmentedControl(group);
    m_theme->setObjectName(QStringLiteral("themeControl"));
    m_theme->addSegment(tr("System"), tr("Follow the Windows light/dark setting"));
    m_theme->addSegment(tr("Light"));
    m_theme->addSegment(tr("Dark"));
    connect(m_theme, &ui::SegmentedControl::currentIndexChanged, this, &SettingsDialog::markDirty);
    group->addRow(makeRow(tr("Theme"), QString(), m_theme));

    auto *sizeBox = new QWidget(group);
    auto *sizeRow = new QHBoxLayout(sizeBox);
    sizeRow->setContentsMargins(0, 0, 0, 0);
    sizeRow->setSpacing(10);
    auto *small = new QLabel(QStringLiteral("A"), sizeBox);
    small->setFont(Theme::uiFont(9));
    sizeRow->addWidget(small);
    m_fontSize = new QSlider(Qt::Horizontal, sizeBox);
    m_fontSize->setObjectName(QStringLiteral("fontSize"));
    m_fontSize->setRange(10, 24);
    m_fontSize->setPageStep(2);
    m_fontSize->setMinimumWidth(180);
    sizeRow->addWidget(m_fontSize, 1);
    auto *big = new QLabel(QStringLiteral("A"), sizeBox);
    big->setFont(Theme::uiFont(15));
    sizeRow->addWidget(big);
    m_fontSizeValue = new QLabel(sizeBox);
    m_fontSizeValue->setMinimumWidth(44);
    m_fontSizeValue->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_fontSizeValue->setFont(Theme::uiFont(9.5, QFont::Medium));
    sizeRow->addWidget(m_fontSizeValue);
    group->addRow(makeRow(tr("Text size"), tr("For the text you type and the translations."), sizeBox));

    m_fontPreview = new QLabel(group);
    m_fontPreview->setObjectName(QStringLiteral("fontPreview"));
    m_fontPreview->setWordWrap(true);
    m_fontPreview->setText(QStringLiteral("好耐冇見！你最近點呀？\nLong time no see! How have you been?"));
    m_fontPreview->setContentsMargins(16, 12, 16, 14);
    group->addRow(m_fontPreview);
    connect(m_fontSize, &QSlider::valueChanged, this, [this] {
        updateFontPreview();
        markDirty();
    });

    m_reduceMotion = new ui::ToggleSwitch(group);
    m_reduceMotion->setObjectName(QStringLiteral("reduceMotion"));
    watch(m_reduceMotion);
    group->addRow(makeRow(tr("Reduce motion"), tr("Turn off animations and transitions."), m_reduceMotion));
    v->addWidget(group);
    v->addStretch(1);
    QWidget *w = scrollPage(page, this);
    w->setProperty("pageTitle", tr("Appearance"));
    return w;
}

// ---- Load / apply ------------------------------------------------------------------------

void SettingsDialog::load()
{
    m_loading = true;
    if (m_settings) {
        const int providerIndex = qMax(0, int(m_providerIds.indexOf(m_settings->aiProvider())));
        m_provider->setCurrentIndex(providerIndex, false);
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
        m_quality->setCurrentIndex(quality, false);

        m_script->setCurrentIndex(m_settings->script() == ChineseScript::Simplified ? 1 : 0, false);
        m_showJyutping->setChecked(m_settings->showJyutping());
        m_showAlternatives->setChecked(m_settings->showAlternatives());
        m_showNotes->setChecked(m_settings->showNotes());

        for (const QString &engine : std::as_const(m_engineIds)) {
            for (Language lang : {Language::English, Language::Cantonese}) {
                m_voiceSelection[engine][langKey(lang)] =
                    engine == kAzure ? m_settings->azureVoice(lang) : m_settings->systemVoice(lang);
            }
        }
        m_engine->setCurrentIndex(qMax(0, int(m_engineIds.indexOf(m_settings->speechEngine()))), false);
        m_rate->setValue(qRound(m_settings->speechRate() * 10));
        m_autoSpeak->setChecked(m_settings->autoSpeak());
        m_azureKey->setText(m_settings->azureKey());
        m_azureRegion->setCurrentText(m_settings->azureRegion());

        m_theme->setCurrentIndex(qMax(0, int(kThemeIds.indexOf(m_settings->theme()))), false);
        m_fontSize->setValue(m_settings->fontPointSize());
    }
    m_reduceMotion->setChecked(UiPrefs::instance()->reduceMotion());
    updateQualityNote();
    updateRateLabel();
    updateFontPreview();
    m_loading = false;
    updateEngineUi();
    m_dirty = false;
    m_apply->setEnabled(false);
}

void SettingsDialog::apply()
{
    if (m_settings) {
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

        m_settings->setScript(m_script->currentIndex() == 1 ? ChineseScript::Simplified : ChineseScript::Traditional);
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
    }
    UiPrefs::instance()->setReduceMotion(m_reduceMotion->isChecked());
    m_dirty = false;
    m_apply->setEnabled(false);
}

void SettingsDialog::accept()
{
    if (m_dirty)
        apply();
    QDialog::accept();
}

void SettingsDialog::keyPressEvent(QKeyEvent *event)
{
    // Enter saves (the custom buttons are not QPushButton defaults).
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && event->modifiers() == Qt::NoModifier
        && !qobject_cast<QPlainTextEdit *>(focusWidget())) {
        accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

void SettingsDialog::setCurrentTab(Tab tab)
{
    const int index = int(tab);
    m_nav->setCurrentIndex(index, false);
    m_pages->setCurrentIndex(index);
    m_pageTitle->setText(m_pages->currentWidget()->property("pageTitle").toString());
}

SettingsDialog::Tab SettingsDialog::currentTab() const { return static_cast<Tab>(m_pages->currentIndex()); }

// ---- Helpers ---------------------------------------------------------------------------------

void SettingsDialog::markDirty()
{
    if (m_loading)
        return;
    m_dirty = true;
    m_apply->setEnabled(true);
}

void SettingsDialog::watch(QObject *w)
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
    const bool wasHidden = label->isHidden();
    label->setVisible(!text.isEmpty());
    if (wasHidden && !text.isEmpty())
        motion::fadeIn(label, motion::kNormal);
}

QString SettingsDialog::selectedProvider() const { return m_providerIds.value(qMax(0, m_provider->currentIndex()), kClaude); }

QString SettingsDialog::selectedEngine() const { return m_engineIds.value(qMax(0, m_engine->currentIndex()), kSystem); }

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
            combo->addItem(tr("%1 (not available)").arg(selected), selected);
            index = combo->count() - 1;
        }
        combo->setCurrentIndex(index);
    }
    m_loading = wasLoading;
}

void SettingsDialog::updateEngineUi()
{
    populateVoices();
    const bool azure = selectedEngine() == kAzure;
    if (m_engineNote) {
        m_engineNote->setText(azure ? tr("Very natural neural voices such as HiuMaan. Needs an Azure Speech key - the "
                                         "free tier covers about 500,000 characters a month.")
                                    : tr("Free and works offline. Cantonese needs the Windows \"Chinese (Traditional, "
                                         "Hong Kong SAR)\" speech pack."));
    }
    updateCantoneseVoiceStatus();
}

void SettingsDialog::updateCantoneseVoiceStatus()
{
    const QList<VoiceInfo> voices = m_speech ? m_speech->voices(kSystem, Language::Cantonese) : QList<VoiceInfo>();
    if (!voices.isEmpty()) {
        const QString name = voices.first().name.isEmpty() ? voices.first().id : voices.first().name;
        m_windowsVoiceIcon->setIcon(QStringLiteral("check-circle"), IconTone::Success);
        m_windowsVoiceStatus->setText(tr("Windows Cantonese voice installed: %1").arg(name));
        m_windowsVoiceHelpSection->hide();
    } else {
        m_windowsVoiceIcon->setIcon(QStringLiteral("warning"), IconTone::Warning);
        m_windowsVoiceStatus->setText(tr("No Cantonese voice is installed in Windows."));
        m_windowsVoiceHelp->setText(SpeechService::cantoneseVoiceHelpText());
        m_windowsVoiceHelpSection->setVisible(!m_windowsVoiceHelp->text().isEmpty());
        m_windowsVoiceHelpSection->contentChanged();
    }
}

void SettingsDialog::updateQualityNote()
{
    if (!m_qualityNote)
        return;
    switch (m_quality->currentIndex()) {
    case 0:
        m_qualityNote->setText(tr("Quickest answers. Great for everyday phrases."));
        break;
    case 2:
        m_qualityNote->setText(tr("The most natural, nuanced Cantonese - especially for slang and idioms. "
                                  "Slower, and costs a bit more per translation."));
        break;
    default:
        m_qualityNote->setText(tr("A good mix of speed and nuance. Recommended."));
        break;
    }
}

void SettingsDialog::updateRateLabel()
{
    const int v = m_rate->value();
    m_rateValue->setText(v == 0 ? tr("Normal")
                                : QStringLiteral("%1%2%").arg(v > 0 ? QStringLiteral("+") : QStringLiteral("−")).arg(qAbs(v) * 10));
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
    w.test->setBusy(true, tr("Testing…"));
    setStatus(w.status, QString(), QStringLiteral("muted"));
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
    m_azureTest->setBusy(true, tr("Playing…"));
    setStatus(m_azureStatus, QString(), QStringLiteral("muted"));
    m_speech->testAzure(key, region, m_voiceSelection[kAzure].value(langKey(Language::Cantonese)));
}

} // namespace sct
