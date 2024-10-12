#ifndef CAPTURER_RENDER_ITEM_H
#define CAPTURER_RENDER_ITEM_H

#include <any>
#include <rhi/qrhi.h>

class IRenderItem
{
public:
    virtual ~IRenderItem() = default;

    virtual bool attach(const std::any&) = 0;

    [[nodiscard]] virtual QSize size() const = 0;

    virtual void rotate(int) {};

    virtual void hdr(bool) {} // HDR to SDR

    virtual void create(QRhi *rhi, QRhiRenderTarget *rt)                            = 0;
    virtual void upload(QRhiResourceUpdateBatch *rub, float scale_x, float scale_y) = 0;
    virtual void draw(QRhiCommandBuffer *cb, const QRhiViewport& viewport)          = 0;
};

#endif //! CAPTURER_RENDER_ITEM_H