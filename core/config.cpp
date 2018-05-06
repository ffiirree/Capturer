#include "config.h"
#include <QStandardPaths>
#include <QDir>
#include <QTextStream>
#include <QDebug>

JSON Config::settings_ = JSON::parse("{}");

Config* Config::Instance()
{
    static Config instance;
    return &instance;
}

Config::Config()
{
    auto config_dir_path_ = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir config_dir(config_dir_path_);
    if(!config_dir.exists()) {
        config_dir.mkpath(config_dir_path_);
    }
    config_file_path_ = config_dir_path_ + QDir::separator() + "config.json";

    QString text;
    QFile config_file(config_file_path_);
    if(config_file.open(QIODevice::ReadWrite | QIODevice::Text)) {
        QTextStream in(&config_file);
        text = in.readAll();
    }

    try {
        settings_ = JSON::parse(text.toStdString());
    }
    catch (JSON::parse_error& e) {
        settings_ = JSON::parse("{}");

        Q_UNUSED(e);
        qDebug() << "Parse config.json failed!";
    }

    // default
    IF_NULL_SET(AUTORUN, true);

    IF_NULL_SET(SNIP_SBW, 1);
    IF_NULL_SET(SNIP_SBC, "#00ffff");
    IF_NULL_SET(SNIP_SBS, Qt::DashDotLine);
    IF_NULL_SET(SNIP_SMC, "#64000000");

    IF_NULL_SET(RECORD_SBW, 1);
    IF_NULL_SET(RECORD_SBC, "#ffff5500");
    IF_NULL_SET(RECORD_SBS, Qt::DashDotLine);
    IF_NULL_SET(RECORD_SMC, "#64000000");

    IF_NULL_SET(GIF_SBW, 1);
    IF_NULL_SET(GIF_SBC, "#ffff00ff");
    IF_NULL_SET(GIF_SBS, Qt::DashDotLine);
    IF_NULL_SET(GIF_SMC, "#64000000");

    IF_NULL_SET(SNIP_HOTKEY, "F1");
    IF_NULL_SET(PIN_HOTKEY, "F3");
    IF_NULL_SET(RECORD_HOTKEY, "Ctrl+Alt+V");
    IF_NULL_SET(GIF_HOTKEY, "Ctrl+Alt+G");

    IF_NULL_SET(SNIP_DW, true);
    IF_NULL_SET(RECORD_DW, true);
    IF_NULL_SET(GIF_DW, true);

    IF_NULL_SET(RECORD_FRAMERATE, 30);
    IF_NULL_SET(GIF_FPS, 6);

    connect(this, &Config::changed, [this]() { save(); });
}

void Config::save()
{
    QFile file(config_file_path_);

    if (!file.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text))
        return;

    QTextStream out(&file);
    out << settings_.dump(4).c_str();

    file.close();
}
