#include "texture-widget-opengl.h"

#include "logging.h"

#include <fmt/format.h>
#include <map>
#include <QCoreApplication>

// clang-format off
static constexpr GLfloat coordinate[] = {
  //  x      y     z
    -1.0f, -1.0f, 0.0f, // bottom left
    +1.0f, -1.0f, 0.0f, // bottom right
    -1.0f, +1.0f, 0.0f, // top    left
    +1.0f, +1.0f, 0.0f, // top    right
    
  // x     y
    0.0f, 1.0f,         // top    left
    1.0f, 1.0f,         // top    right
    0.0f, 0.0f,         // bottom left
    1.0f, 0.0f,         // bottom right
};
// clang-format on

// https://github.com/libsdl-org/SDL/blob/main/src/render/opengl/SDL_shaders_gl.c
static const QString TEXTURE_VERTEX_SHADER = R"(
    attribute vec3 vertex;
    attribute vec2 texture;

    varying vec2 texCoord;

    void main(void)
    {
        gl_Position = vec4(vertex, 1.0);
        texCoord = texture;
    }
)";

static const QString JPEG_SHADER_CONSTANTS = R"(
    // YUV offset
    const vec3 offset = vec3(0, -0.501960814, -0.501960814);

    // RGB coefficients
    const vec3 Rcoeff = vec3(1,  0.0000,  1.4020);
    const vec3 Gcoeff = vec3(1, -0.3441, -0.7141);
    const vec3 Bcoeff = vec3(1,  1.7720,  0.0000);
)";

static const QString BT601_SHADER_CONSTANTS = R"( 
    // YUV offset
    const vec3 offset = vec3(-0.0627451017, -0.501960814, -0.501960814);

    // RGB coefficients
    const vec3 Rcoeff = vec3(1.1644,  0.0000,  1.596);
    const vec3 Gcoeff = vec3(1.1644, -0.3918, -0.813);
    const vec3 Bcoeff = vec3(1.1644,  2.0172,  0.000);
)";

static const QString BT709_SHADER_CONSTANTS = R"(
    // YUV offset
    const vec3 offset = vec3(-0.0627451017, -0.501960814, -0.501960814);

    // RGB coefficients
    const vec3 Rcoeff = vec3(1.1644,  0.0000,  1.7927);
    const vec3 Gcoeff = vec3(1.1644, -0.2132, -0.5329);
    const vec3 Bcoeff = vec3(1.1644,  2.1124,  0.0000);
)";

static const QString TEX1_SHADER_PROLOGUE = R"(
    varying vec2 texCoord;

    uniform sampler2D tex0; // Y
)";

static const QString TEX2_SHADER_PROLOGUE = R"(
    varying vec2 texCoord;

    uniform sampler2D tex0; // Y
    uniform sampler2D tex1; // Y
)";

static const QString TEX3_SHADER_PROLOGUE = R"(
    varying vec2 texCoord;

    uniform sampler2D tex0; // Y
    uniform sampler2D tex1; // U
    uniform sampler2D tex2; // V
)";

// YUV
static const QString YUV_FRAGMENT_SHADER = R"(
    void main(void)
    {
        vec3 yuv, rgb;

        yuv.x = texture2D(tex0, texCoord).r; // Y
        yuv.y = texture2D(tex1, texCoord).r; // U
        yuv.z = texture2D(tex2, texCoord).r; // V

        // color transform
        yuv += offset;

        rgb.r = dot(yuv, Rcoeff);
        rgb.g = dot(yuv, Gcoeff);
        rgb.b = dot(yuv, Bcoeff);

        gl_FragColor = vec4(rgb, 1.0);
    }
)";

// YUYV422
static const QString YUYV_FRAGMENT_SHADER = R"(
    void main(void)
    {
        vec3 yuv, rgb;

        yuv.x = texture2D(tex0, texCoord).r; // Y
        yuv.y = texture2D(tex0, texCoord).g; // U
        yuv.z = texture2D(tex0, texCoord).a; // V

        // color transform
        yuv += offset;

        rgb.r = dot(yuv, Rcoeff);
        rgb.g = dot(yuv, Gcoeff);
        rgb.b = dot(yuv, Bcoeff);

        gl_FragColor = vec4(rgb, 1.0);
    }
)";

// RGB
static const QString RGB_FRAGMENT_SHADER = R"(
    void main(void)
    {
        gl_FragColor = vec4(texture2D(tex0, texCoord).rgb, 1.0);
    }
)";

// BGR
static const QString BGR_FRAGMENT_SHADER = R"(
    void main(void)
    {
        gl_FragColor = vec4(texture2D(tex0, texCoord).bgr, 1.0);
    }
)";

// NV12
static const QString NV12_FRAGMENT_SHADER = R"(
    void main(void)
    {
        vec3 yuv, rgb;

        yuv.x  = texture2D(tex0, texCoord).r;   // Y
        yuv.yz = texture2D(tex1, texCoord).rg;  // U / V

        // color transform
        yuv += offset;

        rgb.r = dot(yuv, Rcoeff);
        rgb.g = dot(yuv, Gcoeff);
        rgb.b = dot(yuv, Bcoeff);

        gl_FragColor = vec4(rgb, 1.0);
    }
)";

// NV21
static const QString NV21_FRAGMENT_SHADER = R"(
    void main(void)
    {
        vec3 yuv, rgb;

        yuv.x  = texture2D(tex0, texCoord).r;   // Y
        yuv.yz = texture2D(tex1, texCoord).gr;  // U / V

        // color transform
        yuv += offset;

        rgb.r = dot(yuv, Rcoeff);
        rgb.g = dot(yuv, Gcoeff);
        rgb.b = dot(yuv, Bcoeff);

        gl_FragColor = vec4(rgb, 1.0);
    }
)";

// ARGB
static const QString ARGB_FRAGMENT_SHADER = R"(
    void main(void)
    {
        gl_FragColor = texture2D(tex0, texCoord).gbar;
    }
)";

// RGBA
static const QString RGBA_FRAGMENT_SHADER = R"(
    void main(void)
    {
        gl_FragColor = texture2D(tex0, texCoord);
    }
)";

// ABGR
static const QString ABGR_FRAGMENT_SHADER = R"(
    void main(void)
    {
        gl_FragColor = texture2D(tex0, texCoord).abgr;
    }
)";

// BGRA
static const QString BGRA_FRAGMENT_SHADER = R"(
    void main(void)
    {
        gl_FragColor = texture2D(tex0, texCoord).bgra;
    }
)";

// 0RGB
static const QString XRGB_FRAGMENT_SHADER = R"(
    void main(void)
    {
        gl_FragColor = vec4(texture2D(tex0, texCoord).gba, 1.0);
    }
)";

// 0BGR
static const QString XBGR_FRAGMENT_SHADER = R"(
    void main(void)
    {
        gl_FragColor = vec4(texture2D(tex0, texCoord).abg, 1.0);
    }
)";

static const std::map<AVPixelFormat, QString> prologues = {
    { AV_PIX_FMT_YUV420P, TEX3_SHADER_PROLOGUE }, { AV_PIX_FMT_YUYV422, TEX1_SHADER_PROLOGUE },
    { AV_PIX_FMT_RGB24, TEX1_SHADER_PROLOGUE },   { AV_PIX_FMT_BGR24, TEX1_SHADER_PROLOGUE },
    { AV_PIX_FMT_YUV422P, TEX3_SHADER_PROLOGUE }, { AV_PIX_FMT_YUV444P, TEX3_SHADER_PROLOGUE },
    { AV_PIX_FMT_YUV410P, TEX3_SHADER_PROLOGUE }, { AV_PIX_FMT_YUV411P, TEX3_SHADER_PROLOGUE },
    { AV_PIX_FMT_BGR8, TEX1_SHADER_PROLOGUE },    { AV_PIX_FMT_RGB8, TEX1_SHADER_PROLOGUE },
    { AV_PIX_FMT_NV12, TEX2_SHADER_PROLOGUE },    { AV_PIX_FMT_NV21, TEX2_SHADER_PROLOGUE },
    { AV_PIX_FMT_ARGB, TEX1_SHADER_PROLOGUE },    { AV_PIX_FMT_RGBA, TEX1_SHADER_PROLOGUE },
    { AV_PIX_FMT_ABGR, TEX1_SHADER_PROLOGUE },    { AV_PIX_FMT_BGRA, TEX1_SHADER_PROLOGUE },
    { AV_PIX_FMT_0RGB, TEX1_SHADER_PROLOGUE },    { AV_PIX_FMT_RGB0, TEX1_SHADER_PROLOGUE },
    { AV_PIX_FMT_0BGR, TEX1_SHADER_PROLOGUE },    { AV_PIX_FMT_BGR0, TEX1_SHADER_PROLOGUE },
};

static const std::map<AVPixelFormat, QString> shaders = {
    { AV_PIX_FMT_YUV420P, YUV_FRAGMENT_SHADER }, { AV_PIX_FMT_YUYV422, YUYV_FRAGMENT_SHADER },
    { AV_PIX_FMT_RGB24, RGB_FRAGMENT_SHADER },   { AV_PIX_FMT_BGR24, BGR_FRAGMENT_SHADER },
    { AV_PIX_FMT_YUV422P, YUV_FRAGMENT_SHADER }, { AV_PIX_FMT_YUV444P, YUV_FRAGMENT_SHADER },
    { AV_PIX_FMT_YUV410P, YUV_FRAGMENT_SHADER }, { AV_PIX_FMT_YUV411P, YUV_FRAGMENT_SHADER },
    { AV_PIX_FMT_BGR8, BGR_FRAGMENT_SHADER },    { AV_PIX_FMT_RGB8, RGB_FRAGMENT_SHADER },
    { AV_PIX_FMT_NV12, NV12_FRAGMENT_SHADER },   { AV_PIX_FMT_NV21, NV21_FRAGMENT_SHADER },
    { AV_PIX_FMT_ARGB, ARGB_FRAGMENT_SHADER },   { AV_PIX_FMT_RGBA, RGBA_FRAGMENT_SHADER },
    { AV_PIX_FMT_ABGR, ABGR_FRAGMENT_SHADER },   { AV_PIX_FMT_BGRA, BGRA_FRAGMENT_SHADER },
    { AV_PIX_FMT_0RGB, XRGB_FRAGMENT_SHADER },   { AV_PIX_FMT_RGB0, RGB_FRAGMENT_SHADER },
    { AV_PIX_FMT_0BGR, XBGR_FRAGMENT_SHADER },   { AV_PIX_FMT_BGR0, BGR_FRAGMENT_SHADER },
};

TextureGLWidget::TextureGLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);

    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_NoSystemBackground);
}

TextureGLWidget ::~TextureGLWidget()
{
    makeCurrent();
    vertex_buffer_.destroy();
    delete_texture();
    doneCurrent();
}

std::vector<AVPixelFormat> TextureGLWidget::formats() const
{
    return {
        AV_PIX_FMT_YUV420P, ///< planar YUV 4:2:0, 12bpp, (1 Cr & Cb sample per 2x2 Y samples)

        AV_PIX_FMT_YUYV422, ///< packed YUV 4:2:2, 16bpp, Y0 Cb Y1 Cr

        AV_PIX_FMT_RGB24,   ///< packed RGB 8:8:8, 24bpp, RGBRGB...
        AV_PIX_FMT_BGR24,   ///< packed RGB 8:8:8, 24bpp, BGRBGR...

        AV_PIX_FMT_YUV422P, ///< planar YUV 4:2:2, 16bpp, (1 Cr & Cb sample per 2x1 Y samples)
        AV_PIX_FMT_YUV444P, ///< planar YUV 4:4:4, 24bpp, (1 Cr & Cb sample per 1x1 Y samples)
        AV_PIX_FMT_YUV410P, ///< planar YUV 4:1:0,  9bpp, (1 Cr & Cb sample per 4x4 Y samples)
        AV_PIX_FMT_YUV411P, ///< planar YUV 4:1:1, 12bpp, (1 Cr & Cb sample per 4x1 Y samples)

        AV_PIX_FMT_BGR8,    ///< packed RGB 3:3:2,  8bpp, (msb)2B 3G 3R(lsb)
        AV_PIX_FMT_RGB8,    ///< packed RGB 3:3:2,  8bpp, (msb)2R 3G 3B(lsb)

        AV_PIX_FMT_NV12,    ///< planar YUV 4:2:0, 12bpp, 1 plane for Y and 1 plane for the UV components,
                            ///< which are interleaved (first byte U and the following byte V)
        AV_PIX_FMT_NV21,    ///< as above, but U and V bytes are swapped

        AV_PIX_FMT_ARGB,    ///< packed ARGB 8:8:8:8, 32bpp, ARGBARGB...
        AV_PIX_FMT_RGBA,    ///< packed RGBA 8:8:8:8, 32bpp, RGBARGBA...
        AV_PIX_FMT_ABGR,    ///< packed ABGR 8:8:8:8, 32bpp, ABGRABGR...
        AV_PIX_FMT_BGRA,    ///< packed BGRA 8:8:8:8, 32bpp, BGRABGRA...

        AV_PIX_FMT_0RGB,    ///< packed RGB 8:8:8, 32bpp, XRGBXRGB...   X=unused/undefined
        AV_PIX_FMT_RGB0,    ///< packed RGB 8:8:8, 32bpp, RGBXRGBX...   X=unused/undefined
        AV_PIX_FMT_0BGR,    ///< packed BGR 8:8:8, 32bpp, XBGRXBGR...   X=unused/undefined
        AV_PIX_FMT_BGR0,    ///< packed BGR 8:8:8, 32bpp, BGRXBGRX...   X=unused/undefined
    };
}

bool TextureGLWidget::isSupported(AVPixelFormat pix_fmt) const
{
    for (const auto& fmt : formats()) {
        if (fmt == pix_fmt) return true;
    }
    return false;
}

int TextureGLWidget::setPixelFormat(AVPixelFormat pix_fmt)
{
    if (format_.pix_fmt == pix_fmt) return 0;

    format_.pix_fmt = pix_fmt;
    reinit_shaders();

    return 0;
}

void TextureGLWidget::present(AVFrame *frame)
{
    std::lock_guard lock(mtx_);

    if (!frame || !frame->data[0] || frame->format == AV_PIX_FMT_NONE) return;

    frame_ = frame;

    update();
}

void TextureGLWidget::update_tex_params()
{

    switch (format_.pix_fmt) {
    case AV_PIX_FMT_YUYV422: tex_params_ = { { 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, 4 } }; break;

    case AV_PIX_FMT_RGB24:
    case AV_PIX_FMT_BGR24: tex_params_ = { { 1, 1, GL_RGB, GL_UNSIGNED_BYTE, 3 } }; break;

    case AV_PIX_FMT_YUV420P:
        tex_params_ = {
            { 1, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
            { 2, 2, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
            { 2, 2, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
        };
        break;

    case AV_PIX_FMT_YUV422P:
        tex_params_ = {
            { 1, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
            { 2, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
            { 2, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
        };
        break;

    case AV_PIX_FMT_YUV444P:
        tex_params_ = {
            { 1, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
            { 1, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
            { 1, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
        };
        break;

    case AV_PIX_FMT_YUV410P:
        tex_params_ = {
            { 1, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
            { 4, 4, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
            { 4, 4, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
        };
        break;

    case AV_PIX_FMT_YUV411P:
        tex_params_ = {
            { 1, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
            { 4, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
            { 4, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
        };
        break;

    case AV_PIX_FMT_BGR8: tex_params_ = { { 1, 1, GL_RGB, GL_UNSIGNED_BYTE_2_3_3_REV, 1 } }; break;

    case AV_PIX_FMT_RGB8: tex_params_ = { { 1, 1, GL_RGB, GL_UNSIGNED_BYTE_3_3_2, 1 } }; break;

    case AV_PIX_FMT_NV12:
    case AV_PIX_FMT_NV21:
        tex_params_ = {
            { 1, 1, GL_RED, GL_UNSIGNED_BYTE, 1 },
            { 2, 2, GL_RG, GL_UNSIGNED_BYTE, 2 },
        };
        break;

    case AV_PIX_FMT_ARGB:
    case AV_PIX_FMT_RGBA:
    case AV_PIX_FMT_ABGR:
    case AV_PIX_FMT_BGRA:
    case AV_PIX_FMT_0RGB:
    case AV_PIX_FMT_RGB0:
    case AV_PIX_FMT_0BGR:
    case AV_PIX_FMT_BGR0: tex_params_ = { { 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 4 } }; break;
    default: LOG(ERROR) << "unsuppored pixel format"; break;
    }
}

void TextureGLWidget::reinit_shaders()
{
    makeCurrent();

    program_.removeAllShaders();

    // TODO: color space
    program_.addShaderFromSourceCode(QOpenGLShader::Vertex, TEXTURE_VERTEX_SHADER);
    program_.addShaderFromSourceCode(QOpenGLShader::Fragment, BT709_SHADER_CONSTANTS +
                                                                  prologues.at(format_.pix_fmt) +
                                                                  shaders.at(format_.pix_fmt));
    program_.link();

    doneCurrent();
}

void TextureGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    vertex_buffer_.create();
    vertex_buffer_.bind();
    vertex_buffer_.allocate(coordinate, sizeof(coordinate));

    program_.addShaderFromSourceCode(QOpenGLShader::Vertex, TEXTURE_VERTEX_SHADER);
    program_.addShaderFromSourceCode(QOpenGLShader::Fragment, BT709_SHADER_CONSTANTS +
                                                                  prologues.at(format_.pix_fmt) +
                                                                  shaders.at(format_.pix_fmt));
    program_.link();
    program_.bind();

    // vertex & texture
    program_.setAttributeBuffer("vertex", GL_FLOAT, 0, 3, 3 * sizeof(float));
    program_.setAttributeBuffer("texture", GL_FLOAT, 12 * sizeof(float), 2, 2 * sizeof(float));
    program_.enableAttributeArray("vertex");
    program_.enableAttributeArray("texture");
}

void TextureGLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    repaint();
}

void TextureGLWidget::paintGL()
{
    //
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    std::lock_guard lock(mtx_);

    if (!frame_->data[0]) return;

    if (format_.width != frame_->width || format_.height != frame_->height) {
        format_.width  = frame_->width;
        format_.height = frame_->height;

        delete_texture();
        create_texture();
    }

    //
    for (size_t idx = 0; idx < tex_params_.size(); ++idx) {
        const auto& [wd, hd, fmt, dt, bytes] = tex_params_[idx];

        glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + idx));
        glBindTexture(GL_TEXTURE_2D, texture_[idx]);

        glPixelStorei(GL_UNPACK_ROW_LENGTH, frame_->linesize[idx] / bytes); // format_.width + padding
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, format_.width / wd, format_.height / hd, 0, fmt, dt,
                     frame_->data[idx]);
    }

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void TextureGLWidget::create_texture()
{
    update_tex_params();

    glGenTextures(static_cast<GLsizei>(tex_params_.size()), texture_);

    for (size_t idx = 0; idx < tex_params_.size(); ++idx) {
        program_.setUniformValue(fmt::format("tex{}", idx).c_str(), static_cast<GLuint>(idx));

        glBindTexture(GL_TEXTURE_2D, texture_[idx]);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
}

void TextureGLWidget::delete_texture()
{
    if (QOpenGLFunctions::isInitialized(QOpenGLFunctions::d_ptr)) {
        glDeleteTextures(3, texture_);
    }

    memset(texture_, 0, sizeof(texture_));
}