#include "camera-player.h"

#include "libcap/devices.h"
#include "logging.h"
#include "menu.h"

#include <probe/defer.h>
#include <probe/thread.h>
#include <QApplication>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QScreen>

#ifdef _WIN32
#include "libcap/win-mfvc/mfvc-capturer.h"
using CameraInput = MFCameraCapturer;
#elif __linux__
#include "libcap/linux-v4l2/v4l2-capturer.h"
using CameraInput = V4l2Capturer;
#endif

CameraPlayer::CameraPlayer(QWidget *parent)
    : FramelessWindow(parent, Qt::WindowTitleHint | Qt::WindowMinMaxButtonsHint |
                                  Qt::WindowFullscreenButtonHint | Qt::WindowStaysOnTopHint)
{
    setAttribute(Qt::WA_DeleteOnClose);

    const auto layout = new QHBoxLayout(this);
    layout->setContentsMargins({});
    texture_ = new TextureRhiWidget();
    layout->addWidget(texture_);
}

int CameraPlayer::open(const std::string& device_id, std::map<std::string, std::string>)
{
    device_id_ = device_id;

    source_ = std::make_unique<CameraInput>();
    if (source_->open(device_id, {}) != 0) {
        loge("[    CAMERA] failed to open video decoder");
        return -1;
    }

    source_->onarrived = [this](auto&& frame, auto) {
        if (frame && frame->data[0]) vbuffer_.wait_and_push(frame);
    };

    // sink video format
    vfmt         = source_->vfmt;
    vfmt.pix_fmt = TextureRhiWidget::IsSupported(vfmt.pix_fmt) ? vfmt.pix_fmt : TextureRhiWidget::PixelFormats()[0];

    ready_ = true;

    // title
    setWindowTitle(QString::fromUtf8(source_->name().c_str()));

    return 0;
}

int CameraPlayer::start()
{
    if (!ready_ || running_ || source_->start() < 0) {
        loge("[     CAMERA] already running or not ready");
        return -1;
    }

    running_ = true;
    thread_  = std::jthread([this] {
        probe::thread::set_name("CAMERA-PLAYER");

#ifdef _WIN32
        ::timeBeginPeriod(1);
        defer(::timeEndPeriod(1));
#endif

        while (running_) {
            const auto frame = vbuffer_.wait_and_pop();
            if (!frame) continue;

            texture_->present(frame.value());
        }
    });

    // resize & move to center
    (vfmt.width > 1920 && vfmt.height > 1080)
        ? resize(QSize(vfmt.width, vfmt.height).scaled(1920, 1080, Qt::KeepAspectRatio))
        : resize(vfmt.width, vfmt.height);

    move(QApplication::primaryScreen()->geometry().center() - QPoint{ width() / 2, height() / 2 });

    QWidget::show();

    return 0;
}

CameraPlayer::~CameraPlayer()
{
    running_ = false;
    vbuffer_.stop();

    logi("[     CAMERA] ~");
}

void CameraPlayer::contextMenuEvent(QContextMenuEvent *event)
{
    const auto menu = new Menu(this);

    menu->addAction(tr("Close"), this, &CameraPlayer::close);

    menu->exec(event->globalPos());
}
