#ifndef CAPTURER_TEXTURE_HELPER_H
#define CAPTURER_TEXTURE_HELPER_H

#include "libcap/media.h"

#include <QString>
#include <rhi/qrhi.h>
#include <rhi/qshader.h>

extern "C" {
#include <libavutil/pixfmt.h>
}

namespace av
{
    struct TextureDescription
    {
        int32_t             scale_x{};
        int32_t             scale_y{};
        QRhiTexture::Format format{};
        uint32_t            bytes{};
    };

    std::vector<AVPixelFormat> texture_formats();

    std::vector<TextureDescription> get_texture_desc(AVPixelFormat fmt);
    const float                    *get_color_matrix_coefficients(const av::vformat_t& fmt);

    QString get_shader_name(AVPixelFormat fmt);
    QString get_frag_shader_path(AVPixelFormat fmt);

    QShader get_shader(const QString& name);
    QShader get_frag_shader(AVPixelFormat name);
} // namespace av

#endif //! CAPTURER_TEXTURE_HELPER_H
