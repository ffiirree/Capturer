#include "settingdialog.h"
#include <QDebug>
#include "shortcutinput.h"
#include <string>
#include <QStandardPaths>
#include <QSpinBox>
#include <QComboBox>
#include <QVBoxLayout>
#include <QGraphicsDropShadowEffect>
#include "colorpanel.h"
#include "apptabcontrol.h"
#include "titlebar.h"


SettingWindow::SettingWindow(QWidget * parent)
    : QWidget(parent)
{
    LOAD_QSS(this, ":/qss/setting/settingswindow.qss");

    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);

    setMinimumSize(850, 600);

    setContentsMargins(25, 25, 25, 25);

    auto shadow_layout = new QVBoxLayout();
    shadow_layout->setSpacing(0);
    shadow_layout->setMargin(0);
    auto window = new QWidget();
    window->setObjectName("settingswindow");
    shadow_layout->addWidget(window);
    setLayout(shadow_layout);


    auto effect = new QGraphicsDropShadowEffect(this);
    effect->setBlurRadius(20);
    effect->setOffset(0);
    effect->setColor(QColor(0, 0, 0, 175));
    window->setGraphicsEffect(effect);

    auto layout = new QVBoxLayout();
    layout->setSpacing(0);
    layout->setMargin(0);
    window->setLayout(layout);

    auto titlebar = new TitleBar();
    titlebar->setFixedHeight(50);
    titlebar->setTitle(tr("Settings"));
    connect(titlebar, &TitleBar::close, this, &QWidget::close);
    connect(titlebar, &TitleBar::moved, this, [this](const QPoint& m) {
        this->move(this->pos() + m);
    });
    layout->addWidget(titlebar);

    auto tabwidget = new AppTabControl(45, 225);
    tabwidget->setObjectName("firstmenu");
    tabwidget->tabBar()->setObjectName("fristtab");
    layout->addWidget(tabwidget);

    general_ = new QWidget();
    snip_ = new QWidget();
    record_ = new QWidget();
    gif_ = new QWidget();
    about_ = new QWidget();

    tabwidget->addTab(general_, tr("General"));
    tabwidget->addTab(snip_, tr("Snip"));
    tabwidget->addTab(record_, tr("Screen Record"));
    tabwidget->addTab(gif_, tr("Gif Record"));
    tabwidget->addTab(about_, tr("About..."));

    setupGeneralWidget();
    setupSnipWidget();
    setupRecordWidget();
    setupGIFWidget();
    setupAboutWidget();
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
    config.set(config["autorun"], statue == Qt::Checked);
}

void SettingWindow::setupGeneralWidget()
{
    auto layout = new QGridLayout();
    layout->setContentsMargins(35, 15, 35, 15);

    //
    auto _01 = new QCheckBox(tr("Run on startup"));
    _01->setChecked(config["autorun"].get<bool>());
    setAutoRun(_01->checkState());
    connect(_01, &QCheckBox::stateChanged, this, &SettingWindow::setAutoRun);

    layout->addWidget(_01, 0, 0, 1, 2);

    //
    auto _1_2 = new QComboBox();
    _1_2->addItem("English");
    _1_2->addItem("简体中文");
    auto language = config["language"].get<QString>();
    if (language == "en_US") {
        _1_2->setCurrentIndex(0);
    }
    else if (language == "zh_CN") {
        _1_2->setCurrentIndex(1);
    }
    connect(_1_2, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this](int i){
        config.set(config["languaue"], i ? "zh_CN" : "en_US");
    });
    layout->addWidget(new QLabel(tr("Language")), 1, 0, 1, 1);
    layout->addWidget(_1_2, 1, 1, 1, 2);

    //
    auto _2_2 = new QLineEdit(config.getFilePath());
    _2_2->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(new QLabel(tr("Settings file")), 2, 0, 1, 1);
    layout->addWidget(_2_2, 2, 1, 1, 2);


    layout->setRowStretch(3, 1);

    general_->setLayout(layout);
}

void SettingWindow::setupSnipWidget()
{
    auto tabwidget = new QTabWidget(snip_);
    tabwidget->setObjectName("subtw");
    tabwidget->tabBar()->setObjectName("subtwtab");

    auto theme = new QWidget(snip_);
    auto hotkey = new QWidget(snip_);
    auto pin = new QWidget(snip_);

    tabwidget->addTab(theme, tr("Appearance"));
    tabwidget->addTab(hotkey, tr("Hotkey"));
    tabwidget->addTab(pin, tr("Paste"));

    // theme
    {
        auto layout = new QGridLayout();
        layout->setContentsMargins(35, 15, 35, 15);

        auto _1_2 = new QSpinBox();
        _1_2->setValue(config["snip"]["selector"]["border"]["width"].get<int>());
        connect(_1_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int w){
            config.set(config["snip"]["selector"]["border"]["width"], w);
        });
        layout->addWidget(new QLabel(tr("Border width")), 1, 1, 1, 1);
        layout->addWidget(_1_2, 1, 2, 1, 2);

        auto _2_2 = new ColorDialogButton(config["snip"]["selector"]["border"]["color"].get<QColor>());
        connect(_2_2, &ColorDialogButton::changed, [this](auto&& c) { config.set(config["snip"]["selector"]["border"]["color"], c); });
        layout->addWidget(new QLabel(tr("Border color")), 2, 1, 1, 1);
        layout->addWidget(_2_2, 2, 2, 1, 2);

        auto _3_2 = new QComboBox();
        _3_2->addItems({ "NoPen", "SolidLine", "DashLine", "DotLine", "DashDotLine", "DashDotDotLine", "CustomDashLine" });
        _3_2->setCurrentIndex(config["snip"]["selector"]["border"]["style"].get<Qt::PenStyle>());
        connect(_3_2, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [this](int s){
            config.set(config["snip"]["selector"]["border"]["style"], s);
        });
        layout->addWidget(new QLabel(tr("Line type")), 3, 1, 1, 1);
        layout->addWidget(_3_2, 3, 2, 1, 2);

        auto _4_2 = new ColorDialogButton(config["snip"]["selector"]["mask"]["color"].get<QColor>());
        connect(_4_2, &ColorDialogButton::changed, [this](auto&& c){ config.set(config["snip"]["selector"]["mask"]["color"], c); });
        layout->addWidget(new QLabel(tr("Mask color")), 4, 1, 1, 1);
        layout->addWidget(_4_2, 4, 2, 1, 2);

        layout->setRowStretch(5, 1);
        theme->setLayout(layout);
    }

    // hotkey
    {
        auto layout = new QGridLayout();
        layout->setContentsMargins(35, 15, 35, 15);

        auto _1_2 = new ShortcutInput(config["snip"]["hotkey"].get<QString>());
        connect(_1_2, &ShortcutInput::changed, [this](auto&& ks){ config.set(config["snip"]["hotkey"], ks); });
        layout->addWidget(new QLabel(tr("Snip")), 1, 1, 1, 1);
        layout->addWidget(_1_2, 1, 2, 1, 3);

        auto _2_2 = new ShortcutInput(config["pin"]["hotkey"].get<QKeySequence>());
        connect(_2_2, &ShortcutInput::changed, [this](auto&& ks){ config.set(config["pin"]["hotkey"], ks); });
        layout->addWidget(new QLabel(tr("Paste")), 2, 1, 1, 1);
        layout->addWidget(_2_2, 2, 2, 1, 3);

        layout->setRowStretch(5, 1);
        hotkey->setLayout(layout);
    }

    //
    auto layout = new QVBoxLayout();
    layout->addWidget(tabwidget);
    snip_->setLayout(layout);
}

void SettingWindow::setupRecordWidget()
{
    auto tabwidget = new QTabWidget(record_);
    tabwidget->setObjectName("subtw");
    tabwidget->tabBar()->setObjectName("subtwtab");

    auto params = new QWidget(record_);
    auto theme = new QWidget(record_);
    auto hotkey = new QWidget(record_);

    tabwidget->addTab(params, tr("Params"));
    tabwidget->addTab(theme, tr("Appearance"));
    tabwidget->addTab(hotkey, tr("Hotkey"));

    // params
    {
        auto layout = new QGridLayout();
        layout->setContentsMargins(35, 15, 35, 15);

        auto _1_1 = new QLabel(tr("Framerate"));
        auto _1_2 = new QSpinBox();
        _1_2->setValue(config["record"]["framerate"].get<int>());
        connect(_1_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int w){
            config.set(config["record"]["framerate"], w);
        });
        layout->addWidget(_1_1, 1, 1, 1, 1);
        layout->addWidget(_1_2, 1, 2, 1, 2);

        layout->setRowStretch(2, 1);

        params->setLayout(layout);
    }

    // theme
    {
        auto layout = new QGridLayout();
        layout->setContentsMargins(35, 15, 35, 15);

        auto _1_1 = new QLabel(tr("Border width"));
        auto _1_2 = new QSpinBox();
        _1_2->setValue(config["record"]["selector"]["border"]["width"].get<int>());
        connect(_1_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int w){
            config.set(config["record"]["selector"]["border"]["width"], w);
        });
        layout->addWidget(_1_1, 1, 1, 1, 1);
        layout->addWidget(_1_2, 1, 2, 1, 2);

        auto _2_1 = new QLabel(tr("Border color"));
        auto _2_2 = new ColorDialogButton(config["record"]["selector"]["border"]["color"].get<QColor>());
        connect(_2_2, &ColorDialogButton::changed, [this](auto&& c){ config.set(config["record"]["selector"]["border"]["color"], c); });
        layout->addWidget(_2_1, 2, 1, 1, 1);
        layout->addWidget(_2_2, 2, 2, 1, 2);

        auto _3_1 = new QLabel(tr("Line type"));
        auto _3_2 = new QComboBox();
        _3_2->addItems({ "NoPen", "SolidLine", "DashLine", "DotLine", "DashDotLine", "DashDotDotLine", "CustomDashLine" });
        _3_2->setCurrentIndex(config["record"]["selector"]["border"]["style"].get<int>());
        connect(_3_2,  static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [this](int s){
            config.set(config["record"]["selector"]["border"]["style"], s);
        });
        layout->addWidget(_3_2, 3, 2, 1, 2);
        layout->addWidget(_3_1, 3, 1, 1, 1);

        auto _4_1 = new QLabel(tr("Mask color"));
        auto _4_2 = new ColorDialogButton(config["record"]["selector"]["mask"]["color"].get<QColor>());
        connect(_4_2, &ColorDialogButton::changed, [this](auto&& c){ config.set(config["record"]["selector"]["mask"]["color"], c); });
        layout->addWidget(_4_2, 4, 2, 1, 2);
        layout->addWidget(_4_1, 4, 1, 1, 1);

        layout->setRowStretch(5, 1);
        theme->setLayout(layout);
    }

    // hotkey
    {
        auto layout = new QGridLayout();
        layout->setContentsMargins(35, 15, 35, 15);

        auto _1_1 = new QLabel(tr("Begin/Finish"));
        auto _1_2 = new ShortcutInput(config["record"]["hotkey"].get<QKeySequence>());
        connect(_1_2, &ShortcutInput::changed, [this](auto&& ks){ config.set(config["record"]["hotkey"], ks); });
        layout->addWidget(_1_1, 1, 1, 1, 1);
        layout->addWidget(_1_2, 1, 2, 1, 3);

        layout->setRowStretch(5, 1);
        hotkey->setLayout(layout);
    }

    //
    auto layout = new QVBoxLayout();
    layout->addWidget(tabwidget);
    record_->setLayout(layout);
}

void SettingWindow::setupGIFWidget()
{
    auto tabwidget = new QTabWidget(gif_);
    tabwidget->setObjectName("subtw");
    tabwidget->tabBar()->setObjectName("subtwtab");

    auto params = new QWidget(gif_);
    auto theme = new QWidget(gif_);
    auto hotkey = new QWidget(gif_);

    tabwidget->addTab(params, tr("Params"));
    tabwidget->addTab(theme, tr("Appearance"));
    tabwidget->addTab(hotkey, tr("Hotkey"));

    // params
    {
        auto layout = new QGridLayout();
        layout->setContentsMargins(35, 15, 35, 15);

        auto _1_1 = new QLabel(tr("FPS"));
        auto _1_2 = new QSpinBox();
        _1_2->setValue(config["gif"]["framerate"].get<int>());
        connect(_1_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int w){
            config.set(config["gif"]["framerate"], w);
        });
        layout->addWidget(_1_1, 1, 1, 1, 1);
        layout->addWidget(_1_2, 1, 2, 1, 2);

        layout->setRowStretch(2, 1);

        params->setLayout(layout);
    }

    // theme
    {
        auto layout = new QGridLayout();
        layout->setContentsMargins(35, 15, 35, 15);

        auto _1_1 = new QLabel(tr("Border width"));
        auto _1_2 = new QSpinBox();
        _1_2->setValue(config["gif"]["selector"]["border"]["width"].get<int>());
        connect(_1_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int w){
            config.set(config["gif"]["selector"]["border"]["width"], w);
        });
        layout->addWidget(_1_1, 1, 1, 1, 1);
        layout->addWidget(_1_2, 1, 2, 1, 2);

        auto _2_1 = new QLabel(tr("Border color"));
        auto _2_2 = new ColorDialogButton(config["gif"]["selector"]["border"]["color"].get<QColor>());
        connect(_2_2, &ColorDialogButton::changed, [this](auto&& c){ config.set(config["gif"]["selector"]["border"]["color"], c); });
        layout->addWidget(_2_2, 2, 2, 1, 2);
        layout->addWidget(_2_1, 2, 1, 1, 1);

        auto _3_1 = new QLabel(tr("Line type"));
        auto _3_2 = new QComboBox();
        _3_2->addItems({ "NoPen", "SolidLine", "DashLine", "DotLine", "DashDotLine", "DashDotDotLine", "CustomDashLine" });
        _3_2->setCurrentIndex(config["gif"]["selector"]["border"]["style"].get<int>());
        connect(_3_2,  static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [this](int s){
            config.set(config["gif"]["selector"]["border"]["style"], s);
        });
        layout->addWidget(_3_2, 3, 2, 1, 2);
        layout->addWidget(_3_1, 3, 1, 1, 1);

        auto _4_1 = new QLabel(tr("Mask color"));
        auto _4_2 = new ColorDialogButton(config["gif"]["selector"]["mask"]["color"].get<QColor>());
        connect(_4_2, &ColorDialogButton::changed, [this](auto&& c){ config.set(config["gif"]["selector"]["mask"]["color"], c); });
        layout->addWidget(_4_2, 4, 2, 1, 2);
        layout->addWidget(_4_1, 4, 1, 1, 1);

        layout->setRowStretch(5, 1);
        theme->setLayout(layout);
    }

    // hotkey
    {
        auto layout = new QGridLayout();
        layout->setContentsMargins(35, 15, 35, 15);

        auto _1_1 = new QLabel(tr("Begin/Finish"));
        auto _1_2 = new ShortcutInput(config["gif"]["hotkey"].get<QKeySequence>());
        _1_2->setFixedHeight(25);
        connect(_1_2, &ShortcutInput::changed, [this](auto&& ks){
            config.set(config["gif"]["hotkey"], ks);
        });
        layout->addWidget(_1_1, 1, 1, 1, 1);
        layout->addWidget(_1_2, 1, 2, 1, 3);

        layout->setRowStretch(5, 1);

        hotkey->setLayout(layout);
    }

    //
    auto layout = new QVBoxLayout();
    layout->addWidget(tabwidget);
    gif_->setLayout(layout);
}

void SettingWindow::setupAboutWidget()
{
    auto layout = new QVBoxLayout();
    layout->setContentsMargins(35, 15, 35, 15);

    about_->setLayout(layout);
    layout->addWidget(new QLabel(tr("Copyright © 2018 - 2019 ffiirree")));
    layout->addStretch();
}

