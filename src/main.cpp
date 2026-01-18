#include "capturer.h"
#include "config.h"
#include "logging.h"
#include "probe/cpu.h"
#include "probe/system.h"
#include "version.h"

#include <QStandardPaths>

int main(int argc, char *argv[])
{
    qputenv("QT_ENABLE_HIGHDPI_SCALING", "0");

    QApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

#ifdef _WIN32
    ::SetConsoleOutputCP(CP_UTF8);
    ::setvbuf(stdout, nullptr, _IONBF, 0);
#endif

    Logger::init(argv[0],
                 QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation).toStdString() +
                     "/Capturer/Logs");

    probe::thread::set_name("capturer-main");

    config::load();

    logi("Capturer               {}", CAPTURER_VERSION);
    logi(" -- Qt               : {}", qVersion());
    logi(" -- Operating System : {} ({})", probe::system::name(),
         probe::to_string(probe::system::version()));
    logi(" -- Kernel           : {} {}", probe::system::kernel::name(),
         probe::to_string(probe::system::kernel::version()));
    logi(" -- CPU              : {}", probe::cpu::info().name);
    logi(" -- Architecture     : {}", probe::to_string(probe::cpu::architecture()));
    logi(" -- Virtual Screen   : {}", probe::to_string(probe::graphics::virtual_screen_geometry()));
    for (const auto& display : probe::graphics::displays()) {
        logi(" --   {:>14} : {}", display.id, probe::to_string(display.geometry));
    }
    logi(" -- Windowing System : {}", probe::to_string(probe::system::windowing_system()));
    logi(" -- Desktop ENV      : {} ({})", probe::to_string(probe::system::desktop_environment()),
         probe::to_string(probe::system::desktop_environment_version()));
    logi(" -- Language         : {}", config::language.toStdString());
    logi(" -- Config File      : {}", config::filepath.toStdString());
    logi("");

    Capturer app(argc, argv);
    app.Init();

    return Capturer::exec();
}
