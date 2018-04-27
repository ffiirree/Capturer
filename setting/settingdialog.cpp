#include "settingdialog.h"
#include <QDebug>
#include "shortcutinput.h"
#include <string>
#include <QStandardPaths>
#include <QSpinBox>
#include <QComboBox>
#include "colorbutton.h"

using json = nlohmann::json;

QString SettingWindow::default_settings_ =
R"({
    "autorun": true,
    "hotkey": {
        "fix_image": "F3",
        "gif": "Ctrl+Alt+G",
        "snip": "F1",
        "video": "Ctrl+Alt+V"
    }
})";

SettingWindow::SettingWindow(QWidget * parent)
    : QWidget(parent)
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

    setWindowFlags((windowFlags()&~Qt::WindowMinMaxButtonsHint)
                    | Qt::WindowStaysOnTopHint);

    tabw_ = new QTabWidget(this);
    tabw_->setMinimumSize(300, 400);

    // tab widget
    general_ = new QWidget(this);
    snip_ = new QWidget(this);
    record_ = new QWidget(this);
    gif_ = new QWidget(this);
    about_ = new QWidget(this);

    tabw_->addTab(general_, "常规");
    tabw_->addTab(snip_, "截屏");
    tabw_->addTab(record_, "录屏");
    tabw_->addTab(gif_, "GIF");
    tabw_->addTab(about_, "关于");

    config();

    //
    setupGeneralWidget();
    setupSnipWidget();
    setupRecordWidget();
    setupGIFWidget();
    setupAboutWidget();

    auto layout = new QVBoxLayout();

    // tab widget
    layout->addWidget(tabw_);
    layout->setMargin(0);
    layout->setSpacing(0);
    setLayout(layout);
}

SettingWindow::~SettingWindow()
{
    QFile file(config_file_path_);

    if (!file.open(QIODevice::ReadWrite | QIODevice::Truncate | QIODevice::Text))
        return;

    QTextStream out(&file);
    out << settings_.dump(4).c_str();
}

void SettingWindow::config()
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
        Q_UNUSED(e);
        qDebug() << "Parse config.json failed!";
    }
}

void SettingWindow::setAutoRun(int statue)
{
#ifdef _WIN32
        QSettings settings("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
        QString exec_path = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
        settings.setValue("capturer_run", statue == Qt::Checked ? exec_path : "");
#elif _LINUX

#endif
    settings_["autorun"] = (statue == Qt::Checked);
}

void SettingWindow::setupGeneralWidget()
{
    auto layout = new QVBoxLayout();
    //
    auto _01 = new QCheckBox("开机自动启动");
    if(settings_["autorun"].is_null()) settings_["autorun"] = true;
    _01->setChecked(settings_["autorun"].get<bool>());
    setAutoRun(_01->checkState());
    connect(_01, &QCheckBox::stateChanged, this, &SettingWindow::setAutoRun);

    layout->addWidget(_01);
    layout->addStretch();

    general_->setLayout(layout);
}

void SettingWindow::setupSnipWidget()
{
    auto tabwidget = new QTabWidget(snip_);

    auto theme = new QWidget(snip_);
    auto hotkey = new QWidget(snip_);
    auto behavior = new QWidget(snip_);
    auto pin = new QWidget(snip_);

    tabwidget->addTab(theme, "外观");
    tabwidget->addTab(hotkey, "快捷键");
    tabwidget->addTab(behavior, "行为");
    tabwidget->addTab(pin, "贴图");

    // theme
    {
        auto theme_layout = new QGridLayout();
        auto _1_1 = new QLabel("边框宽度");
        auto _1_2 = new QSpinBox();
        _1_2->setFixedHeight(25);
        if(settings_["snip"]["selector"]["border"]["width"].is_null())
            settings_["snip"]["selector"]["border"]["width"] = 1;
        _1_2->setValue(settings_["snip"]["selector"]["border"]["width"].get<int>());
        connect(_1_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [&](int w){
            emit snipBorderWidthChanged(w);
            settings_["snip"]["selector"]["border"]["width"] = w;
        });
        theme_layout->addWidget(_1_1, 1, 1);
        theme_layout->addWidget(_1_2, 1, 2);

        auto _2_1 = new QLabel("边框颜色");
        auto _2_2 = new ColorButton();
        _2_2->setFixedHeight(25);
        if(settings_["snip"]["selector"]["border"]["color"].is_null())
            settings_["snip"]["selector"]["border"]["color"] = QColor(Qt::cyan).name().toStdString();
        _2_2->color(JSON_QSTR(settings_["snip"]["selector"]["border"]["color"]));
        connect(_2_2, &ColorButton::changed, [&](const QColor& color){
            emit snipBorderColorChanged(color);
            settings_["snip"]["selector"]["border"]["color"] = color.name().toStdString();
        });
        theme_layout->addWidget(_2_2, 2, 2);
        theme_layout->addWidget(_2_1, 2, 1);

        auto _3_1 = new QLabel("边框线条");
        auto _3_2 = new QComboBox();
        _3_2->setFixedHeight(25);
        _3_2->addItem("NoPen");
        _3_2->addItem("SolidLine");
        _3_2->addItem("DashLine");
        _3_2->addItem("DotLine");
        _3_2->addItem("DashDotLine");
        _3_2->addItem("DashDotDotLine");
        _3_2->addItem("CustomDashLine");
        if(settings_["snip"]["selector"]["border"]["style"].is_null())
            settings_["snip"]["selector"]["border"]["style"] = (int)Qt::DashDotLine;
        _3_2->setCurrentIndex(settings_["snip"]["selector"]["border"]["style"].get<int>());
        connect(_3_2,  static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [&](int s){
            emit snipBorderStyleChanged(Qt::PenStyle(s));
            settings_["snip"]["selector"]["border"]["style"] = s;
        });
        theme_layout->addWidget(_3_2, 3, 2);
        theme_layout->addWidget(_3_1, 3, 1);

        auto _4_1 = new QLabel("遮罩颜色");
        auto _4_2 = new ColorButton();
        _4_2->setFixedHeight(25);
        if(settings_["snip"]["selector"]["mask"]["color"].is_null())
            settings_["snip"]["selector"]["mask"]["color"] = QColor(0, 0, 0, 100).name(QColor::HexArgb).toStdString();
        _4_2->color(JSON_QSTR(settings_["snip"]["selector"]["mask"]["color"]));
        connect(_4_2, &ColorButton::changed, [&](const QColor& color){
            emit snipMaskColorChanged(color);
            settings_["snip"]["selector"]["mask"]["color"] = color.name().toStdString();
        });
        theme_layout->addWidget(_4_2, 4, 2);
        theme_layout->addWidget(_4_1, 4, 1);

        theme_layout->setRowStretch(5, 1);
        theme->setLayout(theme_layout);
    }

    // behavier
    {
        auto behavier_layout = new QGridLayout();

        auto _01 = new QCheckBox("开启窗口探测");
        if(settings_["snip"]["detectwindow"].is_null()) settings_["snip"]["detectwindow"] = true;
        _01->setChecked(settings_["snip"]["detectwindow"].get<bool>());
        connect(_01, &QCheckBox::stateChanged, [this](int state){
            emit snipDetectWindowChanged(state == Qt::Checked);
            settings_["snip"]["detectwindow"] = (state == Qt::Checked);
        });

        behavier_layout->addWidget(_01);
        behavier_layout->setRowStretch(2, 1);

        behavior->setLayout(behavier_layout);
    }

    // hotkey
    {
        auto hotkey_layout = new QGridLayout();

        auto _1_1 = new QLabel("截屏");
        if(settings_["snip"]["hotkey"]["snip"].is_null())
            settings_["snip"]["hotkey"]["snip"] = "F1";
        auto _1_2 = new ShortcutInput(JSON_QSTR(settings_["snip"]["hotkey"]["snip"]));
        _1_2->setFixedHeight(25);
        connect(_1_2, &ShortcutInput::changed, [&](const QKeySequence& ks){
            settings_["snip"]["hotkey"]["snip"] = ks.toString().toStdString();
            emit snipShortcutChanged(ks);
        });
        hotkey_layout->addWidget(_1_1, 1, 1);
        hotkey_layout->addWidget(_1_2, 1, 2);

        auto _2_1 = new QLabel("贴图");
        if(settings_["snip"]["hotkey"]["pin"].is_null())
            settings_["snip"]["hotkey"]["pin"] = "F3";
        auto _2_2 = new ShortcutInput(JSON_QSTR(settings_["snip"]["hotkey"]["pin"]));
        _2_2->setFixedHeight(20);
        connect(_2_2, &ShortcutInput::changed, [&](const QKeySequence& ks){
            settings_["snip"]["hotkey"]["pin"] = ks.toString().toStdString();
            emit pinImgShortcutChanged(ks);
        });
        hotkey_layout->addWidget(_2_1, 2, 1);
        hotkey_layout->addWidget(_2_2, 2, 2);

        hotkey_layout->setRowStretch(5, 1);
        hotkey->setLayout(hotkey_layout);
    }

    //
    auto layout = new QVBoxLayout();
    layout->addWidget(tabwidget);
    snip_->setLayout(layout);
}

void SettingWindow::setupRecordWidget()
{
    auto tabwidget = new QTabWidget(record_);

    auto params = new QWidget(record_);
    auto theme = new QWidget(record_);
    auto hotkey = new QWidget(record_);
    auto behavior = new QWidget(record_);

    tabwidget->addTab(params, "参数");
    tabwidget->addTab(theme, "外观");
    tabwidget->addTab(hotkey, "快捷键");
    tabwidget->addTab(behavior, "行为");

    // params
    {
        auto params_layout = new QGridLayout();

        auto _1_1 = new QLabel("framerate");
        auto _1_2 = new QSpinBox();
        _1_2->setFixedHeight(25);
        if(settings_["record"]["params"]["framerate"].is_null())
            settings_["record"]["params"]["framerate"] = 30;
        _1_2->setValue(settings_["record"]["params"]["framerate"].get<int>());
        connect(_1_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [&](int w){
            emit recordFramerateChanged(w);
            settings_["record"]["params"]["framerate"] = w;
        });
        params_layout->addWidget(_1_1, 1, 1);
        params_layout->addWidget(_1_2, 1, 2);

        params_layout->setRowStretch(2, 1);

        params->setLayout(params_layout);
    }

    // theme
    {
        auto theme_layout = new QGridLayout();
        auto _1_1 = new QLabel("边框宽度");
        auto _1_2 = new QSpinBox();
        _1_2->setFixedHeight(25);
        if(settings_["record"]["selector"]["border"]["width"].is_null())
            settings_["record"]["selector"]["border"]["width"] = 1;
        _1_2->setValue(settings_["record"]["selector"]["border"]["width"].get<int>());
        connect(_1_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [&](int w){
            emit recordBorderWidthChanged(w);
            settings_["record"]["selector"]["border"]["width"] = w;
        });
        theme_layout->addWidget(_1_1, 1, 1);
        theme_layout->addWidget(_1_2, 1, 2);

        auto _2_1 = new QLabel("边框颜色");
        auto _2_2 = new ColorButton();
        _2_2->setFixedHeight(25);
        if(settings_["record"]["selector"]["border"]["color"].is_null())
            settings_["record"]["selector"]["border"]["color"] = QColor(Qt::cyan).name().toStdString();
        _2_2->color(JSON_QSTR(settings_["record"]["selector"]["border"]["color"]));
        connect(_2_2, &ColorButton::changed, [&](const QColor& color){
            emit recordBorderColorChanged(color);
            settings_["record"]["selector"]["border"]["color"] = color.name().toStdString();
        });
        theme_layout->addWidget(_2_2, 2, 2);
        theme_layout->addWidget(_2_1, 2, 1);

        auto _3_1 = new QLabel("边框线条");
        auto _3_2 = new QComboBox();
        _3_2->setFixedHeight(25);
        _3_2->addItem("NoPen");
        _3_2->addItem("SolidLine");
        _3_2->addItem("DashLine");
        _3_2->addItem("DotLine");
        _3_2->addItem("DashDotLine");
        _3_2->addItem("DashDotDotLine");
        _3_2->addItem("CustomDashLine");
        if(settings_["record"]["selector"]["border"]["style"].is_null())
            settings_["record"]["selector"]["border"]["style"] = (int)Qt::DashDotLine;
        _3_2->setCurrentIndex(settings_["record"]["selector"]["border"]["style"].get<int>());
        connect(_3_2,  static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [&](int s){
            emit recordBorderStyleChanged(Qt::PenStyle(s));
            settings_["record"]["selector"]["border"]["style"] = s;
        });
        theme_layout->addWidget(_3_2, 3, 2);
        theme_layout->addWidget(_3_1, 3, 1);

        auto _4_1 = new QLabel("遮罩颜色");
        auto _4_2 = new ColorButton();
        _4_2->setFixedHeight(25);
        if(settings_["record"]["selector"]["mask"]["color"].is_null())
            settings_["record"]["selector"]["mask"]["color"] = QColor(0, 0, 0, 100).name(QColor::HexArgb).toStdString();
        _4_2->color(JSON_QSTR(settings_["record"]["selector"]["mask"]["color"]));
        connect(_4_2, &ColorButton::changed, [&](const QColor& color){
            emit recordMaskColorChanged(color);
            settings_["record"]["selector"]["mask"]["color"] = color.name().toStdString();
        });
        theme_layout->addWidget(_4_2, 4, 2);
        theme_layout->addWidget(_4_1, 4, 1);

        theme_layout->setRowStretch(5, 1);
        theme->setLayout(theme_layout);
    }

    // behavier
    {
        auto behavier_layout = new QGridLayout();
        auto _01 = new QCheckBox("开启窗口探测");
        if(settings_["record"]["detectwindow"].is_null()) settings_["record"]["detectwindow"] = true;
        _01->setChecked(settings_["record"]["detectwindow"].get<bool>());
        connect(_01, &QCheckBox::stateChanged, [this](int state){
            emit recordDetectWindowChanged(state == Qt::Checked);
            settings_["record"]["detectwindow"] = (state == Qt::Checked);
        });

        behavier_layout->addWidget(_01);
        behavier_layout->setRowStretch(2, 1);
        behavior->setLayout(behavier_layout);
    }

    // hotkey
    {
        auto hotkey_layout = new QGridLayout();

        auto _1_1 = new QLabel("开始/结束");
        if(settings_["record"]["hotkey"]["record"].is_null())
            settings_["record"]["hotkey"]["record"] = "Ctrl+Alt+V";
        auto _1_2 = new ShortcutInput(JSON_QSTR(settings_["record"]["hotkey"]["record"]));
        _1_2->setFixedHeight(25);
        connect(_1_2, &ShortcutInput::changed, [&](const QKeySequence& ks){
            settings_["record"]["hotkey"]["record"] = ks.toString().toStdString();
            emit videoShortcutChanged(ks);
        });
        hotkey_layout->addWidget(_1_1, 1, 1);
        hotkey_layout->addWidget(_1_2, 1, 2);
        hotkey_layout->setRowStretch(5, 1);

        hotkey->setLayout(hotkey_layout);
    }

    //
    auto layout = new QVBoxLayout();
    layout->addWidget(tabwidget);
    record_->setLayout(layout);
}

void SettingWindow::setupGIFWidget()
{
    auto tabwidget = new QTabWidget(gif_);

    auto params = new QWidget(gif_);
    auto theme = new QWidget(gif_);
    auto hotkey = new QWidget(gif_);
    auto behavior = new QWidget(gif_);


    tabwidget->addTab(params, "参数");
    tabwidget->addTab(theme, "外观");
    tabwidget->addTab(hotkey, "快捷键");
    tabwidget->addTab(behavior, "行为");

    // params
    {
        auto params_layout = new QGridLayout();

        auto _1_1 = new QLabel("fps");
        auto _1_2 = new QSpinBox();
        _1_2->setFixedHeight(25);
        if(settings_["gif"]["params"]["fps"].is_null())
            settings_["gif"]["params"]["fps"] = 6;
        _1_2->setValue(settings_["gif"]["params"]["fps"].get<int>());
        connect(_1_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [&](int w){
            emit gifFPSChanged(w);
            settings_["gif"]["params"]["fps"] = w;
        });
        params_layout->addWidget(_1_1, 1, 1);
        params_layout->addWidget(_1_2, 1, 2);

        params_layout->setRowStretch(2, 1);

        params->setLayout(params_layout);
    }

    // theme
    {
        auto theme_layout = new QGridLayout();

        auto _1_1 = new QLabel("边框宽度");
        auto _1_2 = new QSpinBox();
        _1_2->setFixedHeight(25);
        if(settings_["gif"]["selector"]["border"]["width"].is_null())
            settings_["gif"]["selector"]["border"]["width"] = 1;
        _1_2->setValue(settings_["gif"]["selector"]["border"]["width"].get<int>());
        connect(_1_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [&](int w){
            emit gifBorderWidthChanged(w);
            settings_["gif"]["selector"]["border"]["width"] = w;
        });
        theme_layout->addWidget(_1_1, 1, 1);
        theme_layout->addWidget(_1_2, 1, 2);

        auto _2_1 = new QLabel("边框颜色");
        auto _2_2 = new ColorButton();
        _2_2->setFixedHeight(25);
        if(settings_["gif"]["selector"]["border"]["color"].is_null())
            settings_["gif"]["selector"]["border"]["color"] = QColor(Qt::cyan).name().toStdString();
        _2_2->color(JSON_QSTR(settings_["gif"]["selector"]["border"]["color"]));
        connect(_2_2, &ColorButton::changed, [&](const QColor& color){
            emit gifBorderColorChanged(color);
            settings_["gif"]["selector"]["border"]["color"] = color.name().toStdString();
        });
        theme_layout->addWidget(_2_2, 2, 2);
        theme_layout->addWidget(_2_1, 2, 1);

        auto _3_1 = new QLabel("边框线条");
        auto _3_2 = new QComboBox();
        _3_2->setFixedHeight(25);
        _3_2->addItem("NoPen");
        _3_2->addItem("SolidLine");
        _3_2->addItem("DashLine");
        _3_2->addItem("DotLine");
        _3_2->addItem("DashDotLine");
        _3_2->addItem("DashDotDotLine");
        _3_2->addItem("CustomDashLine");
        if(settings_["gif"]["selector"]["border"]["style"].is_null())
            settings_["gif"]["selector"]["border"]["style"] = (int)Qt::DashDotLine;
        _3_2->setCurrentIndex(settings_["gif"]["selector"]["border"]["style"].get<int>());
        connect(_3_2,  static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [&](int s){
            emit gifBorderStyleChanged(Qt::PenStyle(s));
            settings_["gif"]["selector"]["border"]["style"] = s;
        });
        theme_layout->addWidget(_3_2, 3, 2);
        theme_layout->addWidget(_3_1, 3, 1);

        auto _4_1 = new QLabel("遮罩颜色");
        auto _4_2 = new ColorButton();
        _4_2->setFixedHeight(25);
        if(settings_["gif"]["selector"]["mask"]["color"].is_null())
            settings_["gif"]["selector"]["mask"]["color"] = QColor(0, 0, 0, 100).name(QColor::HexArgb).toStdString();
        _4_2->color(JSON_QSTR(settings_["gif"]["selector"]["mask"]["color"]));
        connect(_4_2, &ColorButton::changed, [&](const QColor& color){
            emit gifMaskColorChanged(color);
            settings_["gif"]["selector"]["mask"]["color"] = color.name().toStdString();
        });
        theme_layout->addWidget(_4_2, 4, 2);
        theme_layout->addWidget(_4_1, 4, 1);

        theme_layout->setRowStretch(5, 1);
        theme->setLayout(theme_layout);
    }

    // behavier
    {
        auto behavier_layout = new QGridLayout();

        auto _01 = new QCheckBox("开启窗口探测");
        if(settings_["gif"]["detectwindow"].is_null()) settings_["gif"]["detectwindow"] = true;
        _01->setChecked(settings_["gif"]["detectwindow"].get<bool>());
        connect(_01, &QCheckBox::stateChanged, [this](int state){
            emit gifDetectWindowChanged(state == Qt::Checked);
            settings_["gif"]["detectwindow"] = (state == Qt::Checked);
        });

        behavier_layout->addWidget(_01);
        behavier_layout->setRowStretch(2, 1);

        behavior->setLayout(behavier_layout);
    }

    // hotkey
    {
        auto hotkey_layout = new QGridLayout();

        auto _1_1 = new QLabel("开始/结束");
        if(settings_["gif"]["hotkey"]["record"].is_null())
            settings_["gif"]["hotkey"]["record"] = "Ctrl+Alt+G";
        auto _1_2 = new ShortcutInput(JSON_QSTR(settings_["gif"]["hotkey"]["record"]));
        _1_2->setFixedHeight(25);
        connect(_1_2, &ShortcutInput::changed, [&](const QKeySequence& ks){
            settings_["gif"]["hotkey"]["record"] = ks.toString().toStdString();
            emit gifShortcutChanged(ks);
        });
        hotkey_layout->addWidget(_1_1, 1, 1);
        hotkey_layout->addWidget(_1_2, 1, 2);

        hotkey_layout->setRowStretch(5, 1);

        hotkey->setLayout(hotkey_layout);
    }

    //
    auto layout = new QVBoxLayout();
    layout->addWidget(tabwidget);
    gif_->setLayout(layout);
}

void SettingWindow::setupAboutWidget()
{
    new QLabel("Copyright (C) 2018 Zhang Liangqi", about_);
}

