#include "capturer.h"
#include "logging.h"
#include "probe/cpu.h"
#include "probe/system.h"
#include "probe/util.h"
#include "version.h"

#include <QApplication>
#include <QTranslator>

int main(int argc, char *argv[])
{
    Logger::init(argv[0]);

    probe::thread::set_name("capturer-main");

    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);

    LOG(INFO) << "Capturer               " << CAPTURER_VERSION;
    LOG(INFO) << " -- Qt               : " << qVersion();
    LOG(INFO) << " -- Operating System : " << probe::system::os_name() << " ("
              << probe::to_string(probe::system::os_version()) << ")";
    LOG(INFO) << " -- Kernel           : " << probe::system::kernel_name() << " "
              << probe::to_string(probe::system::kernel_version());
    LOG(INFO) << " -- Architecture     : " << probe::to_string(probe::cpu::architecture());
    LOG(INFO) << " -- Virtual Screen   : " << probe::to_string(probe::graphics::virtual_screen_geometry());
    LOG(INFO) << " -- Desktop ENV      : " << probe::to_string(probe::system::desktop());

    Config::instance().load_theme(Config::theme());

    auto language = Config::instance()["language"].get<QString>();
    LOG(INFO) << " -- Language         : " << language.toStdString();
    LOG(INFO) << " -- Config File      : " << Config::instance().getFilePath().toStdString();
    LOG(INFO);

    QTranslator translator;
#ifdef __linux__
    translator.load("/usr/local/etc/capturer/translations/capturer_" + language);
#else
    translator.load("translations/capturer_" + language);
#endif
    qApp->installTranslator(&translator);

    Capturer window;

    return app.exec();
}
