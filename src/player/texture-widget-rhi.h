#ifndef CAPTURER_TEXTURE_WIDGET_RHI_H
#define CAPTURER_TEXTURE_WIDGET_RHI_H

#include "image-render-item.h"
#include "libcap/ffmpeg-wrapper.h"
#include "libcap/media.h"
#include "subtitle-render-item.h"
#include "subtitle.h"
#include "texture-helper.h"

#include <memory>
#include <QRhiWidget>
#include <rhi/qrhi.h>
#include <vector>

class TextureRhiWidget : public QRhiWidget
{
    Q_OBJECT
public:
    explicit TextureRhiWidget(QWidget *parent = nullptr);

    void initialize(QRhiCommandBuffer *cb) override;

    void render(QRhiCommandBuffer *cb) override;

    [[nodiscard]] QSize renderSize() const { return render_sz_; }

    void present(const av::frame& frame);

    void present(const std::list<Subtitle>& subtitles, int changed);

    static AVPixelFormat format(AVPixelFormat, AVPixelFormat = av::texture_formats()[0]);

    void hflip() { hflip_ = hflip_ * (-1.0f); }
    void vflip() { vflip_ = vflip_ * (-1.0f); }

    void hdr(bool en);

    void rotate(int angle);

signals:
    void updateRequest();

private:
    std::mutex mtx_;

    QRhi *rhi_{};

    std::vector<std::shared_ptr<IRenderItem>> items_{};
    std::vector<std::shared_ptr<IRenderItem>> items_slots_[4]{};

    std::atomic<float> hflip_{ 1.0f };
    std::atomic<float> vflip_{ 1.0f };

    QSize image_sz_{};
    QSize render_sz_{};
};

#endif //! CAPTURER_TEXTURE_WIDGET_RHI_H