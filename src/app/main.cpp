// Smart Cantonese Translator - application entry point.

#include "core/AppSettings.h"
#include "core/TranslationService.h"
#include "core/TranslationTypes.h"
#include "core/Version.h"
#include "tts/SpeechService.h"
#include "ui/Icons.h"
#include "ui/MainWindow.h"
#include "ui/Motion.h"
#include "ui/ResultView.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QStyleFactory>
#include <QTimer>

#include <memory>

namespace {

// Sample content for --demo (screenshots / visual checks only).
sct::TranslationResult demoResult()
{
    sct::TranslationResult r;
    r.request.text = QStringLiteral("Long time no see! How have you been?");
    r.request.direction = sct::Direction::EnglishToCantonese;
    r.request.tone = sct::Tone::Neutral;
    r.translation = QStringLiteral("好耐冇見！你最近點呀？");
    r.jyutping = QStringLiteral("hou2 noi6 mou5 gin3! nei5 zeoi3 gan6 dim2 aa3?");
    r.alternatives = {
        {QStringLiteral("咦，好耐冇見喎！近排點呀？"), QStringLiteral("ji2, hou2 noi6 mou5 gin3 wo3! gan6 paai2 dim2 aa3?"),
         QStringLiteral("Casual and warm - 近排 (lately) is very colloquial.")},
        {QStringLiteral("好耐冇見，你最近好嗎？"), QStringLiteral("hou2 noi6 mou5 gin3, nei5 zeoi3 gan6 hou2 maa3?"),
         QStringLiteral("A little more polite; fine with older relatives.")},
    };
    r.notes = {QStringLiteral("點呀 (dim2 aa3) is the everyday way to ask \"how are you\"; 你好嗎 sounds textbook-formal."),
               QStringLiteral("The particle 呀 softens the question and makes it sound friendly.")};
    r.providerId = QStringLiteral("claude");
    r.model = QStringLiteral("claude-opus-5-5");
    r.request.script = sct::ChineseScript::Traditional;
    r.timestamp = QDateTime::currentDateTimeUtc();
    return r;
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral(SCT_ORG_NAME));
    QCoreApplication::setApplicationName(QStringLiteral(SCT_APP_ID));
    QCoreApplication::setApplicationVersion(QStringLiteral(SCT_VERSION_STRING));
    QGuiApplication::setApplicationDisplayName(QStringLiteral(SCT_APP_NAME));
    sct::registerMetaTypes();

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral(SCT_APP_NAME));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption settingsOption(QStringLiteral("settings"),
                                            QStringLiteral("Store settings in this INI file (portable mode)."),
                                            QStringLiteral("file"));
    const QCommandLineOption themeOption(QStringLiteral("theme"),
                                         QStringLiteral("Theme for this run only: system, light or dark."),
                                         QStringLiteral("theme"));
    const QCommandLineOption screenshotOption(QStringLiteral("screenshot"),
                                              QStringLiteral("Save a picture of the main window to <file> and exit."),
                                              QStringLiteral("file"));
    const QCommandLineOption demoOption(QStringLiteral("demo"), QStringLiteral("Show a sample translation."));
    parser.addOption(settingsOption);
    parser.addOption(themeOption);
    parser.addOption(screenshotOption);
    parser.addOption(demoOption);
    parser.process(app);

    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QApplication::setFont(sct::Theme::uiFont(10));
    QApplication::setWindowIcon(sct::ui::appIcon());

    std::unique_ptr<sct::AppSettings> settings;
    if (parser.isSet(settingsOption)) {
        settings = std::make_unique<sct::AppSettings>(parser.value(settingsOption));
        sct::UiPrefs::useIniFile(parser.value(settingsOption));
    } else {
        settings = std::make_unique<sct::AppSettings>();
    }
    sct::Theme::apply(parser.isSet(themeOption) ? parser.value(themeOption) : settings->theme());

    sct::TranslationService translation(settings.get());
    sct::SpeechService speech(settings.get(), translation.networkManager());
    sct::MainWindow window(settings.get(), &translation, &speech);
    if (parser.isSet(themeOption))
        sct::Theme::apply(parser.value(themeOption));

    if (parser.isSet(demoOption)) {
        const sct::TranslationResult demo = demoResult();
        window.setInputText(demo.request.text);
        window.showResult(demo);
        window.resultView()->alternativesSection()->setExpanded(true, false);
        window.resultView()->notesSection()->setExpanded(true, false);
    }

    window.show();

    if (parser.isSet(screenshotOption)) {
        const QString path = parser.value(screenshotOption);
        QTimer::singleShot(1500, &window, [&window, path] {
            const bool ok = window.grab().save(path);
            QCoreApplication::exit(ok ? 0 : 1);
        });
    }
    return app.exec();
}
