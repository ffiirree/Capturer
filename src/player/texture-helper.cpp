#include "texture-helper.h"

#include "logging.h"

#include <QFile>

// clang-format off
// normalized device coordinates
static constexpr float vertices[] = {
  // vertex coordinate: [-1.0, 1.0]    / texture coordinate: [0.0, 1.0]
  //  x      y                         / x     y
    -1.0f, -1.0f,  /* bottom left  */  0.0f, 1.0f, /* top    left  */
    +1.0f, -1.0f,  /* bottom right */  1.0f, 1.0f, /* top    right */
    -1.0f, +1.0f,  /* top    left  */  0.0f, 0.0f, /* bottom left  */
    +1.0f, +1.0f,  /* top    right */  1.0f, 0.0f, /* bottom right */
};

static QMatrix4x4 BT601_FULL_CM {
    1.0000f,  0.0000f,  1.77200f, -0.88600f,
    1.0000f, -0.1646f, -0.57135f,  0.36795f,
    1.0000f,  1.4200f,  0.00000f, -0.71000f,
    0.0000f,  0.0000f,  0.00000f,  1.00000f
};

static QMatrix4x4 BT601_TV_CM {
    1.164f,  0.000f,  1.596f, -0.8708f,
    1.164f, -0.392f, -0.813f,  0.5296f,
    1.164f,  2.017f,  0.000f, -1.0810f,
    0.000f,  0.000f,  0.000f,  1.0000f
};

static QMatrix4x4 BT709_FULL_CM {
    1.0000f,  0.000000f,  1.57480f,  -0.790488f,
    1.0000f, -0.187324f, -0.468124f,  0.329010f,
    1.0000f,  1.855600f,  0.00000f,  -0.931439f,
    0.0000f,  0.000000f,  0.00000f,   1.000000f
};

static QMatrix4x4 BT709_TV_CM {
    1.1644f,  0.0000f,  1.7927f, -0.9729f,
    1.1644f, -0.2132f, -0.5329f,  0.3015f,
    1.1644f,  2.1124f,  0.0000f, -1.1334f,
    0.0000f,  0.0000f,  0.0000f,  1.0000f
};

static QMatrix4x4 BT2020_FULL_CM {
    1.0000f,  0.0000f,  1.4746f, -0.7402f,
    1.0000f, -0.1646f, -0.5714f,  0.3694f,
    1.0000f,  1.8814f,  0.0000f, -0.9445f,
    0.0000f,  0.0000f,  0.0000f,  1.0000f
};

static QMatrix4x4 BT2020_TV_CM {
    1.1644f,  0.000f,   1.6787f, -0.9157f,
    1.1644f, -0.1874f, -0.6504f,  0.3475f,
    1.1644f,  2.1418f,  0.0000f, -1.1483f,
    0.0000f,  0.0000f,  0.0000f,  1.0000f
};

static QMatrix4x4 IDENTITY_CM {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};
// clang-format on

namespace av
{
    std::vector<AVPixelFormat> texture_formats()
    {
        // clang-format off
        return {
            AV_PIX_FMT_YUV420P, ///< planar YUV 4:2:0, 12bpp, (1 Cr & Cb sample per 2x2 Y samples)

            AV_PIX_FMT_YUYV422, ///< packed YUV 4:2:2, 16bpp, Y0 Cb Y1 Cr

            AV_PIX_FMT_YUV422P, ///< planar YUV 4:2:2, 16bpp, (1 Cr & Cb sample per 2x1 Y samples)
            AV_PIX_FMT_YUV444P, ///< planar YUV 4:4:4, 24bpp, (1 Cr & Cb sample per 1x1 Y samples)
            AV_PIX_FMT_YUV410P, ///< planar YUV 4:1:0,  9bpp, (1 Cr & Cb sample per 4x4 Y samples)
            AV_PIX_FMT_YUV411P, ///< planar YUV 4:1:1, 12bpp, (1 Cr & Cb sample per 4x1 Y samples)

            AV_PIX_FMT_GRAY8,     ///<        Y        ,  8bpp
            AV_PIX_FMT_GRAY16LE,  ///<        Y        , 16bpp, little-endian

            AV_PIX_FMT_YUVJ420P,  ///< planar YUV 4:2:0, 12bpp, full scale (JPEG), deprecated in favor of AV_PIX_FMT_YUV420P and setting color_range
            AV_PIX_FMT_YUVJ422P,  ///< planar YUV 4:2:2, 16bpp, full scale (JPEG), deprecated in favor of AV_PIX_FMT_YUV422P and setting color_range
            AV_PIX_FMT_YUVJ444P,  ///< planar YUV 4:4:4, 24bpp, full scale (JPEG), deprecated in favor of AV_PIX_FMT_YUV444P and setting color_range

            AV_PIX_FMT_YUVA444P,  ///< planar YUV 4:4:4 32bpp, (1 Cr & Cb sample per 1x1 Y & A samples)

            AV_PIX_FMT_NV12,    ///< planar YUV 4:2:0, 12bpp, 1 plane for Y and 1 plane for the UV components, < which are interleaved (first byte U and the following byte V)
            AV_PIX_FMT_NV21,    ///< as above, but U and V bytes are swapped

            AV_PIX_FMT_ARGB,    ///< packed ARGB 8:8:8:8, 32bpp, ARGBARGB...
            AV_PIX_FMT_RGBA,    ///< packed RGBA 8:8:8:8, 32bpp, RGBARGBA...
            AV_PIX_FMT_ABGR,    ///< packed ABGR 8:8:8:8, 32bpp, ABGRABGR...
            AV_PIX_FMT_BGRA,    ///< packed BGRA 8:8:8:8, 32bpp, BGRABGRA...

            AV_PIX_FMT_0RGB,    ///< packed RGB 8:8:8, 32bpp, XRGBXRGB...   X=unused/undefined
            AV_PIX_FMT_RGB0,    ///< packed RGB 8:8:8, 32bpp, RGBXRGBX...   X=unused/undefined
            AV_PIX_FMT_0BGR,    ///< packed BGR 8:8:8, 32bpp, XBGRXBGR...   X=unused/undefined
            AV_PIX_FMT_BGR0,    ///< packed BGR 8:8:8, 32bpp, BGRXBGRX...   X=unused/undefined

            AV_PIX_FMT_YUV420P10LE, ///< planar YUV 4:2:0, 15bpp, (1 Cr & Cb sample per 2x2 Y samples), little-endian

            AV_PIX_FMT_GBRP,      ///< planar GBR 4:4:4 24bpp
        };
        // clang-format on
    }

    std::vector<TextureDescription> get_texture_desc(AVPixelFormat fmt)
    {
        switch (fmt) {
        case AV_PIX_FMT_YUYV422: return { { 2, 1, QRhiTexture::RGBA8, 4 } };

        case AV_PIX_FMT_GRAY8:   return { { 1, 1, QRhiTexture::R8, 1 } };

        case AV_PIX_FMT_YUV420P:
            return {
                { 1, 1, QRhiTexture::R8, 1 },
                { 2, 2, QRhiTexture::R8, 1 },
                { 2, 2, QRhiTexture::R8, 1 },
            };

        case AV_PIX_FMT_YUV422P:
            return {
                { 1, 1, QRhiTexture::R8, 1 },
                { 2, 1, QRhiTexture::R8, 1 },
                { 2, 1, QRhiTexture::R8, 1 },
            };

        case AV_PIX_FMT_YUV444P:
            return {
                { 1, 1, QRhiTexture::R8, 1 },
                { 1, 1, QRhiTexture::R8, 1 },
                { 1, 1, QRhiTexture::R8, 1 },
            };

        case AV_PIX_FMT_YUV410P:
            return {
                { 1, 1, QRhiTexture::R8, 1 },
                { 4, 4, QRhiTexture::R8, 1 },
                { 4, 4, QRhiTexture::R8, 1 },
            };

        case AV_PIX_FMT_YUV411P:
            return {
                { 1, 1, QRhiTexture::R8, 1 },
                { 4, 1, QRhiTexture::R8, 1 },
                { 4, 1, QRhiTexture::R8, 1 },
            };

        case AV_PIX_FMT_GRAY16LE: return { { 1, 1, QRhiTexture::R16, 2 } };

        case AV_PIX_FMT_YUVJ420P:
            return {
                { 1, 1, QRhiTexture::R8, 1 },
                { 2, 2, QRhiTexture::R8, 1 },
                { 2, 2, QRhiTexture::R8, 1 },
            };

        case AV_PIX_FMT_YUVJ422P:
            return {
                { 1, 1, QRhiTexture::R8, 1 },
                { 2, 1, QRhiTexture::R8, 1 },
                { 2, 1, QRhiTexture::R8, 1 },
            };

        case AV_PIX_FMT_YUVJ444P:
            return {
                { 1, 1, QRhiTexture::R8, 1 },
                { 1, 1, QRhiTexture::R8, 1 },
                { 1, 1, QRhiTexture::R8, 1 },
            };

        case AV_PIX_FMT_UYVY422: return { { 2, 1, QRhiTexture::RGBA8, 4 } };

        case AV_PIX_FMT_YUVA444P:
            return {
                { 1, 1, QRhiTexture::R8, 1 },
                { 1, 1, QRhiTexture::R8, 1 },
                { 1, 1, QRhiTexture::R8, 1 },
                { 1, 1, QRhiTexture::R8, 1 },
            };

        case AV_PIX_FMT_NV12:
        case AV_PIX_FMT_NV21:
            return {
                { 1, 1, QRhiTexture::R8, 1 },
                { 2, 2, QRhiTexture::RG8, 2 },
            };

        case AV_PIX_FMT_ARGB:
        case AV_PIX_FMT_RGBA:
        case AV_PIX_FMT_ABGR:
        case AV_PIX_FMT_BGRA:
        case AV_PIX_FMT_0RGB:
        case AV_PIX_FMT_RGB0:
        case AV_PIX_FMT_0BGR:
        case AV_PIX_FMT_BGR0: return { { 1, 1, QRhiTexture::RGBA8, 4 } };

        case AV_PIX_FMT_YUV420P10LE:
            return {
                { 1, 1, QRhiTexture::R16, 2 },
                { 2, 2, QRhiTexture::R16, 2 },
                { 2, 2, QRhiTexture::R16, 2 },
            };

        case AV_PIX_FMT_GBRP:
            return {
                { 1, 1, QRhiTexture::R8, 1 },
                { 1, 1, QRhiTexture::R8, 1 },
                { 1, 1, QRhiTexture::R8, 1 },
            };

        default: loge("unsupported pixel format: {}", av::to_string(fmt)); return {};
        }
    }

    const float *get_color_matrix_coefficients(const av::vformat_t& fmt)
    {
        auto space = fmt.color.space;
        auto range = fmt.color.range;
        if (fmt.color.space == AVCOL_SPC_UNSPECIFIED && fmt.height > 0 && fmt.width > 0) {
            if (fmt.height <= 576 || fmt.width <= 1024) {
                space = AVCOL_SPC_BT470BG;
            }
            else if (fmt.height <= 1536 || fmt.width <= 2048) {
                space = AVCOL_SPC_BT709;
            }
            else {
                space = AVCOL_SPC_BT2020_NCL;
            }
        }

        switch (space) {
        case AVCOL_SPC_BT470BG:
            return ((range == AVCOL_RANGE_JPEG) ? BT601_FULL_CM : BT601_TV_CM).constData();
        case AVCOL_SPC_BT2020_NCL:
            return ((range == AVCOL_RANGE_JPEG) ? BT2020_FULL_CM : BT2020_TV_CM).constData();
        case AVCOL_SPC_BT709:
        default:              return ((range == AVCOL_RANGE_JPEG) ? BT709_FULL_CM : BT709_TV_CM).constData();
        }
    }

    QString get_shader_name(AVPixelFormat fmt)
    {
        switch (fmt) {
        case AV_PIX_FMT_PAL8:        return "pal8";
        case AV_PIX_FMT_GRAY8:
        case AV_PIX_FMT_GRAY16LE:    return "gray";
        case AV_PIX_FMT_YUV420P:     // planar
        case AV_PIX_FMT_YUV422P:
        case AV_PIX_FMT_YUV444P:
        case AV_PIX_FMT_YUV410P:
        case AV_PIX_FMT_YUV411P:
        case AV_PIX_FMT_YUVJ420P:
        case AV_PIX_FMT_YUVJ422P:
        case AV_PIX_FMT_YUVJ444P:    return "yuv";
        case AV_PIX_FMT_YUVA444P:    return "yuva";
        case AV_PIX_FMT_YUV420P10LE: return "yuv_p10";
        case AV_PIX_FMT_NV12:        return "nv12"; // semi-planar
        case AV_PIX_FMT_NV21:        return "nv21";
        case AV_PIX_FMT_YUYV422:     return "yuyv"; // packed
        case AV_PIX_FMT_UYVY422:     return "uyvy";
        case AV_PIX_FMT_ARGB:
        case AV_PIX_FMT_0RGB:        return "argb";
        case AV_PIX_FMT_RGBA:
        case AV_PIX_FMT_RGB0:        return "bgra";
        case AV_PIX_FMT_ABGR:
        case AV_PIX_FMT_0BGR:        return "abgr";
        case AV_PIX_FMT_BGRA:
        case AV_PIX_FMT_BGR0:        return "bgra";
        case AV_PIX_FMT_GBRP:        return "gbrp"; // planar
        default:                     return {};
        }
    }

    QString get_frag_shader_path(AVPixelFormat fmt)
    {
        return ":/src/resources/shaders/" + av::get_shader_name(fmt) + ".frag.qsb";
    }

    QShader get_shader(const QString& name)
    {
        QFile f(name);
        return f.open(QIODevice::ReadOnly) ? QShader::fromSerialized(f.readAll()) : QShader();
    }

    QShader get_frag_shader(const AVPixelFormat fmt)
    {
        QFile f(get_frag_shader_path(fmt));
        return f.open(QIODevice::ReadOnly) ? QShader::fromSerialized(f.readAll()) : QShader();
    }

} // namespace av
