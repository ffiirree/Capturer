#include "config.h"

#include "devices.h"
#include "logging.h"
#include "probe/system.h"
#include "utils.h"

#include <fstream>
#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QTextStream>
#include <streambuf>

#define IF_NULL_SET(X, default_value) if (X.is_null()) X = default_value;

Config::Config()
{
    auto config_dir_path_ = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir config_dir(config_dir_path_);
    if (!config_dir.exists()) {
        config_dir.mkpath(config_dir_path_);
    }
    filepath_ = config_dir_path_ + "/config.json";

    QString text;
    QFile config_file(filepath_);
    if (config_file.open(QIODevice::ReadWrite | QIODevice::Text)) {
        QTextStream in(&config_file);
        text = in.readAll();
    }

    try {
        settings_ = json::parse(text.toStdString());
    }
    catch (json::parse_error&) {
        settings_ = json::parse("{}");
    }

    // default values
    // clang-format off
    IF_NULL_SET(settings_["autorun"],                               true);
    IF_NULL_SET(settings_["language"],                              "zh_CN");
    IF_NULL_SET(settings_["detectwindow"],                          true);
    IF_NULL_SET(settings_["theme"],                                 "auto");

    IF_NULL_SET(settings_["snip"]["selector"]["border"]["width"],   2);
    IF_NULL_SET(settings_["snip"]["selector"]["border"]["color"],   "#409EFF");
    IF_NULL_SET(settings_["snip"]["selector"]["border"]["style"],   Qt::DashDotLine);
    IF_NULL_SET(settings_["snip"]["selector"]["mask"]["color"],     "#88000000");

    IF_NULL_SET(settings_["record"]["selector"]["border"]["width"], 2);
    IF_NULL_SET(settings_["record"]["selector"]["border"]["color"], "#ffff5500");
    IF_NULL_SET(settings_["record"]["selector"]["border"]["style"], Qt::DashDotLine);
    IF_NULL_SET(settings_["record"]["selector"]["mask"]["color"],   "#88000000");
    IF_NULL_SET(settings_["record"]["encoder"],                     "libx264");
    IF_NULL_SET(settings_["record"]["quality"],                     "medium");
    IF_NULL_SET(settings_["record"]["box"],                         false);
    IF_NULL_SET(settings_["record"]["mouse"],                       true);

    IF_NULL_SET(settings_["gif"]["selector"]["border"]["width"],    2);
    IF_NULL_SET(settings_["gif"]["selector"]["border"]["color"],    "#ffff00ff");
    IF_NULL_SET(settings_["gif"]["selector"]["border"]["style"],    Qt::DashDotLine);
    IF_NULL_SET(settings_["gif"]["selector"]["mask"]["color"],      "#88000000");
    IF_NULL_SET(settings_["gif"]["quality"],                        "medium");
    IF_NULL_SET(settings_["gif"]["box"],                            false);
    IF_NULL_SET(settings_["gif"]["mouse"],                          true);

    IF_NULL_SET(settings_["snip"]["hotkey"],                        "F1");
    IF_NULL_SET(settings_["pin"]["hotkey"],                         "F3");
    IF_NULL_SET(settings_["pin"]["visible"]["hotkey"],              "Shift+F3");
    IF_NULL_SET(settings_["record"]["hotkey"],                      "Ctrl+Alt+V");
    IF_NULL_SET(settings_["gif"]["hotkey"],                         "Ctrl+Alt+G");

    IF_NULL_SET(settings_["record"]["framerate"],                   30);
    IF_NULL_SET(settings_["gif"]["framerate"],                      6);
    // clang-format on

    if ((settings_["devices"]["cameras"].is_null() ||
         settings_["devices"]["cameras"].get<std::string>().empty()) &&
        !av::cameras().empty()) {
#ifdef _WIN32
        settings_["devices"]["cameras"] = av::cameras()[0].name;
#else
        settings_["devices"]["cameras"] = av::cameras()[0].id;
#endif
    }

    if (settings_["devices"]["microphones"].is_null()) {
        auto asrc = av::default_audio_source();
        if (asrc.has_value()) {
            settings_["devices"]["microphones"] = asrc.value().id;
        }
    }

    if (settings_["devices"]["speakers"].is_null()) {
        auto asink = av::default_audio_sink();
        if (asink.has_value()) {
            settings_["devices"]["speakers"] = asink.value().id;
        }
    }

    connect(this, &Config::changed, this, &Config::save);
    connect(this, &Config::SYSTEM_THEME_CHANGED, this, [this](int theme) {
        if (settings_["theme"].get<std::string>() == "auto") {
            load_theme(probe::to_string(static_cast<probe::system::theme_t>(theme)));
        }
    });

    monitor_system_theme(settings_["theme"].get<std::string>() == "auto");
}

void Config::save()
{
    QFile file(filepath_);

    if (!file.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text)) return;

    QTextStream out(&file);
    out << settings_.dump(4).c_str();

    file.close();
}

std::string Config::theme()
{
    auto theme = Config::instance()["theme"].get<std::string>();
    if (theme == "auto") {
        return probe::to_string(probe::system::theme());
    }

    return (theme == "dark") ? "dark" : "light";
}

void Config::monitor_system_theme(bool m)
{
#ifdef _WIN32
    if (m && theme_monitor_ == nullptr) {
        theme_monitor_ = std::make_shared<probe::util::registry::RegistryListener>();

        theme_monitor_->listen(
            std::pair<HKEY, std::string>{
                HKEY_CURRENT_USER, R"(Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)" },
            [this](auto) { emit SYSTEM_THEME_CHANGED(static_cast<int>(probe::system::theme())); });
    }

    if (!m) {
        theme_monitor_ = nullptr;
    }
#elif __linux__
    if (m && theme_monitor_ == nullptr) {
        if (probe::system::desktop() == probe::system::desktop_t::GNOME ||
            probe::system::desktop() == probe::system::desktop_t::Unity) {

            theme_monitor_ = std::make_shared<probe::util::PipeListener>();

            auto color_scheme = probe::util::exec_sync(
                { "gsettings", "get", "org.gnome.desktop.interface", "color-scheme" });
            if (!color_scheme.empty() && color_scheme[0] != "\t") { // TODO
                theme_monitor_->listen(
                    std::vector{ "gsettings", "monitor", "org.gnome.desktop.interface", "color-scheme" },
                    [this](auto str) {
                        probe::system::theme_t _theme = probe::system::theme_t::dark;
                        if (std::any_cast<std::string>(str).find("dark") != std::string::npos) {
                            _theme = probe::system::theme_t::dark;
                        }
                        if (std::any_cast<std::string>(str).find("light") != std::string::npos) {
                            _theme = probe::system::theme_t::light;
                        }
                        emit SYSTEM_THEME_CHANGED(static_cast<int>(_theme));
                    });

                return;
            }

            auto gtk_theme =
                probe::util::exec_sync({ "gsettings", "get", "org.gnome.desktop.interface", "gtk-theme" });
            if (!gtk_theme.empty() && gtk_theme[0] != "\t") {
                theme_monitor_->listen(
                    std::vector{ "gsettings", "monitor", "org.gnome.desktop.interface", "gtk-theme" },
                    [this](auto str) {
                        probe::system::theme_t _theme = probe::system::theme_t::dark;
                        if (std::any_cast<std::string>(str).find("dark") != std::string::npos) {
                            _theme = probe::system::theme_t::dark;
                        }
                        if (std::any_cast<std::string>(str).find("light") != std::string::npos) {
                            _theme = probe::system::theme_t::light;
                        }
                        emit SYSTEM_THEME_CHANGED(static_cast<int>(_theme));
                    });

                return;
            }
        }
    }

    if (!m) {
        theme_monitor_ = nullptr;
    }
#endif
}

void Config::set_theme(const std::string& theme)
{
    if (settings_["theme"].get<std::string>() == theme) return;

    set(settings_["theme"], theme);

    monitor_system_theme(theme == "auto");

    load_theme(Config::theme());
}

void Config::load_theme(const std::string& theme)
{
    static std::string _theme = "unknown";

    if (_theme != theme) {
        _theme = theme;

        emit theme_changed();

        std::vector<QString> files{
            ":/stylesheets/capturer",
            ":/stylesheets/capturer-" + QString::fromStdString(theme),
            ":/stylesheets/menu",
            ":/stylesheets/menu-" + QString::fromStdString(theme),
            ":/stylesheets/settingswindow",
            ":/stylesheets/settingswindow-" + QString::fromStdString(theme),
        };

        QString style{};
        for (auto& qss : files) {

            QFile file(qss);
            file.open(QFile::ReadOnly);

            if (file.isOpen()) {
                style += file.readAll();
                file.close();
            }
        }
        qApp->setStyleSheet(style);
    }
}