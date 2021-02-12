#include <QApplication>
#include <QOperatingSystemVersion>
#include <QFile>
#include <QTranslator>
#include "version.h"
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

    LOG(INFO) << QCoreApplication::applicationName() << " "
              << CAPTURER_VERSION
              << " (Qt " << qVersion() << ")";

    LOG(INFO) << "Operating System: "
#ifdef __linux__
              << "Linux" <<  " ("
#else
              <<  QOperatingSystemVersion::current().name() << " "
              <<  QOperatingSystemVersion::current().majorVersion() << "."
              <<  QOperatingSystemVersion::current().minorVersion() << "."
              <<  QOperatingSystemVersion::current().microVersion() << " ("
#endif
              << QSysInfo::currentCpuArchitecture() << ")";


    LOG(INFO) << "Application Dir: " << QCoreApplication::applicationDirPath();

    // displays
    DisplayInfo::instance();

    LOAD_QSS(qApp, ":/qss/menu/menu.qss");

    auto language = Config::instance()["language"].get<QString>();
    LOG(INFO) << "LANGUAGE: " << language;

    QTranslator translator;
#ifdef __linux__
    translator.load("/etc/capturer/translations/capturer_" + language);
#else
    translator.load("translations/capturer_" + language);
#endif
    qApp->installTranslator(&translator);

    Capturer window;

    return a.exec();
}
