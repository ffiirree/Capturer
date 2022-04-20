#include "settingdialog.h"
#include "shortcutinput.h"
#include <string>
#include <QStandardPaths>
#include <QSpinBox>
#include <QComboBox>
#include <QListView>
#include <QVBoxLayout>
#include <QGraphicsDropShadowEffect>
#include "colorpanel.h"
#include "titlebar.h"
#include "logging.h"
#include "version.h"

SettingWindow::SettingWindow(QWidget * parent)
    : QWidget(parent)
{
    LOAD_QSS(this, ":/qss/setting/settingswindow.qss");

    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);

    setFixedSize(850, 600);

    setContentsMargins(20, 20, 20, 20);

    auto shadow_layout = new QVBoxLayout();
    shadow_layout->setSpacing(0);
    shadow_layout->setMargin(0);
    auto window = new QWidget();
    window->setObjectName("settings-mainwindow");
    shadow_layout->addWidget(window);
    setLayout(shadow_layout);

    // shadow
    auto effect = new QGraphicsDropShadowEffect(this);
    effect->setBlurRadius(20);
    effect->setOffset(0);
    effect->setColor(QColor(0, 0, 0, 50));
    window->setGraphicsEffect(effect);

    auto layout = new QVBoxLayout();
    layout->setSpacing(0);
    layout->setMargin(0);
    window->setLayout(layout);

    // title bar
    auto titlebar = new TitleBar();
    titlebar->setTitle(tr("Settings"));
    connect(titlebar, &TitleBar::close, this, &QWidget::close);
    connect(titlebar, &TitleBar::moved, this, [this](const QPoint& m) {
        this->move(this->pos() + m);
    });
    layout->addWidget(titlebar);

    tabwidget_ = new AppTabControl(45, 225);
    tabwidget_->setObjectName("firstmenu");
    tabwidget_->tabBar()->setObjectName("fristtab");
    layout->addWidget(tabwidget_);

    setupGeneralWidget();
    setupSnipWidget();
    setupRecordWidget();
    setupGIFWidget();
    setupHotkeyWidget();
    setupAboutWidget();
}

void SettingWindow::setupGeneralWidget()
{
    auto index = tabwidget_->addTab(new QWidget(), tr("General"));

    auto layout = new QGridLayout();
    layout->setContentsMargins(35, 10, 35, 15);

    //
    auto _01 = new QCheckBox(tr("Run on startup"));
    _01->setChecked(config["autorun"].get<bool>());
    setAutoRun(_01->checkState());
    connect(_01, &QCheckBox::stateChanged, this, &SettingWindow::setAutoRun);
    layout->addWidget(_01, 0, 0, 1, 2);

    //
    auto _1_2 = new QComboBox();
    _1_2->setView(new QListView());
    _1_2->view()->window()->setWindowFlag(Qt::NoDropShadowWindowHint);
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
        config.set(config["language"], i ? "zh_CN" : "en_US");
    });
    layout->addWidget(new QLabel(tr("Language")), 1, 0, 1, 1);
    layout->addWidget(_1_2, 1, 1, 1, 2);

    //
    auto _2_2 = new QLineEdit(config.getFilePath());
    _2_2->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(new QLabel(tr("Settings file")), 2, 0, 1, 1);
    layout->addWidget(_2_2, 2, 1, 1, 2);

    layout->setRowStretch(3, 1);
    tabwidget_->widget(index)->setLayout(layout);
}

void SettingWindow::setupSnipWidget()
{
    auto index = tabwidget_->addTab(new QWidget(), tr("Screenshot"));

    auto layout = new QGridLayout();
    layout->setContentsMargins(35, 10, 35, 15);

    auto _0 = new QLabel(tr("Apperance:"));
    _0->setObjectName("sub-title");
    layout->addWidget(_0, 0, 1, 1, 1);

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
    _3_2->setView(new QListView());
    _3_2->view()->window()->setWindowFlag(Qt::NoDropShadowWindowHint);
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
    tabwidget_->widget(index)->setLayout(layout);
}

void SettingWindow::setupRecordWidget()
{
    auto index = tabwidget_->addTab(new QWidget(), tr("Screen recording"));

    auto layout = new QGridLayout();
    layout->setContentsMargins(35, 10, 35, 15);

    auto _0 = new QLabel(tr("Apperance:"));
    _0->setObjectName("sub-title");
    layout->addWidget(_0, 0, 1, 1, 1);

    auto _1_2 = new QSpinBox();
    _1_2->setValue(config["record"]["selector"]["border"]["width"].get<int>());
    connect(_1_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int w){
        config.set(config["record"]["selector"]["border"]["width"], w);
    });
    layout->addWidget(new QLabel(tr("Border width")), 1, 1, 1, 1);
    layout->addWidget(_1_2, 1, 2, 1, 2);

    auto _2_2 = new ColorDialogButton(config["record"]["selector"]["border"]["color"].get<QColor>());
    connect(_2_2, &ColorDialogButton::changed, [this](auto&& c){ config.set(config["record"]["selector"]["border"]["color"], c); });
    layout->addWidget(new QLabel(tr("Border color")), 2, 1, 1, 1);
    layout->addWidget(_2_2, 2, 2, 1, 2);

    auto _3_2 = new QComboBox();
    _3_2->setView(new QListView());
    _3_2->view()->window()->setWindowFlag(Qt::NoDropShadowWindowHint);
    _3_2->addItems({ "NoPen", "SolidLine", "DashLine", "DotLine", "DashDotLine", "DashDotDotLine", "CustomDashLine" });
    _3_2->setCurrentIndex(config["record"]["selector"]["border"]["style"].get<int>());
    connect(_3_2,  static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [this](int s){
        config.set(config["record"]["selector"]["border"]["style"], s);
    });
    layout->addWidget(new QLabel(tr("Line type")), 3, 1, 1, 1);
    layout->addWidget(_3_2, 3, 2, 1, 2);

    auto _4_2 = new ColorDialogButton(config["record"]["selector"]["mask"]["color"].get<QColor>());
    connect(_4_2, &ColorDialogButton::changed, [this](auto&& c){ config.set(config["record"]["selector"]["mask"]["color"], c); });
    layout->addWidget(new QLabel(tr("Mask color")), 4, 1, 1, 1);
    layout->addWidget(_4_2, 4, 2, 1, 2);

    layout->addWidget(new QLabel(), 5, 1, 1, 1);

    auto _5 = new QLabel(tr("Params:"));
    _5->setObjectName("sub-title");
    layout->addWidget(_5, 6, 1, 1, 1);

    auto _6_2 = new QSpinBox();
    _6_2->setValue(config["record"]["framerate"].get<int>());
    connect(_6_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int w){
        config.set(config["record"]["framerate"], w);
    });
    layout->addWidget(new QLabel(tr("Framerate")), 7, 1, 1, 1);
    layout->addWidget(_6_2, 7, 2, 1, 2);

    layout->setRowStretch(8, 1);

    tabwidget_->widget(index)->setLayout(layout);
}

void SettingWindow::setupGIFWidget()
{
    auto index = tabwidget_->addTab(new QWidget(), tr("Gif recording"));

    auto layout = new QGridLayout();
    layout->setContentsMargins(35, 10, 35, 15);

    auto _0 = new QLabel(tr("Apperance:"));
    _0->setObjectName("sub-title");
    layout->addWidget(_0, 0, 1, 1, 1);

    auto _1_2 = new QSpinBox();
    _1_2->setValue(config["gif"]["selector"]["border"]["width"].get<int>());
    connect(_1_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int w){
        config.set(config["gif"]["selector"]["border"]["width"], w);
    });
    layout->addWidget(new QLabel(tr("Border width")), 1, 1, 1, 1);
    layout->addWidget(_1_2, 1, 2, 1, 2);

    auto _2_2 = new ColorDialogButton(config["gif"]["selector"]["border"]["color"].get<QColor>());
    connect(_2_2, &ColorDialogButton::changed, [this](auto&& c){ config.set(config["gif"]["selector"]["border"]["color"], c); });
    layout->addWidget(_2_2, 2, 2, 1, 2);
    layout->addWidget(new QLabel(tr("Border color")), 2, 1, 1, 1);

    auto _3_2 = new QComboBox();
    _3_2->setView(new QListView());
    _3_2->view()->window()->setWindowFlag(Qt::NoDropShadowWindowHint);
    _3_2->addItems({ "NoPen", "SolidLine", "DashLine", "DotLine", "DashDotLine", "DashDotDotLine", "CustomDashLine" });
    _3_2->setCurrentIndex(config["gif"]["selector"]["border"]["style"].get<int>());
    connect(_3_2,  static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [this](int s){
        config.set(config["gif"]["selector"]["border"]["style"], s);
    });
    layout->addWidget(_3_2, 3, 2, 1, 2);
    layout->addWidget(new QLabel(tr("Line type")), 3, 1, 1, 1);

    auto _4_2 = new ColorDialogButton(config["gif"]["selector"]["mask"]["color"].get<QColor>());
    connect(_4_2, &ColorDialogButton::changed, [this](auto&& c){ config.set(config["gif"]["selector"]["mask"]["color"], c); });
    layout->addWidget(_4_2, 4, 2, 1, 2);
    layout->addWidget(new QLabel(tr("Mask color")), 4, 1, 1, 1);

    layout->addWidget(new QLabel(), 5, 1, 1, 1);

    auto _5 = new QLabel(tr("Params:"));
    _5->setObjectName("sub-title");
    layout->addWidget(_5, 6, 1, 1, 1);

    auto _7_2 = new QSpinBox();
    _7_2->setValue(config["gif"]["framerate"].get<int>());
    connect(_7_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int w){
        config.set(config["gif"]["framerate"], w);
    });
    layout->addWidget(new QLabel(tr("Framerate")), 7, 1, 1, 1);
    layout->addWidget(_7_2, 7, 2, 1, 2);

    layout->setRowStretch(8, 1);
    tabwidget_->widget(index)->setLayout(layout);
}


void SettingWindow::setupHotkeyWidget()
{
    auto index = tabwidget_->addTab(new QWidget(), tr("Shortcuts"));
    auto idx_row = 1;

    auto layout = new QGridLayout();
    layout->setContentsMargins(35, 10, 35, 15);
    layout->setVerticalSpacing(10);

    auto _1_2 = new ShortcutInput(config["snip"]["hotkey"].get<QKeySequence>());
    connect(_1_2, &ShortcutInput::changed, [this](auto&& ks){
        config.set(config["snip"]["hotkey"], ks);
    });
    layout->addWidget(new QLabel(tr("Screenshot")), idx_row, 1, 1, 1);
    layout->addWidget(_1_2, idx_row++, 2, 1, 3);

    auto _2_2 = new ShortcutInput(config["pin"]["hotkey"].get<QKeySequence>());
    connect(_2_2, &ShortcutInput::changed, [this](auto&& ks){
        config.set(config["pin"]["hotkey"], ks);
    });
    layout->addWidget(new QLabel(tr("Paste")), idx_row, 1, 1, 1);
    layout->addWidget(_2_2, idx_row++, 2, 1, 3);

    auto hide_show_images = new ShortcutInput(config["pin"]["visiable"]["hotkey"].get<QKeySequence>());
    connect(hide_show_images, &ShortcutInput::changed, [this](auto&& ks){
        config.set(config["pin"]["visiable"]["hotkey"], ks);
    });
    layout->addWidget(new QLabel(tr("Hide/Show all images")), idx_row, 1, 1, 1);
    layout->addWidget(hide_show_images, idx_row++, 2, 1, 3);

    auto _3_2 = new ShortcutInput(config["record"]["hotkey"].get<QKeySequence>());
    connect(_3_2, &ShortcutInput::changed, [this](auto&& ks){
        config.set(config["record"]["hotkey"], ks);
    });
    layout->addWidget(new QLabel(tr("Video recording")), idx_row, 1, 1, 1);
    layout->addWidget(_3_2, idx_row++, 2, 1, 3);

    auto _4_2 = new ShortcutInput(config["gif"]["hotkey"].get<QKeySequence>());
    connect(_4_2, &ShortcutInput::changed, [this](auto&& ks){
        config.set(config["gif"]["hotkey"], ks);
    });
    layout->addWidget(new QLabel(tr("Gif recording")), idx_row, 1, 1, 1);
    layout->addWidget(_4_2, idx_row++, 2, 1, 3);

    layout->setRowStretch(idx_row, 1);
    tabwidget_->widget(index)->setLayout(layout);
}

void SettingWindow::setupAboutWidget()
{
    auto index = tabwidget_->addTab(new QWidget(), tr("About"));

    auto parent_layout = new QVBoxLayout();
    tabwidget_->widget(index)->setLayout(parent_layout);
    parent_layout->setContentsMargins(35, 10, 35, 15);

    /////
    auto layout = new QGridLayout();
    parent_layout->addLayout(layout);

    layout->addWidget(new QLabel(tr("Version")), 0, 0, 1, 1);
    auto version = new QLineEdit(CAPTURER_VERSION);
    version->setFocusPolicy(Qt::NoFocus);
    version->setAlignment(Qt::AlignCenter);
    layout->addWidget(version, 0, 1, 1, 2);

    /////
    parent_layout->addStretch();

    auto copyright_ = new QLabel(tr("Copyright © 2018 - 2022 ffiirree. All rights reserved"));
    copyright_->setObjectName("copyright-label");
    copyright_->setAlignment(Qt::AlignCenter);
    parent_layout->addWidget(copyright_);
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
