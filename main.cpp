#include <QApplication>
#include <QFile>
#include <QTranslator>
#include "utils.h"
#include "displayinfo.h"
#include "capturer.h"
#include "logging.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);

    // log pattern
    qSetMessagePattern("%{time yyyy-MM-dd hh:mm:ss.zzz} (%{type}) %{file}:%{line}] %{message}");

    // displays
    DisplayInfo::instance();

    LOAD_QSS(qApp, ":/qss/menu/menu.qss");

    auto language = Config::instance()["language"].get<QString>();
    LOG(INFO) << "LANGUAGE: " << language;

    QTranslator translator;
    translator.load("languages/capturer_" + language);
    qApp->installTranslator(&translator);

    Capturer window;

    return a.exec();
}
