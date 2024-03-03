#ifndef CAPTURER_CONFIG_H
#define CAPTURER_CONFIG_H

#include "libcap/media.h"
#include "selector.h"

#include <functional>

namespace config
{
    inline bool        autorun{ true };
    inline QString     language{ "zh_CN" };
    inline std::string theme{ "auto" }; // auto, dark, light
    inline QString     filepath{};

    namespace hotkeys
    {
        inline QKeySequence screenshot{ "F1" };
        inline QKeySequence preview{ "F3" };
        inline QKeySequence toggle_previews{ "Shift+F3" };
        inline QKeySequence quick_look{ "F2" };
        inline QKeySequence record_video{ "Ctrl+Alt+V" };
        inline QKeySequence record_gif{ "Ctrl+Alt+G" };
        inline QKeySequence transparent_input{ "Ctrl+T" };
    }; // namespace hotkeys

    namespace snip
    {
        inline SelectorStyle style{ 2, "#409EFF", Qt::SolidLine, "#88000000" };
        inline QString       path{};
    } // namespace snip

    namespace recording
    {
        namespace video
        {
            inline SelectorStyle style{ 2, "#ffff5500", Qt::SolidLine, "#88000000" };
            inline bool          show_region{ true };
            inline bool          capture_mouse{ true };
            inline bool          floating_menu{ true };

            inline QString mcf{ "mp4" };
            inline QString path{};

            inline bool mic_enabled{ false };
            inline bool speaker_enabled{ true };

            namespace v
            {
                inline std::string codec{ "libx264" };
                // options
                inline AVRational  framerate{ 30, 1 };
                // H.264: 0-51, default 23
                // H.265: 0-51, default 28
                // Vp9  : 0-63, 15-35
                // Values of ±6 will result in about half or twice the original bitrate.
                inline int         crf{ 23 }; // CRF or  CQ
                inline std::string rate_control{ "crf" };
                inline std::string preset{ "medium" };
                inline std::string profile{ "high" };
                inline int         bitrate{}; // kbs
                inline std::string maxrate{};
                inline std::string tuning{};
            } // namespace v

            namespace a
            {
                inline std::string codec{ "aac" };
                // options
                inline int         channels{ 2 };
                inline int         sample_rate{ 48000 };
            } // namespace a
        };    // namespace video

        namespace gif
        {
            inline SelectorStyle style{ 2, "#ffff00ff", Qt::SolidLine, "#88000000" };
            inline QString       path{};
            inline bool          show_region{ true };
            inline bool          capture_mouse{ true };
            inline bool          floating_menu{ true };

            // options
            inline AVRational framerate{ 6, 1 };
            inline int        colors{ 128 };
            inline bool       dither{ false };
        }; // namespace gif
    };     // namespace recording

    namespace devices
    {
        inline std::string mic{};
        inline std::string speaker{};
        inline std::string camera{};
    } // namespace devices

    void from_json(const json& j);
    void to_json(json& j);
} // namespace config

namespace config
{
    inline constexpr auto                      filename = "capturer.json";
    inline std::shared_ptr<probe::Listener>    theme_monitor{};
    inline std::function<void(const QString&)> on_theme_changed = [](auto) {};

    void load();
    void save();

    QString definite_theme();

    bool set_autorun(bool autorun);

    void set_theme(const std::string& theme);

    void monitor_system_theme(bool en);
} // namespace config

#endif //! CAPTURER_CONFIG_H
