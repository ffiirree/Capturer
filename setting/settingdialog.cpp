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
    cfg_ = Config::Instance();

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
    cfg_->set(AUTORUN, statue == Qt::Checked);
}

void SettingWindow::setupGeneralWidget()
{
    auto layout = new QGridLayout();
    layout->setContentsMargins(35, 15, 35, 15);

    //
    auto _01 = new QCheckBox(tr("Run on startup"));
    _01->setChecked(cfg_->get<bool>(AUTORUN));
    setAutoRun(_01->checkState());
    connect(_01, &QCheckBox::stateChanged, this, &SettingWindow::setAutoRun);

    layout->addWidget(_01, 0, 0, 1, 2);

    auto _1_2 = new QComboBox();
    _1_2->addItem("English");
    _1_2->addItem("简体中文");
    auto language = cfg_->get<QString>(SETTINGS["language"]);
    if (language == "en_US") {
        _1_2->setCurrentIndex(0);
    }
    else if (language == "zh_CN") {
        _1_2->setCurrentIndex(1);
    }
    connect(_1_2, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this, [this](int i){
        cfg_->set(SETTINGS["language"], i ? "zh_CN" : "en_US");
    });
    layout->addWidget(new QLabel(tr("Language")), 1, 0, 1, 1);
    layout->addWidget(_1_2, 1, 1, 1, 2);

    layout->setRowStretch(2, 1);

    general_->setLayout(layout);
}

void SettingWindow::setupSnipWidget()
{
    auto tabwidget = new QTabWidget(snip_);
    tabwidget->setObjectName("subtw");
    tabwidget->tabBar()->setObjectName("subtwtab");

    auto theme = new QWidget(snip_);
    auto hotkey = new QWidget(snip_);
    auto behavior = new QWidget(snip_);
    auto pin = new QWidget(snip_);

    tabwidget->addTab(theme, tr("Appearance"));
    tabwidget->addTab(hotkey, tr("Hotkey"));
    tabwidget->addTab(behavior, tr("Behavior"));
    tabwidget->addTab(pin, tr("Paste"));

    // theme
    {
        auto layout = new QGridLayout();
        layout->setContentsMargins(35, 15, 35, 15);

        auto _1_2 = new QSpinBox();
        _1_2->setValue(cfg_->get<int>(SNIP_SBW));
        connect(_1_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int w){
            cfg_->set(SNIP_SBW, w);
        });
        layout->addWidget(new QLabel(tr("Border width")), 1, 1, 1, 1);
        layout->addWidget(_1_2, 1, 2, 1, 2);

        auto _2_2 = new ColorDialogButton(cfg_->get<QColor>(SNIP_SBC));
        connect(_2_2, &ColorDialogButton::changed, [this](auto&& c) { cfg_->set(SNIP_SBC, c); });
        layout->addWidget(new QLabel(tr("Border color")), 2, 1, 1, 1);
        layout->addWidget(_2_2, 2, 2, 1, 2);

        auto _3_2 = new QComboBox();
        _3_2->addItems({ "NoPen", "SolidLine", "DashLine", "DotLine", "DashDotLine", "DashDotDotLine", "CustomDashLine" });
        _3_2->setCurrentIndex(cfg_->get<int>(SNIP_SBS));
        connect(_3_2, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [this](int s){
            cfg_->set(SNIP_SBS, s);
        });
        layout->addWidget(new QLabel(tr("Line type")), 3, 1, 1, 1);
        layout->addWidget(_3_2, 3, 2, 1, 2);

        auto _4_2 = new ColorDialogButton(cfg_->get<QColor>(SNIP_SMC));
        connect(_4_2, &ColorDialogButton::changed, [this](auto&& c){ cfg_->set(SNIP_SMC, c); });
        layout->addWidget(new QLabel(tr("Mask color")), 4, 1, 1, 1);
        layout->addWidget(_4_2, 4, 2, 1, 2);

        layout->setRowStretch(5, 1);
        theme->setLayout(layout);
    }

    // behavier
    {
        auto layout = new QGridLayout();
        layout->setContentsMargins(35, 15, 35, 15);

        auto _01 = new QCheckBox(tr("Auto detect windows"));
        _01->setChecked(cfg_->get<bool>(SNIP_DW));
        connect(_01, &QCheckBox::stateChanged, [this](int state){ cfg_->set(SNIP_DW, state == Qt::Checked); });

        layout->addWidget(_01);
        layout->setRowStretch(2, 1);

        behavior->setLayout(layout);
    }

    // hotkey
    {
        auto layout = new QGridLayout();
        layout->setContentsMargins(35, 15, 35, 15);

        auto _1_2 = new ShortcutInput(cfg_->get<QKeySequence>(SNIP_HOTKEY));
        connect(_1_2, &ShortcutInput::changed, [this](auto&& ks){ cfg_->set(SNIP_HOTKEY, ks); });
        layout->addWidget(new QLabel(tr("Snip")), 1, 1, 1, 1);
        layout->addWidget(_1_2, 1, 2, 1, 3);

        auto _2_2 = new ShortcutInput(cfg_->get<QKeySequence>(PIN_HOTKEY));
        connect(_2_2, &ShortcutInput::changed, [this](auto&& ks){ cfg_->set(PIN_HOTKEY, ks); });
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
    auto behavior = new QWidget(record_);

    tabwidget->addTab(params, tr("Params"));
    tabwidget->addTab(theme, tr("Appearance"));
    tabwidget->addTab(hotkey, tr("Hotkey"));
    tabwidget->addTab(behavior, tr("Behavior"));

    // params
    {
        auto layout = new QGridLayout();
        layout->setContentsMargins(35, 15, 35, 15);

        auto _1_1 = new QLabel(tr("Framerate"));
        auto _1_2 = new QSpinBox();
        _1_2->setValue(cfg_->get<int>(RECORD_FRAMERATE));
        connect(_1_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int w){
            cfg_->set(RECORD_FRAMERATE, w);
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
        _1_2->setValue(cfg_->get<int>(RECORD_SBW));
        connect(_1_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int w){
            cfg_->set(RECORD_SBW, w);
        });
        layout->addWidget(_1_1, 1, 1, 1, 1);
        layout->addWidget(_1_2, 1, 2, 1, 2);

        auto _2_1 = new QLabel(tr("Border color"));
        auto _2_2 = new ColorDialogButton(cfg_->get<QColor>(RECORD_SBC));
        connect(_2_2, &ColorDialogButton::changed, [this](auto&& c){ cfg_->set(RECORD_SBC, c); });
        layout->addWidget(_2_1, 2, 1, 1, 1);
        layout->addWidget(_2_2, 2, 2, 1, 2);

        auto _3_1 = new QLabel(tr("Line type"));
        auto _3_2 = new QComboBox();
        _3_2->addItems({ "NoPen", "SolidLine", "DashLine", "DotLine", "DashDotLine", "DashDotDotLine", "CustomDashLine" });
        _3_2->setCurrentIndex(cfg_->get<int>(RECORD_SBS));
        connect(_3_2,  static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [this](int s){
            cfg_->set(RECORD_SBS, s);
        });
        layout->addWidget(_3_2, 3, 2, 1, 2);
        layout->addWidget(_3_1, 3, 1, 1, 1);

        auto _4_1 = new QLabel(tr("Mask color"));
        auto _4_2 = new ColorDialogButton(cfg_->get<QColor>(RECORD_SMC));
        connect(_4_2, &ColorDialogButton::changed, [this](auto&& c){ cfg_->set(RECORD_SMC, c); });
        layout->addWidget(_4_2, 4, 2, 1, 2);
        layout->addWidget(_4_1, 4, 1, 1, 1);

        layout->setRowStretch(5, 1);
        theme->setLayout(layout);
    }

    // behavier
    {
        auto layout = new QGridLayout();
        layout->setContentsMargins(35, 15, 35, 15);

        auto _01 = new QCheckBox(tr("Auto detect windows"));
        _01->setChecked(cfg_->get<bool>(RECORD_DW));
        connect(_01, &QCheckBox::stateChanged, [this](int state){
            cfg_->set(RECORD_DW, state == Qt::Checked);
        });

        layout->addWidget(_01);
        layout->setRowStretch(2, 1);
        behavior->setLayout(layout);
    }

    // hotkey
    {
        auto layout = new QGridLayout();
        layout->setContentsMargins(35, 15, 35, 15);

        auto _1_1 = new QLabel(tr("Begin/Finish"));
        auto _1_2 = new ShortcutInput(cfg_->get<QKeySequence>(RECORD_HOTKEY));
        connect(_1_2, &ShortcutInput::changed, [this](auto&& ks){ cfg_->set(RECORD_HOTKEY, ks); });
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
    auto behavior = new QWidget(gif_);


    tabwidget->addTab(params, tr("Params"));
    tabwidget->addTab(theme, tr("Appearance"));
    tabwidget->addTab(hotkey, tr("Hotkey"));
    tabwidget->addTab(behavior, tr("Behavior"));

    // params
    {
        auto layout = new QGridLayout();
        layout->setContentsMargins(35, 15, 35, 15);

        auto _1_1 = new QLabel(tr("FPS"));
        auto _1_2 = new QSpinBox();
        _1_2->setValue(cfg_->get<int>(GIF_FPS));
        connect(_1_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int w){
            cfg_->set(GIF_FPS, w);
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
        _1_2->setValue(cfg_->get<int>(GIF_SBW));
        connect(_1_2, static_cast<void (QSpinBox::*)(int)>(&QSpinBox::valueChanged), [this](int w){
            cfg_->set(GIF_SBW, w);
        });
        layout->addWidget(_1_1, 1, 1, 1, 1);
        layout->addWidget(_1_2, 1, 2, 1, 2);

        auto _2_1 = new QLabel(tr("Border color"));
        auto _2_2 = new ColorDialogButton(cfg_->get<QColor>(GIF_SBC));
        connect(_2_2, &ColorDialogButton::changed, [this](auto&& c){ cfg_->set(GIF_SBC, c); });
        layout->addWidget(_2_2, 2, 2, 1, 2);
        layout->addWidget(_2_1, 2, 1, 1, 1);

        auto _3_1 = new QLabel(tr("Line type"));
        auto _3_2 = new QComboBox();
        _3_2->addItems({ "NoPen", "SolidLine", "DashLine", "DotLine", "DashDotLine", "DashDotDotLine", "CustomDashLine" });
        _3_2->setCurrentIndex(cfg_->get<int>(GIF_SBS));
        connect(_3_2,  static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), [this](int s){
            cfg_->set(GIF_SBS, s);
        });
        layout->addWidget(_3_2, 3, 2, 1, 2);
        layout->addWidget(_3_1, 3, 1, 1, 1);

        auto _4_1 = new QLabel(tr("Mask color"));
        auto _4_2 = new ColorDialogButton(cfg_->get<QColor>(GIF_SMC));
        connect(_4_2, &ColorDialogButton::changed, [this](auto&& c){ cfg_->set(GIF_SMC, c); });
        layout->addWidget(_4_2, 4, 2, 1, 2);
        layout->addWidget(_4_1, 4, 1, 1, 1);

        layout->setRowStretch(5, 1);
        theme->setLayout(layout);
    }

    // behavier
    {
        auto layout = new QGridLayout();
        layout->setContentsMargins(35, 15, 35, 15);

        auto _01 = new QCheckBox(tr("Auto detect windows"));
        _01->setChecked(cfg_->get<bool>(GIF_DW));
        connect(_01, &QCheckBox::stateChanged, [this](int state){
            cfg_->set(GIF_DW, state == Qt::Checked);
        });

        layout->addWidget(_01);
        layout->setRowStretch(2, 1);

        behavior->setLayout(layout);
    }

    // hotkey
    {
        auto layout = new QGridLayout();
        layout->setContentsMargins(35, 15, 35, 15);

        auto _1_1 = new QLabel(tr("Begin/Finish"));
        auto _1_2 = new ShortcutInput(cfg_->get<QKeySequence>(GIF_HOTKEY));
        _1_2->setFixedHeight(25);
        connect(_1_2, &ShortcutInput::changed, [this](auto&& ks){
            cfg_->set(GIF_HOTKEY, ks);
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

