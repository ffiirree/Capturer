#include <QApplication>
#include <QFile>
#include <QTranslator>
#include "utils.h"
#include "displayinfo.h"
#include "capturer.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);

    // log pattern
    qSetMessagePattern("%{time yyyy-MM-dd hh:mm:ss.zzz} (%{type}) %{file}:%{line}] %{message}");

    LOG(INFO) << "Logging for Capturer";  LOG(INFO);

    // displays
    DisplayInfo::instance();

    LOAD_QSS(qApp, ":/qss/menu/menu.qss");


    auto settings = Config::Instance();
    auto language = settings->get<QString>(SETTINGS["language"]);

    LOG(INFO) << language;

    QTranslator translator;
    translator.load("languages/capturer_" + language);
    qApp->installTranslator(&translator);

    Capturer window;

    return a.exec();
}
