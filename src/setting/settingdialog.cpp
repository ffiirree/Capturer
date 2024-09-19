#include "settingdialog.h"

#include "capturer.h"
#include "colorpanel.h"
#include "combobox.h"
#include "config.h"
#include "libcap/devices.h"
#include "libcap/hwaccel.h"
#include "path-edit.h"
#include "scrollwidget.h"
#include "version.h"

#include <QCheckBox>
#include <QDir>
#include <QFormLayout>
#include <QKeySequenceEdit>
#include <QLineEdit>
#include <QListWidget>
#include <QSpinBox>
#include <QStackedWidget>
#include <QVBoxLayout>

static const std::vector<std::pair<std::underlying_type_t<Qt::PenStyle>, QString>> PENSTYLES = {
    { Qt::SolidLine, "SolidLine" },
    { Qt::DashLine, "DashLine" },
    { Qt::DotLine, "DotLine" },
    { Qt::DashDotLine, "DashDotLine" },
    { Qt::DashDotDotLine, "DashDotDotLine" },
};

SettingWindow::SettingWindow(QWidget *parent)
    : FramelessWindow(parent, Qt::WindowMinMaxButtonsHint)
{
    setContentsMargins({});

    setAttribute(Qt::WA_DeleteOnClose);

    setWindowTitle(tr("Settings"));

    //
    const auto layout = new QHBoxLayout(this);
    layout->setSpacing(0);
    layout->setContentsMargins({});
    setLayout(layout);

    {
        const auto menu = new QListWidget();
        menu->setFocusPolicy(Qt::NoFocus);
        menu->addItem(tr("General"));
        menu->addItem(tr("Hotkeys"));
        menu->addItem(tr("Screenshot"));
        menu->addItem(tr("Video Recording"));
        menu->addItem(tr("GIF Recording"));
        menu->addItem(tr("Devices"));
        menu->addItem(tr("About"));
        menu->setMinimumWidth(250);
        menu->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
        layout->addWidget(menu);

        const auto vlayout = new QVBoxLayout();
        vlayout->setSpacing(0);
        vlayout->setContentsMargins({});
        {
            const auto close_btn = new QCheckBox();
            close_btn->setObjectName("close-btn");
            close_btn->setCheckable(false);
            connect(close_btn, &QCheckBox::clicked, this, &QWidget::close);

            vlayout->addWidget(close_btn, 0, Qt::AlignRight);

            const auto pages = new QStackedWidget();

            pages->addWidget(setupGeneralWidget());
            pages->addWidget(setupHotkeyWidget());
            pages->addWidget(setupSnipWidget());
            pages->addWidget(setupRecordWidget());
            pages->addWidget(setupGIFWidget());
            pages->addWidget(setupDevicesWidget());
            pages->addWidget(setupAboutWidget());

            vlayout->addWidget(pages);

            connect(menu, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
        }
        layout->addLayout(vlayout);

        menu->setCurrentRow(0);
    }

    resize(960, 640);

    connect(this, &SettingWindow::closed, [] { config::save(); });
}

static QLabel *LABEL(const QString& text, int width)
{
    const auto label = new QLabel(text);
    label->setMinimumWidth(width);
    return label;
}

QWidget *SettingWindow::setupGeneralWidget()
{
    const auto page = new ScrollWidget();
    {
        const auto form = page->addForm(tr("General"));

        const auto autorun = new QCheckBox();
        autorun->setChecked(config::autorun);
        connect(autorun, &QCheckBox::toggled, [](bool checked) { config::set_autorun(checked); });
        form->addRow(LABEL(tr("Run on Startup"), 175), autorun);

        //
        const auto language = new ComboBox({
            { "en_US", "English" },
            { "zh_CN", "简体中文" },
        });
        language->select(config::language);
        language->onselected([](auto value) { config::language = value.toString(); });
        form->addRow(tr("Language"), language);

        //
        const auto file = new QLineEdit(config::filepath);
        file->setContextMenuPolicy(Qt::NoContextMenu);
        file->setReadOnly(true);
        form->addRow(tr("Settings File"), file);

        //
        const auto theme = new ComboBox({
            { "auto", tr("Auto") },
            { "dark", tr("Dark") },
            { "light", tr("Light") },
        });
        theme->onselected([](auto value) { config::set_theme(value.toString().toStdString()); });
        theme->select(config::theme);
        form->addRow(tr("Theme"), theme);
    }

    page->addSpacer();
    return page;
}

QWidget *SettingWindow::setupHotkeyWidget()
{
    using namespace config;

    const auto page = new ScrollWidget();
    {
        const auto form = page->addForm(tr("Hotkeys"));

        const auto updateHotkey = [=](const QString& name, QKeySequence& sequence) {
            const auto edit = new QKeySequenceEdit(sequence);
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
            edit->setMaximumSequenceLength(1);
#endif
            connect(edit, &QKeySequenceEdit::keySequenceChanged, [&](auto ks) {
                sequence = ks;
                App()->UpdateHotkeys();
            });
            form->addRow(LABEL(name, 175), edit);
        };

        updateHotkey(tr("Screenshot"), config::hotkeys::screenshot);
        updateHotkey(tr("Preview Clipboard"), config::hotkeys::preview);
        updateHotkey(tr("Toggle Previews"), config::hotkeys::toggle_previews);
#if _WIN32
        updateHotkey(tr("Quick Look"), config::hotkeys::quick_look);
#endif
        updateHotkey(tr("Transparent Input"), config::hotkeys::transparent_input);
        updateHotkey(tr("Video Recording"), config::hotkeys::record_video);
        updateHotkey(tr("Gif Recording"), config::hotkeys::record_gif);
    }

    page->addSpacer();
    return page;
}

QWidget *SettingWindow::setupSnipWidget()
{
    const auto page = new ScrollWidget();
    {
        const auto form = page->addForm(tr("Appearance"));

        const auto bwidth = new QSpinBox();
        bwidth->setRange(1, 6);
        bwidth->setContextMenuPolicy(Qt::NoContextMenu);
        bwidth->setValue(config::snip::style.border_width);
        connect(bwidth, QOverload<int>::of(&QSpinBox::valueChanged),
                [](int w) { config::snip::style.border_width = w; });
        connect(bwidth, QOverload<int>::of(&QSpinBox::valueChanged),
                [] { App()->UpdateScreenshotStyle(); });
        form->addRow(LABEL(tr("Border Width"), 175), bwidth);

        const auto bcolor = new ColorDialogButton(config::snip::style.border_color);
        connect(bcolor, &ColorDialogButton::changed,
                [](auto&& c) { config::snip::style.border_color = c; });
        connect(bcolor, &ColorDialogButton::changed, [] { App()->UpdateScreenshotStyle(); });
        form->addRow(tr("Border Color"), bcolor);

        const auto bstyle = new ComboBox();
        bstyle->add(PENSTYLES);
        bstyle->onselected([&](auto value) {
            config::snip::style.border_style = static_cast<Qt::PenStyle>(value.toInt());
        });
        connect(bstyle, &ComboBox::selected, [] { App()->UpdateScreenshotStyle(); });
        bstyle->select(static_cast<int>(config::snip::style.border_style));
        form->addRow(tr("Border Style"), bstyle);

        const auto mcolor = new ColorDialogButton(config::snip::style.mask_color);
        connect(mcolor, &ColorDialogButton::changed, [&](auto&& c) { config::snip::style.mask_color = c; });
        connect(mcolor, &ColorDialogButton::changed, [] { App()->UpdateScreenshotStyle(); });
        form->addRow(tr("Mask Color"), mcolor);
    }

    const auto form = page->addForm(tr("File"));
    {
        const auto path = new PathEdit(config::snip::path);
        connect(path, &PathEdit::changed, [](auto&& dir) { config::snip::path = dir; });
        form->addRow(LABEL(tr("Save Path"), 175), path);
    }

    page->addSpacer();
    return page;
}

QWidget *SettingWindow::setupRecordWidget()
{
    const auto page = new ScrollWidget();
    {
        const auto form = page->addForm(tr("Appearance"));

        auto& style = config::recording::video::style;

        const auto bwidth = new QSpinBox();
        bwidth->setRange(1, 6);
        bwidth->setContextMenuPolicy(Qt::NoContextMenu);
        bwidth->setValue(style.border_width);
        connect(bwidth, QOverload<int>::of(&QSpinBox::valueChanged),
                [&](auto w) { style.border_width = w; });
        form->addRow(LABEL(tr("Border Width"), 175), bwidth);

        const auto bcolor = new ColorDialogButton(style.border_color);
        connect(bcolor, &ColorDialogButton::changed, [&](auto c) { style.border_color = c; });
        form->addRow(tr("Border Color"), bcolor);

        const auto bstyle = new ComboBox();
        bstyle->add(PENSTYLES)
            .onselected([&](auto value) { style.border_style = static_cast<Qt::PenStyle>(value.toInt()); })
            .select(static_cast<int>(style.border_style));
        form->addRow(tr("Border Style"), bstyle);

        const auto mcolor = new ColorDialogButton(style.mask_color);
        connect(mcolor, &ColorDialogButton::changed, [&](auto&& c) { style.mask_color = c; });
        form->addRow(tr("Mask Color"), mcolor);

        const auto region = new QCheckBox();
        region->setChecked(config::recording::video::show_region);
        connect(region, &QCheckBox::toggled,
                [](auto checked) { config::recording::video::show_region = checked; });
        form->addRow(tr("Show Region"), region);

        const auto hmenu = new QCheckBox();
        hmenu->setChecked(config::recording::video::floating_menu);
        connect(hmenu, &QCheckBox::toggled,
                [](auto checked) { config::recording::video::floating_menu = checked; });
        form->addRow(tr("Floating Menu"), hmenu);
    }

    {
        const auto form = page->addForm(tr("File"));

        const auto path = new PathEdit(config::recording::video::path);
        connect(path, &PathEdit::changed, [](auto&& dir) { config::recording::video::path = dir; });
        form->addRow(LABEL(tr("Save Path"), 175), path);

        const auto format = new ComboBox();
        format->add({ { "mp4", "MPEG-4 (.mp4)" }, { "mkv", "Matroska Video (.mkv)" } })
            .onselected([](auto value) { config::recording::video::mcf = value.toString(); })
            .select(config::recording::video::mcf);
        form->addRow(tr("Format"), format);
    }

    {
        const auto form = page->addForm(tr("Captured by Default"));

        auto mouse = new QCheckBox();
        mouse->setChecked(config::recording::video::capture_mouse);
        connect(mouse, &QCheckBox::toggled,
                [](auto checked) { config::recording::video::capture_mouse = checked; });
        form->addRow(LABEL(tr("Mouse"), 175), mouse);

        const auto mic_en = new QCheckBox();
        mic_en->setChecked(config::recording::video::mic_enabled);
        connect(mic_en, &QCheckBox::toggled,
                [](auto checked) { config::recording::video::mic_enabled = checked; });
        form->addRow(tr("Microphone"), mic_en);

        const auto sa_en = new QCheckBox();
        sa_en->setChecked(config::recording::video::speaker_enabled);
        connect(sa_en, &QCheckBox::toggled,
                [](auto checked) { config::recording::video::speaker_enabled = checked; });
        form->addRow(tr("System Audio"), sa_en);
    }

    {
        const auto form = page->addForm(tr("Video"));

        const auto encoder = new ComboBox();
        encoder->add({
            { "libx264", tr("Software x264 [H.264 / AVC]") },
            { "libx265", tr("Software x265 [H.265 / HEVC]") },
        });
        if (av::hwaccel::is_supported(AV_HWDEVICE_TYPE_CUDA)) {
            encoder->add({
                { "h264_nvenc", tr("Hardware NVENC [H.264 / AVC]") },
                { "hevc_nvenc", tr("Hardware NVENC [H.265 / HEVC]") },
            });
        }
        encoder->onselected(
            [](auto value) { config::recording::video::v::codec = value.toString().toStdString(); });
        encoder->select(QString::fromStdString(config::recording::video::v::codec));
        form->addRow(tr("Encoder"), encoder);

        const auto framerate = new ComboBox();
        framerate
            ->add({
                { QPoint{ 10, 1 }, "10" },
                { QPoint{ 20, 1 }, "20" },
                { QPoint{ 24, 1 }, "24 NTSC" },
                { QPoint{ 25, 1 }, "25 PAL" },
                { QPoint{ 30000, 1001 }, "29.97" },
                { QPoint{ 30, 1 }, "30" },
                { QPoint{ 48, 1 }, "48" },
                { QPoint{ 50, 1 }, "50 PAL" },
                { QPoint{ 60000, 1001 }, "59.94" },
                { QPoint{ 60, 1 }, "60" },
                { QPoint{ 120, 1 }, "120" },
            })
            .onselected([](auto value) {
                const auto fps                         = value.toPoint();
                config::recording::video::v::framerate = { fps.x(), fps.y() };
            })
            .select(QPoint{ config::recording::video::v::framerate.num,
                            config::recording::video::v::framerate.den });
        form->addRow(LABEL(tr("Framerate"), 175), framerate);

        const auto ratectrl = new ComboBox();
        ratectrl->add("crf", "CRF")
            .onselected([](auto value) {
                config::recording::video::v::rate_control = value.toString().toStdString();
            })
            .select(QString::fromStdString(config::recording::video::v::rate_control));
        form->addRow(tr("Rate Control"), ratectrl);

        const auto crf = new QSpinBox();
        crf->setRange(0, 51);
        crf->setContextMenuPolicy(Qt::NoContextMenu);
        crf->setValue(config::recording::video::v::crf);
        connect(crf, QOverload<int>::of(&QSpinBox::valueChanged),
                [&](auto w) { config::recording::video::v::crf = w; });
        form->addRow("CRF/CQ", crf);

        const auto preset = new ComboBox();
        preset
            ->add({
                { "", "(none)" },
                { "ultrafast", "ultrafast" },
                { "superfast", "superfast" },
                { "veryfast", "veryfast" },
                { "faster", "faster" },
                { "fast", "fast" },
                { "medium", "medium" },
                { "slow", "slow" },
                { "slower", "slower" },
                { "veryslow", "veryslow" },
            })
            .onselected(
                [](auto value) { config::recording::video::v::preset = value.toString().toStdString(); })
            .select(QString::fromStdString(config::recording::video::v::preset));
        form->addRow(tr("Preset"), preset);

        const auto profile = new ComboBox();
        profile
            ->add({
                { "", "(none)" },
                { "baseline", "baseline" },
                { "main", "main" },
                { "high", "high" },
            })
            .onselected(
                [](auto value) { config::recording::video::v::profile = value.toString().toStdString(); })
            .select(QString::fromStdString(config::recording::video::v::profile));
        form->addRow(tr("Profile"), profile);

        const auto tune = new ComboBox();
        tune->add({
                      { "", "(none)" },
                      { "film", "film" },
                      { "animation", "animation" },
                      { "grain", "grain" },
                      { "stillimage", "stillimage" },
                      { "psnr", "psnr" },
                      { "ssim", "ssim" },
                      { "fastdecode", "fastdecode" },
                      { "zerolatency", "zerolatency" },
                  })
            .onselected(
                [](auto value) { config::recording::video::v::tune = value.toString().toStdString(); })
            .select(QString::fromStdString(config::recording::video::v::tune));
        form->addRow(tr("Tune"), tune);

        const auto pixfmt = new ComboBox();
        pixfmt
            ->add({
                { AV_PIX_FMT_YUV420P, "YUV420P (8-bit, planar YUV 4:2:0, 12bpp)" },
                { AV_PIX_FMT_YUV422P, "YUV422P (8-bit, planar YUV 4:2:2, 16bpp)" },
                { AV_PIX_FMT_YUV444P, "YUV444P (8-bit, planar YUV 4:4:4, 24bpp)" },
                { AV_PIX_FMT_YUV420P10LE, "YUV420P10 (10-bit, planar YUV 4:2:0, 15bpp)" },

            })
            .onselected([](auto value) {
                config::recording::video::v::pix_fmt = static_cast<AVPixelFormat>(value.toUInt());
            })
            .select(config::recording::video::v::pix_fmt);
        form->addRow(tr("Pixel Format"), pixfmt);

        const auto colorspace = new ComboBox();
        colorspace
            ->add({
                { AVCOL_SPC_RGB, "sRGB" },
                { AVCOL_SPC_BT709, "BT709" },
                { AVCOL_SPC_BT470BG, "BT601" },
                { AVCOL_SPC_BT2020_NCL, "BT2020" },
            })
            .onselected([](auto value) {
                config::recording::video::v::color_space = static_cast<AVColorSpace>(value.toUInt());
            })
            .select(config::recording::video::v::color_space);
        form->addRow(tr("Color Space"), colorspace);

        const auto colorrange = new ComboBox();
        colorrange
            ->add({
                { AVCOL_RANGE_MPEG, "Limited (TV/MPEG)" },
                { AVCOL_RANGE_JPEG, "Full (PC/JPEG)" },
            })
            .onselected([](auto value) {
                config::recording::video::v::color_range = static_cast<AVColorRange>(value.toUInt());
            })
            .select(config::recording::video::v::color_range);
        form->addRow(tr("Color Range"), colorrange);
    }

    {
        const auto form = page->addForm(tr("Audio"));

        const auto codec = new ComboBox();
        codec->add("aac", "AAC / Advanced Audio Coding")
            .onselected(
                [](auto value) { config::recording::video::a::codec = value.toString().toStdString(); })
            .select(QString::fromStdString(config::recording::video::a::codec));
        form->addRow(LABEL(tr("Encoder"), 175), codec);

        const auto channels = new ComboBox();
        channels->add(2, "Stereo")
            .onselected([](auto value) { config::recording::video::a::channels = value.toInt(); })
            .select(config::recording::video::a::channels);
        form->addRow(tr("Channel Layout"), channels);

        const auto srate = new ComboBox();
        srate->add(48000, "48 kHz")
            .onselected([](auto r) { config::recording::video::a::sample_rate = r.toInt(); })
            .select(config::recording::video::a::sample_rate);
        form->addRow(tr("Sample Rate"), srate);
    }

    page->addSpacer();
    return page;
}

QWidget *SettingWindow::setupGIFWidget()
{
    const auto page = new ScrollWidget();
    {
        const auto form = page->addForm(tr("Appearance"));

        auto&      style  = config::recording::gif::style;
        const auto bwidth = new QSpinBox();
        bwidth->setRange(1, 6);
        bwidth->setValue(style.border_width);
        connect(bwidth, QOverload<int>::of(&QSpinBox::valueChanged),
                [&](int width) { style.border_width = width; });
        form->addRow(LABEL(tr("Border Width"), 175), bwidth);

        const auto bcolor = new ColorDialogButton(style.border_color);
        connect(bcolor, &ColorDialogButton::changed, [&](auto&& c) { style.border_color = c; });
        form->addRow(tr("Border Color"), bcolor);

        const auto bstyle = new ComboBox();
        bstyle->add(PENSTYLES);
        bstyle->onselected([&](auto s) { style.border_style = static_cast<Qt::PenStyle>(s.toInt()); });
        bstyle->select(static_cast<int>(style.border_style));
        form->addRow(tr("Border Style"), bstyle);

        const auto mcolor = new ColorDialogButton(style.mask_color);
        connect(mcolor, &ColorDialogButton::changed, [&](auto&& c) { style.mask_color = c; });
        form->addRow(tr("Mask Color"), mcolor);

        const auto region = new QCheckBox();
        region->setChecked(config::recording::gif::show_region);
        connect(region, &QCheckBox::toggled,
                [](auto checked) { config::recording::gif::show_region = checked; });
        form->addRow(tr("Show Region"), region);

        const auto hmenu = new QCheckBox();
        hmenu->setChecked(config::recording::gif::floating_menu);
        connect(hmenu, &QCheckBox::toggled,
                [](auto checked) { config::recording::gif::floating_menu = checked; });
        form->addRow(tr("Floating Menu"), hmenu);
    }

    {
        const auto form = page->addForm(tr("File"));

        const auto path = new PathEdit(config::recording::gif::path);
        connect(path, &PathEdit::changed, [](auto&& dir) { config::recording::gif::path = dir; });
        form->addRow(LABEL(tr("Save Path"), 175), path);
    }

    {
        const auto form = page->addForm(tr("Captured by Default"));

        auto mouse = new QCheckBox();
        mouse->setChecked(config::recording::gif::capture_mouse);
        connect(mouse, &QCheckBox::toggled,
                [](auto checked) { config::recording::gif::capture_mouse = checked; });
        form->addRow(LABEL(tr("Mouse"), 175), mouse);
    }

    {
        const auto form = page->addForm(tr("Params"));

        const auto framerate = new QSpinBox();
        framerate->setContextMenuPolicy(Qt::NoContextMenu);
        framerate->setValue(config::recording::gif::framerate.num);
        connect(framerate, QOverload<int>::of(&QSpinBox::valueChanged),
                [](int w) { config::recording::gif::framerate.num = w; });
        form->addRow(LABEL(tr("Framerate"), 175), framerate);

        const auto colors = new QSpinBox();
        colors->setRange(32, 256);
        colors->setValue(config::recording::gif::colors);
        connect(colors, QOverload<int>::of(&QSpinBox::valueChanged),
                [&](int n) { config::recording::gif::colors = n; });
        form->addRow(tr("Max Colors"), colors);

        const auto dither = new QCheckBox();
        dither->setChecked(config::recording::gif::dither);
        connect(dither, &QCheckBox::toggled,
                [](auto checked) { config::recording::gif::dither = checked; });
        form->addRow(tr("Dither"), dither);
    }

    page->addSpacer();
    return page;
}

QWidget *SettingWindow::setupDevicesWidget()
{
    // microphones
    std::vector<std::pair<QVariant, QString>> microphones{};
    for (const auto& dev : av::audio_sources()) {
#ifdef _WIN32
        std::string name = dev.description + " - " + dev.name;
#else
        std::string name = dev.name;
#endif
        microphones.emplace_back(QString::fromUtf8(dev.id.c_str()), QString::fromUtf8(name.c_str()));
    }

    // speakers
    std::vector<std::pair<QVariant, QString>> speakers{};

    for (const auto& dev : av::audio_sinks()) {
#ifdef _WIN32
        std::string name = dev.description + " - " + dev.name;
#else
        std::string name = dev.name;
#endif
        speakers.emplace_back(QString::fromUtf8(dev.id.c_str()), QString::fromUtf8(name.c_str()));
    }

    // cameras
    std::vector<std::pair<QVariant, QString>> cameras{};
    for (const auto& dev : av::cameras()) {
        cameras.emplace_back(QString::fromUtf8(dev.id.c_str()), QString::fromUtf8(dev.name.c_str()));
    }

    const auto page = new ScrollWidget();
    {
        const auto form = page->addForm(tr("Devices"));

        const auto mic = new ComboBox(microphones);
        mic->onselected([](auto value) { config::devices::mic = value.toString().toStdString(); });
        auto default_src = av::default_audio_source();
        if (default_src.has_value()) mic->select(default_src.value().id);
        form->addRow(LABEL(tr("Microphone"), 175), mic);

        const auto speaker = new ComboBox(speakers);
        speaker->onselected([](auto value) { config::devices::speaker = value.toString().toStdString(); });
        auto default_sink = av::default_audio_sink();
        if (default_sink.has_value()) speaker->select(default_sink.value().id);
        form->addRow(tr("Speaker"), speaker);

        const auto camera = new ComboBox(cameras);
        camera->onselected([](auto value) { config::devices::camera = value.toString().toStdString(); });
        if (config::devices::camera.empty()) camera->select(config::devices::camera);
        form->addRow(tr("Camera"), camera);
    }

    page->addSpacer();
    return page;
}

QWidget *SettingWindow::setupAboutWidget()
{
    const auto page    = new QWidget();
    const auto vlayout = new QVBoxLayout();
    vlayout->setContentsMargins(35, 0, 35, 15);

    {
        vlayout->addStretch(1);

        const auto icon = new QLabel();
        icon->setPixmap(QPixmap(":/icons/capturer"));
        icon->setAlignment(Qt::AlignCenter);
        vlayout->addWidget(icon);

        const auto name = new QLabel("Capturer");
        name->setAlignment(Qt::AlignCenter);
        name->setObjectName("about-name");
        vlayout->addWidget(name);

        const auto version = new QLabel("Version : " + QString(CAPTURER_VERSION));
        version->setAlignment(Qt::AlignCenter);
        version->setObjectName("about-version");
        vlayout->addWidget(version);

        vlayout->addStretch(2);

        const auto copyright = new QLabel(tr("Copyright © 2018 - 2024 ffiirree"));
        copyright->setAlignment(Qt::AlignCenter);
        copyright->setObjectName("about-copyright");
        vlayout->addWidget(copyright);
    }
    page->setLayout(vlayout);

    return page;
}