#include <QApplication>
#include <QOperatingSystemVersion>
#include <QFile>
#include <QTranslator>
#include "version.h"
#include "utils.h"
#include "capturer.h"
#include "logging.h"
#include "platform.h"

int main(int argc, char *argv[])
{
    Logger::init(argv);

    QApplication a(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);

    LOG(INFO) << "Capturer " << CAPTURER_VERSION << " (Qt " << qVersion() << ")";

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
    LOG(INFO) << "VIRTUAL SCREEN: " << platform::display::virtual_screen_geometry();

    LOAD_QSS(qApp,
        {
            ":/qss/capturer.qss",
            ":/qss/capturer-" + Config::instance()["theme"].get<QString>() + ".qss",
            ":/qss/menu/menu.qss",
            ":/qss/menu/menu-" + Config::instance()["theme"].get<QString>() + ".qss",
            ":/qss/setting/settingswindow.qss",
            ":/qss/setting/settingswindow-" + Config::instance()["theme"].get<QString>() + ".qss"
        }
    );

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
