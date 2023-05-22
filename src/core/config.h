#ifndef CONFIG_H
#define CONFIG_H

#include "json.h"
#include "probe/util.h"
#include "utils.h"

#include <QObject>

class Config : public QObject
{
    Q_OBJECT

public:
    static Config& instance()
    {
        static Config instance;
        return instance;
    }

    Config(const Config&)            = delete;
    Config& operator=(const Config&) = delete;

    [[nodiscard]] QString getFilePath() const { return filepath_; }

    static std::string theme();

    // use this function to set theme
    void set_theme(const std::string&);
    void load_theme(const std::string&);

    template<typename T> void set(json& key, T value)
    {
        key = value;
        emit changed();
    }

    decltype(auto) operator[](const std::string& key) { return settings_[key]; }

    decltype(auto) operator[](const std::string& key) const { return settings_[key]; }

public slots:
    void save();

signals:
    void changed();
    void theme_changed();
    void SYSTEM_THEME_CHANGED(int);

private:
    Config();

    void monitor_system_theme(bool);

    QString filepath_;
    json settings_ = json::parse("{}");

    std::shared_ptr<probe::Listener> theme_monitor_{ nullptr };
};

#endif // CONFIG_H
