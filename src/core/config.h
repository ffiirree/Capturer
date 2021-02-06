#ifndef CONFIG_H
#define CONFIG_H

#include <QObject>
#include "json.h"
#include "utils.h"

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

private:
    Config();

    QString filepath_;
    json settings_ = json::parse("{}");
};


#endif // CONFIG_H
