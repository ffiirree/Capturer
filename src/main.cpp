#include <QApplication>
#include <QTranslator>
#include "version.h"
#include "utils.h"
#include "capturer.h"
#include "logging.h"
#include "platform.h"

int main(int argc, char *argv[])
{
    Logger::init(argv);

    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);

    LOG(INFO) << "Capturer " << CAPTURER_VERSION;

    LOG(INFO) << " -- Qt               : " << qVersion();
    LOG(INFO) << " -- Operating System : " << platform::system::os_name() << " " << platform::system::os_version();
    LOG(INFO) << " -- Kernel           : " << platform::system::kernel_name() << " " << platform::system::kernel_version();
    LOG(INFO) << " -- Architecture     : " << platform::cpu::architecture();
    LOG(INFO) << " -- Virtual Screen   : " << platform::display::virtual_screen_geometry();

    Config::load_theme(Config::theme());

    auto language = Config::instance()["language"].get<QString>();
    LOG(INFO) << " -- Language         : " << language;
    LOG(INFO) << " -- Config File      : " << Config::instance().getFilePath();
    LOG(INFO);

    QTranslator translator;
#ifdef __linux__
    translator.load("/etc/capturer/translations/capturer_" + language);
#else
    translator.load("translations/capturer_" + language);
#endif
    qApp->installTranslator(&translator);

    Capturer window;

    return app.exec();
}
