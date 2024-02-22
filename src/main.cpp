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
    QApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

#ifdef _WIN32
    ::SetConsoleOutputCP(CP_UTF8);
    ::setvbuf(stdout, nullptr, _IONBF, 0);
#endif

    Logger::init(argv[0]);

    probe::thread::set_name("capturer-main");

    QApplication app(argc, argv);
    QApplication::setQuitOnLastWindowClosed(false);

    LOG(INFO) << "Capturer               " << CAPTURER_VERSION;
    LOG(INFO) << " -- Qt               : " << qVersion();
    LOG(INFO) << " -- Operating System : " << probe::system::name() << " ("
              << probe::to_string(probe::system::version()) << ")";
    LOG(INFO) << " -- Kernel           : " << probe::system::kernel::name() << " "
              << probe::to_string(probe::system::kernel::version());
    LOG(INFO) << " -- CPU              : " << probe::cpu::info().name;
    LOG(INFO) << " -- Architecture     : " << probe::to_string(probe::cpu::architecture());
    LOG(INFO) << " -- Virtual Screen   : " << probe::to_string(probe::graphics::virtual_screen_geometry());
    for (const auto& display : probe::graphics::displays()) {
        LOG(INFO) << fmt::format(" --   {:>14} : ", display.id) << probe::to_string(display.geometry);
    }
    LOG(INFO) << " -- Windowing System : " << probe::to_string(probe::system::windowing_system());
    LOG(INFO) << " -- Desktop ENV      : " << probe::to_string(probe::system::desktop_environment()) << " ("
              << probe::to_string(probe::system::desktop_environment_version()) << ")";

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
