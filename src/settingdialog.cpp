#include "settingdialog.h"
#include <QDebug>
#include "shortcutinput.h"
#include <string>
#include <QStandardPaths>

using json = nlohmann::json;

QString SettingDialog::default_settings_ =
R"({
    "autorun": true,
    "hotkey": {
        "fix_image": "F3",
        "gif": "Ctrl+Alt+G",
        "snip": "F1",
        "video": "Ctrl+Alt+V"
    }
})";

SettingDialog::SettingDialog(QWidget * parent)
    : QFrame(parent)
{
    config_dir_path_ = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    QDir config_dir(config_dir_path_);
    if(!config_dir.exists()) {
        config_dir.mkpath(config_dir_path_);
    }
    config_file_path_ = config_dir_path_ + QDir::separator() + "config.json";
    if(!QFile::exists(config_file_path_)) {
        QFile config_file(config_file_path_);
        if(config_file.open(QIODevice::ReadWrite | QIODevice::Text)) {
            QTextStream out(&config_file);
            out << default_settings_;
        }
    }

    setWindowFlags(Qt::WindowStaysOnTopHint);

    tabw_ = new QTabWidget(this);
    tabw_->setMinimumSize(300, 400);

    // tab widget
    general_ = new QWidget(this);
    appearance_ = new QWidget(this);
    hotkey_ = new QWidget(this);
    about_ = new QWidget(this);

    tabw_->addTab(general_, "常规");
    tabw_->addTab(appearance_, "外观");
    tabw_->addTab(hotkey_, "快捷键");
    tabw_->addTab(about_, "关于");

    config();

    //
    setupGeneralWidget();
    setupAppearanceWidget();
    setupHotkeyWidget();
    setupAboutWidget();
}

SettingDialog::~SettingDialog()
{
    QFile file(config_file_path_);

    if (!file.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text))
        return;

    QTextStream out(&file);
    out << settings_.dump(4).c_str();
}

void SettingDialog::config()
{
    QFile file(config_file_path_);

    QString text = default_settings_;

    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        text = in.readAll();
    }

    try {
        // parsing input with a syntax error
        settings_ = nlohmann::json::parse(text.toStdString());
    }
    catch (json::parse_error& e) {
        qDebug() << "Parse config.json failed!";
    }
}

void SettingDialog::setAutoRun(int statue)
{
#ifdef _WIN32
        QSettings settings("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
        QString exec_path = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        settings.setValue("capturer_run", statue == Qt::Checked ? exec_path : "");
#elif _LINUX

#endif
    settings_["autorun"] = (statue == Qt::Checked);
}

void SettingDialog::setupGeneralWidget()
{
    auto layout = new QVBoxLayout();
    //
    auto _01 = new QCheckBox("开机自动启动");
    if(settings_["autorun"].is_null()) settings_["auto"] = true;
    _01->setChecked(settings_["autorun"].get<bool>());
    setAutoRun(_01->checkState());
    connect(_01, &QCheckBox::stateChanged, this, &SettingDialog::setAutoRun);

    layout->addWidget(_01);
    layout->addStretch();

    general_->setLayout(layout);
}

void SettingDialog::setupAppearanceWidget()
{

}

void SettingDialog::setupHotkeyWidget()
{
    auto layout = new QGridLayout();

    auto _1_1 = new QLabel("截屏");
    if(settings_["hotkey"]["snip"].is_null())
        settings_["hotkey"]["snip"] = "F1";
    auto _1_2 = new ShortcutInput(JSON_QSTR(settings_["hotkey"]["snip"]));
    connect(_1_2, &ShortcutInput::changed, [&](const QKeySequence& ks){
        settings_["hotkey"]["snip"] = ks.toString().toStdString();
        emit snipShortcutChanged(ks);
    });
    layout->addWidget(_1_1, 1, 1);
    layout->addWidget(_1_2, 1, 2);

    auto _2_1 = new QLabel("贴图");
    if(settings_["hotkey"]["fix_image"].is_null())
        settings_["hotkey"]["fix_image"] = "F3";
    auto _2_2 = new ShortcutInput(JSON_QSTR(settings_["hotkey"]["fix_image"]));
    connect(_2_2, &ShortcutInput::changed, [&](const QKeySequence& ks){
        settings_["hotkey"]["fix_image"] = ks.toString().toStdString();
        emit fixImgShortcutChanged(ks);
    });
    layout->addWidget(_2_1, 2, 1);
    layout->addWidget(_2_2, 2, 2);

    auto _3_1 = new QLabel("GIF");
    if(settings_["hotkey"]["gif"].is_null())
        settings_["hotkey"]["gif"] = "F3";
    auto _3_2 = new ShortcutInput(JSON_QSTR(settings_["hotkey"]["gif"]));
    connect(_3_2, &ShortcutInput::changed,[&](const QKeySequence& ks) {
        settings_["hotkey"]["gif"] = ks.toString().toStdString();
        emit gifShortcutChanged(ks);
    });
    layout->addWidget(_3_1, 3, 1);
    layout->addWidget(_3_2, 3, 2);

    auto _4_1 = new QLabel("录屏");
    if(settings_["hotkey"]["video"].is_null())
        settings_["hotkey"]["video"] = "F3";
    auto _4_2 = new ShortcutInput(JSON_QSTR(settings_["hotkey"]["video"]));
    connect(_4_2, &ShortcutInput::changed, [&](const QKeySequence& ks) {
        settings_["hotkey"]["video"] = ks.toString().toStdString();
        emit videoShortcutChanged(ks);
    });
    layout->addWidget(_4_1, 4, 1);
    layout->addWidget(_4_2, 4, 2);

    layout->setRowStretch(5, 1);

    hotkey_->setLayout(layout);
}
void SettingDialog::setupAboutWidget()
{
    new QLabel("Copyright (C) 2018 Zhang Liangqi", about_);
}

