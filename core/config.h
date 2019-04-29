#ifndef CONFIG_H
#define CONFIG_H

#include <QObject>
#include <QKeySequence>
#include <QColor>
#include "json.hpp"

using JSON = nlohmann::json;

#define IF_NULL_SET(X, default_value) st(if(X.is_null())  X = default_value;)
#define GET_INT(X)  X.get<int>()
#define GET_STR(X)  X.get<std::string>()
#define GET_QSTR(X) QString::fromStdString(GET_STR(X))
#define GET_BOOL(X) X.get<bool>()

#define SET(X, value) X = value; emit changed();

#define SETTINGS        Config::rsettings()

//
#define AUTORUN         SETTINGS["autorun"]

// selector
#define SNIP_SBW        SETTINGS["snip"]["selector"]["border"]["width"]
#define SNIP_SBC        SETTINGS["snip"]["selector"]["border"]["color"]
#define SNIP_SBS        SETTINGS["snip"]["selector"]["border"]["style"]
#define SNIP_SMC        SETTINGS["snip"]["selector"]["mask"]["color"]

#define RECORD_SBW      SETTINGS["record"]["selector"]["border"]["width"]
#define RECORD_SBC      SETTINGS["record"]["selector"]["border"]["color"]
#define RECORD_SBS      SETTINGS["record"]["selector"]["border"]["style"]
#define RECORD_SMC      SETTINGS["record"]["selector"]["mask"]["color"]

#define GIF_SBW         SETTINGS["gif"]["selector"]["border"]["width"]
#define GIF_SBC         SETTINGS["gif"]["selector"]["border"]["color"]
#define GIF_SBS         SETTINGS["gif"]["selector"]["border"]["style"]
#define GIF_SMC         SETTINGS["gif"]["selector"]["mask"]["color"]

// shortcut
#define SNIP_HOTKEY     SETTINGS["snip"]["hotkey"]["snip"]
#define PIN_HOTKEY      SETTINGS["snip"]["hotkey"]["pin"]
#define RECORD_HOTKEY   SETTINGS["record"]["hotkey"]["record"]
#define GIF_HOTKEY      SETTINGS["gif"]["hotkey"]["record"]

// detectwindow
#define SNIP_DW         SETTINGS["snip"]["detectwindow"]
#define RECORD_DW       SETTINGS["record"]["detectwindow"]
#define GIF_DW          SETTINGS["gif"]["detectwindow"]

// record params
#define RECORD_FRAMERATE    SETTINGS["record"]["params"]["framerate"]

#define GIF_FPS             SETTINGS["gif"]["params"]["fps"]



class Config : public QObject
{
    Q_OBJECT

public:
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    void save();

    static Config* Instance();

    static JSON& rsettings() { return settings_; }

    template <typename T> T get(const JSON& key) const { return _get(key, static_cast<T *>(nullptr)); }

    template <typename T> void set(JSON& key, T value) { SET(key, value); }
    void set(JSON &key, const QKeySequence& ks) { SET(key, ks.toString().toStdString()); }
    void set(JSON &key, const QColor& c) { SET(key, c.name(QColor::HexArgb).toStdString()); }

signals:
    void changed();

private:
    inline bool _get(const JSON &key, bool*) const { return GET_BOOL(key); }
    inline int _get(const JSON &key, int*) const { return GET_INT(key); }
    inline std::string _get(const JSON &key, std::string*) const { return GET_STR(key); }
    inline QString _get(const JSON &key, QString*) const { return GET_QSTR(key); }
    inline QColor _get(const JSON &key, QColor*) const { return QColor(GET_QSTR(key)); }
    inline QKeySequence _get(const JSON &key, QKeySequence*) const { return QKeySequence(GET_QSTR(key)); }
    inline Qt::PenStyle _get(const JSON &key, Qt::PenStyle *) const { return Qt::PenStyle(GET_INT(key)); }

private:
    Config();

    QString config_file_path_;

    static JSON settings_;
};

#endif // CONFIG_H
