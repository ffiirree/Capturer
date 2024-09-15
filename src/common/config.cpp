#include "config.h"

#include "json.h"
#include "logging.h"

#include <probe/system.h>
#include <QApplication>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>

#define JSON_GET(V, J, KEY)                                                                                \
    if ((J).contains(KEY)) V = (J)[KEY].get<decltype(V)>();

namespace config
{
    void from_json(const json& j)
    {
        JSON_GET(autorun, j, "autorun");
        JSON_GET(language, j, "language");
        JSON_GET(theme, j, "theme");

        if (j.contains("hotkeys")) {
            JSON_GET(hotkeys::screenshot, j["hotkeys"], "screenshot");
            JSON_GET(hotkeys::preview, j["hotkeys"], "preview");
            JSON_GET(hotkeys::toggle_previews, j["hotkeys"], "toggle-previews");
            JSON_GET(hotkeys::quick_look, j["hotkeys"], "quick-look");
            JSON_GET(hotkeys::record_video, j["hotkeys"], "record-video");
            JSON_GET(hotkeys::record_gif, j["hotkeys"], "record-gif");
            JSON_GET(hotkeys::transparent_input, j["hotkeys"], "transparent-input");
        }

        if (j.contains("snip")) {
            if (j["snip"].contains("style")) {
                JSON_GET(snip::style.border_width, j["snip"]["style"], "border-width");
                JSON_GET(snip::style.border_color, j["snip"]["style"], "border-color");
                JSON_GET(snip::style.border_style, j["snip"]["style"], "border-style");
                JSON_GET(snip::style.mask_color, j["snip"]["style"], "mask-color");
            }

            JSON_GET(snip::path, j["snip"], "save-path");
        }

        if (j.contains("recording")) {
            if (j["recording"].contains("video")) {
                using namespace recording::video;

                if (j["recording"]["video"].contains("style")) {
                    JSON_GET(style.border_width, j["recording"]["video"]["style"], "border-width");
                    JSON_GET(style.border_color, j["recording"]["video"]["style"], "border-color");
                    JSON_GET(style.border_style, j["recording"]["video"]["style"], "border-style");
                    JSON_GET(style.mask_color, j["recording"]["video"]["style"], "mask-color");
                }

                JSON_GET(show_region, j["recording"]["video"], "show-region");
                JSON_GET(capture_mouse, j["recording"]["video"], "capture-mouse");
                JSON_GET(floating_menu, j["recording"]["video"], "floating-menu");

                JSON_GET(mcf, j["recording"]["video"], "container-format");
                JSON_GET(path, j["recording"]["video"], "save-path");

                JSON_GET(mic_enabled, j["recording"]["video"], "mic-enabled");
                JSON_GET(speaker_enabled, j["recording"]["video"], "speaker-enabled");

                if (j["recording"]["video"].contains("v")) {
                    JSON_GET(v::codec, j["recording"]["video"]["v"], "codec");
                    if (j["recording"]["video"]["v"].contains("framerate")) {
                        JSON_GET(v::framerate.num, j["recording"]["video"]["v"]["framerate"], "num");
                        JSON_GET(v::framerate.den, j["recording"]["video"]["v"]["framerate"], "den");
                    }
                    JSON_GET(v::rate_control, j["recording"]["video"]["v"], "rate-control");
                    JSON_GET(v::crf, j["recording"]["video"]["v"], "crf");
                    JSON_GET(v::preset, j["recording"]["video"]["v"], "preset");
                    JSON_GET(v::profile, j["recording"]["video"]["v"], "profile");
                    JSON_GET(v::tune, j["recording"]["video"]["v"], "tune");
                    JSON_GET(v::pix_fmt, j["recording"]["video"]["v"], "pixel-format");
                    JSON_GET(v::color_space, j["recording"]["video"]["v"], "color-space");
                    JSON_GET(v::color_range, j["recording"]["video"]["v"], "color-range");
                }
                if (j["recording"]["video"].contains("a")) {
                    JSON_GET(a::codec, j["recording"]["video"]["a"], "codec");
                    JSON_GET(a::channels, j["recording"]["video"]["a"], "channels");
                    JSON_GET(a::sample_rate, j["recording"]["video"]["a"], "sample-rate");
                }
            }

            if (j["recording"].contains("gif")) {
                using namespace recording::gif;

                if (j["recording"]["gif"].contains("style")) {
                    JSON_GET(style.border_width, j["recording"]["gif"]["style"], "border-width");
                    JSON_GET(style.border_color, j["recording"]["gif"]["style"], "border-color");
                    JSON_GET(style.border_style, j["recording"]["gif"]["style"], "border-style");
                    JSON_GET(style.mask_color, j["recording"]["gif"]["style"], "mask-color");
                }

                JSON_GET(path, j["recording"]["gif"], "save-path");
                JSON_GET(show_region, j["recording"]["gif"], "show-region");
                JSON_GET(capture_mouse, j["recording"]["gif"], "captuer-mouse");
                JSON_GET(floating_menu, j["recording"]["gif"], "floating-menu");

                JSON_GET(framerate.num, j["recording"]["gif"], "framerate");
                JSON_GET(colors, j["recording"]["gif"], "colors");
                JSON_GET(dither, j["recording"]["gif"], "dither");
            }
        }
    }

    json to_json()
    {
        json j;

        j["autorun"]  = autorun;
        j["language"] = language;
        j["theme"]    = theme;

        j["hotkeys"]["screenshot"]        = hotkeys::screenshot;
        j["hotkeys"]["preview"]           = hotkeys::preview;
        j["hotkeys"]["toggle-previews"]   = hotkeys::toggle_previews;
        j["hotkeys"]["quick-look"]        = hotkeys::quick_look;
        j["hotkeys"]["record-video"]      = hotkeys::record_video;
        j["hotkeys"]["record-gif"]        = hotkeys::record_gif;
        j["hotkeys"]["transparent-input"] = hotkeys::transparent_input;

        j["snip"]["style"]["border-width"] = snip::style.border_width;
        j["snip"]["style"]["border-color"] = snip::style.border_color;
        j["snip"]["style"]["border-style"] = snip::style.border_style;
        j["snip"]["style"]["mask-color"]   = snip::style.mask_color;

        j["snip"]["save-path"] = snip::path;

        j["recording"]["video"]["style"]["border-width"] = recording::video::style.border_width;
        j["recording"]["video"]["style"]["border-color"] = recording::video::style.border_color;
        j["recording"]["video"]["style"]["border-style"] = recording::video::style.border_style;
        j["recording"]["video"]["style"]["mask-color"]   = recording::video::style.mask_color;

        j["recording"]["video"]["show-region"]   = recording::video::show_region;
        j["recording"]["video"]["capture-mouse"] = recording::video::capture_mouse;
        j["recording"]["video"]["floating-menu"] = recording::video::floating_menu;

        j["recording"]["video"]["container-format"] = recording::video::mcf;
        j["recording"]["video"]["save-path"]        = recording::video::path;

        j["recording"]["video"]["mic-enabled"]     = recording::video::mic_enabled;
        j["recording"]["video"]["speaker-enabled"] = recording::video::speaker_enabled;

        j["recording"]["video"]["v"]["codec"]            = recording::video::v::codec;
        j["recording"]["video"]["v"]["framerate"]["num"] = recording::video::v::framerate.num;
        j["recording"]["video"]["v"]["framerate"]["den"] = recording::video::v::framerate.den;
        j["recording"]["video"]["v"]["rate-control"]     = recording::video::v::rate_control;
        j["recording"]["video"]["v"]["crf"]              = recording::video::v::crf;
        j["recording"]["video"]["v"]["preset"]           = recording::video::v::preset;
        j["recording"]["video"]["v"]["profile"]          = recording::video::v::profile;
        j["recording"]["video"]["v"]["tune"]             = recording::video::v::tune;
        j["recording"]["video"]["v"]["pixel-format"]     = recording::video::v::pix_fmt;
        j["recording"]["video"]["v"]["color-space"]      = recording::video::v::color_space;
        j["recording"]["video"]["v"]["color-range"]      = recording::video::v::color_range;

        j["recording"]["video"]["a"]["codec"]       = recording::video::a::codec;
        j["recording"]["video"]["a"]["channels"]    = recording::video::a::channels;
        j["recording"]["video"]["a"]["sample-rate"] = recording::video::a::sample_rate;

        j["recording"]["gif"]["style"]["border-width"] = recording::gif::style.border_width;
        j["recording"]["gif"]["style"]["border-color"] = recording::gif::style.border_color;
        j["recording"]["gif"]["style"]["border-style"] = recording::gif::style.border_style;
        j["recording"]["gif"]["style"]["mask-color"]   = recording::gif::style.mask_color;

        j["recording"]["gif"]["save-path"]     = recording::gif::path;
        j["recording"]["gif"]["show-region"]   = recording::gif::show_region;
        j["recording"]["gif"]["captuer-mouse"] = recording::gif::capture_mouse;
        j["recording"]["gif"]["floating-menu"] = recording::gif::floating_menu;

        j["recording"]["gif"]["framerate"] = recording::gif::framerate.num;
        j["recording"]["gif"]["colors"]    = recording::gif::colors;
        j["recording"]["gif"]["dither"]    = recording::gif::dither;

        return j;
    }
} // namespace config

namespace config
{
    void load()
    {
        filepath = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + "/" + filename;
        // load configure file
        QString text;
        QFile   file(filepath);
        if (file.open(QIODevice::ReadWrite | QIODevice::Text)) {
            QTextStream in(&file);
            text = in.readAll();
        }

        // parse json string
        try {
            from_json(json::parse(text.toStdString()));
        }
        catch (json::parse_error&) {
        }

        if (snip::path.isEmpty())
            snip::path = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);

        if (recording::video::path.isEmpty())
            recording::video::path = QStandardPaths::writableLocation(QStandardPaths::MoviesLocation);

        if (recording::gif::path.isEmpty())
            recording::gif::path = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    }

    void save()
    {
        QFile file(filepath);

        if (!file.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text)) return;

        QTextStream out(&file);
        out << to_json().dump(4).c_str();

        file.close();
    }

    QString definite_theme()
    {
        if (theme == "auto") {
            return probe::to_string(probe::system::theme()).c_str();
        }

        return (theme == "dark") ? "dark" : "light";
    }

    void set_theme(const std::string& nt)
    {
        if (theme == nt) return;

        theme = nt;

        monitor_system_theme(theme == "auto");

        on_theme_changed(definite_theme());
    }

    void monitor_system_theme(bool m)
    {
#ifdef _WIN32
        if (m && theme_monitor == nullptr) {
            theme_monitor = std::make_shared<probe::util::registry::RegistryListener>();

            theme_monitor->listen(
                std::pair<HKEY, std::string>{
                    HKEY_CURRENT_USER, R"(Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)" },
                [&](auto) { set_theme(probe::to_string(probe::system::theme())); });
        }

        if (!m) {
            theme_monitor = nullptr;
        }
#elif __linux__
        if (m && theme_monitor == nullptr) {
            if (probe::system::desktop_environment() == probe::system::desktop_environment_t::GNOME ||
                probe::system::desktop_environment() == probe::system::desktop_environment_t::Unity) {

                theme_monitor = std::make_shared<probe::util::PipeListener>();

                auto color_scheme = probe::util::exec_sync(
                    { "gsettings", "get", "org.gnome.desktop.interface", "color-scheme" });
                if (!color_scheme.empty() && color_scheme[0] != "\t") { // TODO
                    theme_monitor->listen(std::vector{ "gsettings", "monitor",
                                                       "org.gnome.desktop.interface", "color-scheme" },
                                          [](auto str) {
                                              auto _theme = probe::system::theme_t::light;
                                              if (std::any_cast<std::string>(str).find("dark") !=
                                                  std::string::npos) {
                                                  _theme = probe::system::theme_t::dark;
                                              }
                                              set_theme(probe::to_string(_theme));
                                          });

                    return;
                }

                auto gtk_theme = probe::util::exec_sync(
                    { "gsettings", "get", "org.gnome.desktop.interface", "gtk-theme" });
                if (!gtk_theme.empty() && gtk_theme[0] != "\t") {
                    theme_monitor->listen(
                        std::vector{ "gsettings", "monitor", "org.gnome.desktop.interface", "gtk-theme" },
                        [](auto str) {
                            auto _theme = probe::system::theme_t::light;
                            if (std::any_cast<std::string>(str).find("dark") != std::string::npos) {
                                _theme = probe::system::theme_t::dark;
                            }

                            set_theme(probe::to_string(_theme));
                        });

                    return;
                }
            }
        }

        if (!m) {
            theme_monitor = nullptr;
        }
#endif
    }

    bool set_autorun(bool state)
    {
#ifdef _WIN32
        QSettings run(R"(HKEY_CURRENT_USER\SOFTWARE\Microsoft\Windows\CurrentVersion\Run)",
                      QSettings::NativeFormat);
        run.setValue("capturer_run",
                     state ? QDir::toNativeSeparators(QCoreApplication::applicationFilePath()) : "");
#elif __linux__
        const std::string desktop_file = "/usr/share/applications/capturer.desktop";
        const std::string autorun_dir  = std::string{ ::getenv("HOME") } + "/.config/autostart";
        const std::string autorun_file = autorun_dir + "/capturer.desktop";

        if (!std::filesystem::exists(desktop_file)) {
            logw("failed to set `autorun` since the '{}' does not exists.", desktop_file);
            state = false;
        }

        if (state) {
            if (std::filesystem::exists(desktop_file) && !std::filesystem::exists(autorun_file)) {
                if (!std::filesystem::exists(autorun_dir)) {
                    std::filesystem::create_directories(autorun_dir);
                }
                std::filesystem::create_symlink(desktop_file, autorun_file);
            }
        }
        else {
            std::filesystem::remove(autorun_file);
        }
#endif

        config::autorun = state;

        return state;
    }

} // namespace config
