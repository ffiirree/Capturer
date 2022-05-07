#include "config.h"
#include <QStandardPaths>
#include <QDir>
#include <QTextStream>
#include "utils.h"
#include "devices.h"
#include "logging.h"

Config::Config()
{
    auto config_dir_path_ = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir config_dir(config_dir_path_);
    if(!config_dir.exists()) {
        config_dir.mkpath(config_dir_path_);
    }
    filepath_ = config_dir_path_ + "/config.json";
    LOG(INFO) << "config file path: " << filepath_;

    QString text;
    QFile config_file(filepath_);
    if(config_file.open(QIODevice::ReadWrite | QIODevice::Text)) {
        QTextStream in(&config_file);
        text = in.readAll();
    }

    try {
        settings_ = json::parse(text.toStdString());
    }
    catch (json::parse_error&) {
        settings_ = json::parse("{}");
    }

    // default
    IF_NULL_SET(settings_["autorun"], true);
    IF_NULL_SET(settings_["language"], "zh_CN");
    IF_NULL_SET(settings_["detectwindow"], true);
    IF_NULL_SET(settings_["theme"], "dark");

    IF_NULL_SET(settings_["snip"]["selector"]["border"]["width"],   1);
    IF_NULL_SET(settings_["snip"]["selector"]["border"]["color"],   "#409EFF");
    IF_NULL_SET(settings_["snip"]["selector"]["border"]["style"],   Qt::DashDotLine);
    IF_NULL_SET(settings_["snip"]["selector"]["mask"]["color"],     "#88000000");

    IF_NULL_SET(settings_["record"]["selector"]["border"]["width"], 1);
    IF_NULL_SET(settings_["record"]["selector"]["border"]["color"], "#ffff5500");
    IF_NULL_SET(settings_["record"]["selector"]["border"]["style"], Qt::DashDotLine);
    IF_NULL_SET(settings_["record"]["selector"]["mask"]["color"],   "#88000000");

    IF_NULL_SET(settings_["gif"]["selector"]["border"]["width"],    1);
    IF_NULL_SET(settings_["gif"]["selector"]["border"]["color"],    "#ffff00ff");
    IF_NULL_SET(settings_["gif"]["selector"]["border"]["style"],    Qt::DashDotLine);
    IF_NULL_SET(settings_["gif"]["selector"]["mask"]["color"],      "#88000000");
    IF_NULL_SET(settings_["gif"]["quality"],                        "medium");

    IF_NULL_SET(settings_["snip"]["hotkey"],                        "F1");
    IF_NULL_SET(settings_["pin"]["hotkey"],                         "F3");
    IF_NULL_SET(settings_["pin"]["visiable"]["hotkey"],             "Shift+F3");
    IF_NULL_SET(settings_["record"]["hotkey"],                      "Ctrl+Alt+V");
    IF_NULL_SET(settings_["gif"]["hotkey"],                         "Ctrl+Alt+G");

    IF_NULL_SET(settings_["record"]["framerate"],                   30);
    IF_NULL_SET(settings_["gif"]["framerate"],                      6);

    if(Devices::cameras().size() > 0)
        settings_["devices"]["cameras"] = Devices::cameras()[0];

    connect(this, &Config::changed, this, &Config::save);
}

void Config::save()
{
    QFile file(filepath_);

    if (!file.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text))
        return;

    QTextStream out(&file);
    out << settings_.dump(4).c_str();

    file.close();
}
