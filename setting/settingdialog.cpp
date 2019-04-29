#include "settingdialog.h"
#include <QDebug>
#include "shortcutinput.h"
#include <string>
#include <QStandardPaths>
#include <QSpinBox>
#include <QComboBox>
#include "colorpanel.h"

SettingWindow::SettingWindow(QWidget * parent)
    : QWidget(parent)
{
    cfg_ = Config::Instance();

    setWindowFlags((windowFlags()&~Qt::WindowMinMaxButtonsHint)
                    | Qt::WindowStaysOnTopHint | Qt::Dialog);

    tabw_ = new QTabWidget(this);
    tabw_->setMinimumSize(300, 400);
    tabw_->setFixedWidth(500);
    tabw_->setMinimumHeight(100);
//    setFixedSize(300, 400);

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

void SettingWindow::setAutoRun(int statue)
{
    QString exec_path = QDir::toNativeSeparators(QCoreApplication::applicationFilePath());

#ifdef _WIN32
        QSettings settings("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", QSettings::NativeFormat);
        settings.setValue("capturer_run", statue == Qt::Checked ? exec_path : "");
#elif __linux__
    auto native_config_path = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
    auto native_autostart_path = native_config_path + QDir::separator() + "autostart";
    auto autorun_file = native_autostart_path + QDir::separator() + "Capturer.desktop";

    if(statue == Qt::Checked) {
        QFile file(autorun_file);
        if(file.open(QIODevice::ReadWrite | QIODevice::Text)) {
            QTextStream in(&file);
            in << "[Desktop Entry]\n"
               << "Name=Capturer\n"
               << "Comment=Screen capture/record/gif\n"
               << "Exec=" + exec_path + "\n"
               << "Terminal=false\n"
               << "StartupNotify=true\n"
               << "Type=Application\n"
               << "Categories=Utility;\n"
               << "Icon=capturer\n";
        }
        file.close();
    }
    else {
        QFile::remove(autorun_file);
    }

#endif
    cfg_->set(AUTORUN, statue == Qt::Checked);
}

void SettingWindow::setupGeneralWidget()
{
    auto layout = new QVBoxLayout();

    //
    auto _01 = new QCheckBox("开机自动启动");
    _01->setChecked(cfg_->get<bool>(AUTORUN));
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
        _1_2->setValue(cfg_->get<int>(SNIP_SBW));
        connect(_1_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int w){
            cfg_->set(SNIP_SBW, w);
        });
        theme_layout->addWidget(_1_1, 1, 1);
        theme_layout->addWidget(_1_2, 1, 2);

        auto _2_1 = new QLabel("边框颜色");
        auto _2_2 = new ColorDialogButton(cfg_->get<QColor>(SNIP_SBC));
        _2_2->setFixedHeight(25);
        connect(_2_2, &ColorDialogButton::changed, [this](auto&& c) { cfg_->set(SNIP_SBC, c); });
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
        _3_2->setCurrentIndex(cfg_->get<int>(SNIP_SBS));
        connect(_3_2,  static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [this](int s){
            cfg_->set(SNIP_SBS, s);
        });
        theme_layout->addWidget(_3_2, 3, 2);
        theme_layout->addWidget(_3_1, 3, 1);

        auto _4_1 = new QLabel("遮罩颜色");
        auto _4_2 = new ColorDialogButton(cfg_->get<QColor>(SNIP_SMC));
        _4_2->setFixedHeight(25);
        connect(_4_2, &ColorDialogButton::changed, [this](auto&& c){ cfg_->set(SNIP_SMC, c); });
        theme_layout->addWidget(_4_2, 4, 2);
        theme_layout->addWidget(_4_1, 4, 1);

        theme_layout->setRowStretch(5, 1);
        theme->setLayout(theme_layout);
    }

    // behavier
    {
        auto behavier_layout = new QGridLayout();

        auto _01 = new QCheckBox("开启窗口探测");
        _01->setChecked(cfg_->get<bool>(SNIP_DW));
        connect(_01, &QCheckBox::stateChanged, [this](int state){ cfg_->set(SNIP_DW, state == Qt::Checked); });

        behavier_layout->addWidget(_01);
        behavier_layout->setRowStretch(2, 1);

        behavior->setLayout(behavier_layout);
    }

    // hotkey
    {
        auto hotkey_layout = new QGridLayout();

        auto _1_1 = new QLabel("截屏");
        auto _1_2 = new ShortcutInput(cfg_->get<QKeySequence>(SNIP_HOTKEY));
        _1_2->setFixedHeight(25);
        connect(_1_2, &ShortcutInput::changed, [this](auto&& ks){ cfg_->set(SNIP_HOTKEY, ks); });
        hotkey_layout->addWidget(_1_1, 1, 1);
        hotkey_layout->addWidget(_1_2, 1, 2);

        auto _2_1 = new QLabel("贴图");
        auto _2_2 = new ShortcutInput(cfg_->get<QKeySequence>(PIN_HOTKEY));
        _2_2->setFixedHeight(20);
        connect(_2_2, &ShortcutInput::changed, [this](auto&& ks){ cfg_->set(PIN_HOTKEY, ks); });
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
        _1_2->setValue(cfg_->get<int>(RECORD_FRAMERATE));
        connect(_1_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int w){
            cfg_->set(RECORD_FRAMERATE, w);
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
        _1_2->setValue(cfg_->get<int>(RECORD_SBW));
        connect(_1_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int w){
            cfg_->set(RECORD_SBW, w);
        });
        theme_layout->addWidget(_1_1, 1, 1);
        theme_layout->addWidget(_1_2, 1, 2);

        auto _2_1 = new QLabel("边框颜色");
        auto _2_2 = new ColorDialogButton(cfg_->get<QColor>(RECORD_SBC));
        _2_2->setFixedHeight(25);
        connect(_2_2, &ColorDialogButton::changed, [this](auto&& c){ cfg_->set(RECORD_SBC, c); });
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
        _3_2->setCurrentIndex(cfg_->get<int>(RECORD_SBS));
        connect(_3_2,  static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [this](int s){
            cfg_->set(RECORD_SBS, s);
        });
        theme_layout->addWidget(_3_2, 3, 2);
        theme_layout->addWidget(_3_1, 3, 1);

        auto _4_1 = new QLabel("遮罩颜色");
        auto _4_2 = new ColorDialogButton(cfg_->get<QColor>(RECORD_SMC));
        _4_2->setFixedHeight(25);
        connect(_4_2, &ColorDialogButton::changed, [this](auto&& c){ cfg_->set(RECORD_SMC, c); });
        theme_layout->addWidget(_4_2, 4, 2);
        theme_layout->addWidget(_4_1, 4, 1);

        theme_layout->setRowStretch(5, 1);
        theme->setLayout(theme_layout);
    }

    // behavier
    {
        auto behavier_layout = new QGridLayout();
        auto _01 = new QCheckBox("开启窗口探测");
        _01->setChecked(cfg_->get<bool>(RECORD_DW));
        connect(_01, &QCheckBox::stateChanged, [this](int state){
            cfg_->set(RECORD_DW, state == Qt::Checked);
        });

        behavier_layout->addWidget(_01);
        behavier_layout->setRowStretch(2, 1);
        behavior->setLayout(behavier_layout);
    }

    // hotkey
    {
        auto hotkey_layout = new QGridLayout();

        auto _1_1 = new QLabel("开始/结束");
        auto _1_2 = new ShortcutInput(cfg_->get<QKeySequence>(RECORD_HOTKEY));
        _1_2->setFixedHeight(25);
        connect(_1_2, &ShortcutInput::changed, [this](auto&& ks){ cfg_->set(RECORD_HOTKEY, ks); });
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
        _1_2->setValue(cfg_->get<int>(GIF_FPS));
        connect(_1_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int w){
            cfg_->set(GIF_FPS, w);
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
        _1_2->setValue(cfg_->get<int>(GIF_SBW));
        connect(_1_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int w){
            cfg_->set(GIF_SBW, w);
        });
        theme_layout->addWidget(_1_1, 1, 1);
        theme_layout->addWidget(_1_2, 1, 2);

        auto _2_1 = new QLabel("边框颜色");
        auto _2_2 = new ColorDialogButton(cfg_->get<QColor>(GIF_SBC));
        _2_2->setFixedHeight(25);
        connect(_2_2, &ColorDialogButton::changed, [this](auto&& c){ cfg_->set(GIF_SBC, c); });
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
        _3_2->setCurrentIndex(cfg_->get<int>(GIF_SBS));
        connect(_3_2,  static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [this](int s){
            cfg_->set(GIF_SBS, s);
        });
        theme_layout->addWidget(_3_2, 3, 2);
        theme_layout->addWidget(_3_1, 3, 1);

        auto _4_1 = new QLabel("遮罩颜色");
        auto _4_2 = new ColorDialogButton(cfg_->get<QColor>(GIF_SMC));
        _4_2->setFixedHeight(25);
        connect(_4_2, &ColorDialogButton::changed, [this](auto&& c){ cfg_->set(GIF_SMC, c); });
        theme_layout->addWidget(_4_2, 4, 2);
        theme_layout->addWidget(_4_1, 4, 1);

        theme_layout->setRowStretch(5, 1);
        theme->setLayout(theme_layout);
    }

    // behavier
    {
        auto behavier_layout = new QGridLayout();

        auto _01 = new QCheckBox("开启窗口探测");
        _01->setChecked(cfg_->get<bool>(GIF_DW));
        connect(_01, &QCheckBox::stateChanged, [this](int state){
            cfg_->set(GIF_DW, state == Qt::Checked);
        });

        behavier_layout->addWidget(_01);
        behavier_layout->setRowStretch(2, 1);

        behavior->setLayout(behavier_layout);
    }

    // hotkey
    {
        auto hotkey_layout = new QGridLayout();

        auto _1_1 = new QLabel("开始/结束");
        auto _1_2 = new ShortcutInput(cfg_->get<QKeySequence>(GIF_HOTKEY));
        _1_2->setFixedHeight(25);
        connect(_1_2, &ShortcutInput::changed, [this](auto&& ks){
            cfg_->set(GIF_HOTKEY, ks);
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
    new QLabel("Copyright (C) 2018 - 2019 ffiirree", about_);
}

