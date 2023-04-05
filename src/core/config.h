#ifndef CONFIG_H
#define CONFIG_H

#include <QObject>
#include "json.h"
#include "utils.h"
#include "platform.h"

#define IF_NULL_SET(X, default_value) st(if(X.is_null())  X = default_value;)

class Config : public QObject
{
    Q_OBJECT

public:
    static Config& instance()
    {
        static Config instance;
        return instance;
    }

    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    QString getFilePath() const { return filepath_; }

    static std::string theme();

    // use this funtion to set theme
    void set_theme(const std::string&);
    static void load_theme(const std::string&);

    template <typename T> void set(json& key, T value)
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
    void SYSTEM_THEME_CHANGED(int);

private:
    Config();

    void monitor_system_theme(bool);

    QString filepath_;
    json settings_ = json::parse("{}");

#ifdef _WIN32
    std::shared_ptr<platform::windows::RegistryMonitor> theme_monitor_{ nullptr };
#elif __linux__
    std::shared_ptr<platform::linux::GSettingsMonitor> theme_monitor_{ nullptr };
#endif // _WIN32
};


#endif // CONFIG_H
