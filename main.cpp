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
              << CAPTURER_VERSION_MAJOR << "." << CAPTURER_VERSION_MINOR << "." << CAPTURER_VERSION_PATCH
              << " (Qt " << qVersion() << ")";

    LOG(INFO) << "Operating System: "
#ifdef Q_OS_LINUX
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
    translator.load("languages/capturer_" + language);
    qApp->installTranslator(&translator);

    Capturer window;

    return a.exec();
}
