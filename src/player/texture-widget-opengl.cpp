#include "texture-widget-opengl.h"

#include "logging.h"

#include <fmt/format.h>
#include <map>
#include <QCoreApplication>

// clang-format off
// normalized device coordinates
static constexpr GLfloat vertices[] = {
  // vertex coordinate: [-1.0, 1.0]       / texture coordinate: [0.0, 1.0]
  //  x      y     z                       / x     y
    -1.0f, -1.0f, 0.0f, /* bottom left  */  0.0f, 1.0f, /* top    left  */
    +1.0f, -1.0f, 0.0f, /* bottom right */  1.0f, 1.0f, /* top    right */
    -1.0f, +1.0f, 0.0f, /* top    left  */  0.0f, 0.0f, /* bottom left  */
    +1.0f, +1.0f, 0.0f, /* top    right */  1.0f, 0.0f, /* bottom right */ 
};
// clang-format on

// https://github.com/libsdl-org/SDL/blob/main/src/render/opengl/SDL_shaders_gl.c
static const QString TEXTURE_VERTEX_SHADER = R"(
    #version 330 core

    layout (location = 0) in vec3 vtx;
    layout (location = 1) in vec2 tex;

    uniform mat4 ProjM;

    out vec2 texCoord;

    void main(void)
    {
        gl_Position = ProjM * vec4(vtx, 1.0);
        texCoord = tex;
    }
)";

// FULL: [0, 255]
// TV  : Y [16, 235], Cb & Cr [16, 240]
static const QString BT601_TV_SHADER_CONSTANTS = R"( 
    // BT.601 PAL, TV

    // range offset
    const vec3 OFFSET = vec3(-0.0627451017, -0.501960814, -0.501960814);

    // convert matrix
    const mat3 CM = mat3(
        1.1643835616,  1.1643835616, 1.1643835616,
        0.0000000000, -0.3917622901, 2.0172321429,
        1.5960267857, -0.8129676472, 0.0000000000
    );
)";

static const QString BT709_TV_SHADER_CONSTANTS = R"(
    // BT.709 & sRGB, TV

    // range offset
    const vec3 OFFSET = vec3(-0.0627451017, -0.501960814, -0.501960814);

    // convert matrix
    const mat3 CM = mat3(
        1.1643835616,  1.1643835616, 1.1643835616,
        0.0000000000, -0.2132486143, 2.1124017857,
        1.7927410714, -0.5329093286, 0.0000000000
    );
)";

static const QString BT2020_TV_SHADER_CONSTANTS = R"(
    // BT.2020, TV

    // range offset
    const vec3 OFFSET = vec3(-0.0627451017, -0.501960814, -0.501960814);

    // convert matrix
    const mat3 CM = mat3(
        1.1643835616,  1.1643835616, 1.1643835616,
        0.0000000000, -0.1873261042, 2.1417723214,
        1.6786741071, -0.6504243185, 0.0000000000
    );
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
        vec3 yuv;

        yuv.x = texture2D(tex0, texCoord).r; // Y
        yuv.y = texture2D(tex1, texCoord).r; // U
        yuv.z = texture2D(tex2, texCoord).r; // V

        gl_FragColor = vec4(CM * (yuv + OFFSET), 1.0);
    }
)";

// YUV 10LE
static const QString YUV_10LE_FRAGMENT_SHADER = R"(
    void main(void)
    {
        vec3 yuv0, yuv1, yuv;

        yuv0.x = texture2D(tex0, texCoord).r; // Y
        yuv1.x = texture2D(tex0, texCoord).a; // Y
        yuv0.y = texture2D(tex1, texCoord).r; // U
        yuv1.y = texture2D(tex1, texCoord).a; // U
        yuv0.z = texture2D(tex2, texCoord).r; // V
        yuv1.z = texture2D(tex2, texCoord).a; // V

        yuv = (yuv0 * 255.0 + yuv1 * 255.0 * 256.0) / (1023.0);

        gl_FragColor = vec4(CM * (yuv + OFFSET), 1.0);
    }
)";

// YUYV422
static const QString YUYV_FRAGMENT_SHADER = R"(
    void main(void)
    {
        vec3 yuv;

        yuv.x = texture2D(tex0, texCoord).r; // Y
        yuv.y = texture2D(tex0, texCoord).g; // U
        yuv.z = texture2D(tex0, texCoord).a; // V

        gl_FragColor = vec4(CM * (yuv + OFFSET), 1.0);
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
        vec3 yuv;

        yuv.x  = texture2D(tex0, texCoord).r;   // Y
        yuv.yz = texture2D(tex1, texCoord).rg;  // U / V

        gl_FragColor = vec4(CM * (yuv + OFFSET), 1.0);
    }
)";

// NV21
static const QString NV21_FRAGMENT_SHADER = R"(
    void main(void)
    {
        vec3 yuv;

        yuv.x  = texture2D(tex0, texCoord).r;   // Y
        yuv.yz = texture2D(tex1, texCoord).gr;  // U / V

        gl_FragColor = vec4(CM * (yuv + OFFSET), 1.0);
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

static const std::map<AVColorSpace, QString> SHADER_CONSTANTS = {
    { AVCOL_SPC_RGB, "" },                                // RGB
    { AVCOL_SPC_BT709, BT709_TV_SHADER_CONSTANTS },       // BT.709 & sRGB
    { AVCOL_SPC_UNSPECIFIED, BT709_TV_SHADER_CONSTANTS },
    { AVCOL_SPC_BT470BG, BT601_TV_SHADER_CONSTANTS },     // BT.601 PAL
    { AVCOL_SPC_BT2020_NCL, BT2020_TV_SHADER_CONSTANTS }, // BT.2020
};

static const std::map<AVPixelFormat, QString> PROLOGUES = {
    { AV_PIX_FMT_YUV420P, TEX3_SHADER_PROLOGUE },     { AV_PIX_FMT_YUYV422, TEX1_SHADER_PROLOGUE },
    { AV_PIX_FMT_RGB24, TEX1_SHADER_PROLOGUE },       { AV_PIX_FMT_BGR24, TEX1_SHADER_PROLOGUE },
    { AV_PIX_FMT_YUV422P, TEX3_SHADER_PROLOGUE },     { AV_PIX_FMT_YUV444P, TEX3_SHADER_PROLOGUE },
    { AV_PIX_FMT_YUV410P, TEX3_SHADER_PROLOGUE },     { AV_PIX_FMT_YUV411P, TEX3_SHADER_PROLOGUE },
    { AV_PIX_FMT_BGR8, TEX1_SHADER_PROLOGUE },        { AV_PIX_FMT_RGB8, TEX1_SHADER_PROLOGUE },
    { AV_PIX_FMT_NV12, TEX2_SHADER_PROLOGUE },        { AV_PIX_FMT_NV21, TEX2_SHADER_PROLOGUE },
    { AV_PIX_FMT_ARGB, TEX1_SHADER_PROLOGUE },        { AV_PIX_FMT_RGBA, TEX1_SHADER_PROLOGUE },
    { AV_PIX_FMT_ABGR, TEX1_SHADER_PROLOGUE },        { AV_PIX_FMT_BGRA, TEX1_SHADER_PROLOGUE },
    { AV_PIX_FMT_0RGB, TEX1_SHADER_PROLOGUE },        { AV_PIX_FMT_RGB0, TEX1_SHADER_PROLOGUE },
    { AV_PIX_FMT_0BGR, TEX1_SHADER_PROLOGUE },        { AV_PIX_FMT_BGR0, TEX1_SHADER_PROLOGUE },
    { AV_PIX_FMT_YUV420P10LE, TEX3_SHADER_PROLOGUE },
};

static const std::map<AVPixelFormat, QString> SHADERS = {
    { AV_PIX_FMT_YUV420P, YUV_FRAGMENT_SHADER },
    { AV_PIX_FMT_YUYV422, YUYV_FRAGMENT_SHADER },
    { AV_PIX_FMT_RGB24, RGB_FRAGMENT_SHADER },
    { AV_PIX_FMT_BGR24, BGR_FRAGMENT_SHADER },
    { AV_PIX_FMT_YUV422P, YUV_FRAGMENT_SHADER },
    { AV_PIX_FMT_YUV444P, YUV_FRAGMENT_SHADER },
    { AV_PIX_FMT_YUV410P, YUV_FRAGMENT_SHADER },
    { AV_PIX_FMT_YUV411P, YUV_FRAGMENT_SHADER },
    { AV_PIX_FMT_BGR8, BGR_FRAGMENT_SHADER },
    { AV_PIX_FMT_RGB8, RGB_FRAGMENT_SHADER },
    { AV_PIX_FMT_NV12, NV12_FRAGMENT_SHADER },
    { AV_PIX_FMT_NV21, NV21_FRAGMENT_SHADER },
    { AV_PIX_FMT_ARGB, ARGB_FRAGMENT_SHADER },
    { AV_PIX_FMT_RGBA, RGBA_FRAGMENT_SHADER },
    { AV_PIX_FMT_ABGR, ABGR_FRAGMENT_SHADER },
    { AV_PIX_FMT_BGRA, BGRA_FRAGMENT_SHADER },
    { AV_PIX_FMT_0RGB, XRGB_FRAGMENT_SHADER },
    { AV_PIX_FMT_RGB0, RGB_FRAGMENT_SHADER },
    { AV_PIX_FMT_0BGR, XBGR_FRAGMENT_SHADER },
    { AV_PIX_FMT_BGR0, BGR_FRAGMENT_SHADER },
    { AV_PIX_FMT_YUV420P10LE, YUV_10LE_FRAGMENT_SHADER },
};

static QString GENERATOR_FRAGMENT_SHADER(const av::vformat_t& fmt)
{
    return SHADER_CONSTANTS.at(fmt.color.space) + PROLOGUES.at(fmt.pix_fmt) + SHADERS.at(fmt.pix_fmt);
}

///

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

    vbo_.destroy();
    DeleteTextures();

    doneCurrent();
}

std::vector<AVPixelFormat> TextureGLWidget::pix_fmts() const
{
    // clang-format off
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

        AV_PIX_FMT_NV12,    ///< planar YUV 4:2:0, 12bpp, 1 plane for Y and 1 plane for the UV components, which are interleaved (first byte U and the following byte V)
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
    };
    // clang-format on
}

bool TextureGLWidget::isSupported(AVPixelFormat pix_fmt) const
{
    for (const auto& fmt : pix_fmts()) {
        if (fmt == pix_fmt) return true;
    }
    return false;
}

int TextureGLWidget::setFormat(const av::vformat_t& vfmt)
{
    if (!isSupported(vfmt.pix_fmt)) return -1;

    format_       = vfmt;
    config_dirty_ = true;

    return 0;
}

AVRational TextureGLWidget::SAR() const
{
    if (format_.sample_aspect_ratio.num > 0 && format_.sample_aspect_ratio.den > 0)
        return format_.sample_aspect_ratio;

    return { 1, 1 };
}

AVRational TextureGLWidget::DAR() const
{
    if (!format_.width || !format_.height) return { 0, 0 };

    auto       sar = SAR();
    AVRational dar{};
    av_reduce(&dar.num, &dar.den, static_cast<int64_t>(format_.width) * sar.num,
              static_cast<int64_t>(format_.height) * sar.den, 1024 * 1024);

    return dar;
}

bool TextureGLWidget::UpdateTextureParams()
{
    switch (format_.pix_fmt) {
    case AV_PIX_FMT_YUYV422: tex_params_ = { { 2, 1, GL_RGBA, GL_UNSIGNED_BYTE, 4 } }; return true;

    case AV_PIX_FMT_RGB24:
    case AV_PIX_FMT_BGR24:   tex_params_ = { { 1, 1, GL_RGB, GL_UNSIGNED_BYTE, 3 } }; return true;

    case AV_PIX_FMT_YUV420P:
        tex_params_ = {
            { 1, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
            { 2, 2, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
            { 2, 2, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
        };
        return true;

    case AV_PIX_FMT_YUV422P:
        tex_params_ = {
            { 1, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
            { 2, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
            { 2, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
        };
        return true;

    case AV_PIX_FMT_YUV444P:
        tex_params_ = {
            { 1, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
            { 1, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
            { 1, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
        };
        return true;

    case AV_PIX_FMT_YUV410P:
        tex_params_ = {
            { 1, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
            { 4, 4, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
            { 4, 4, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
        };
        return true;

    case AV_PIX_FMT_YUV411P:
        tex_params_ = {
            { 1, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
            { 4, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
            { 4, 1, GL_LUMINANCE, GL_UNSIGNED_BYTE, 1 },
        };
        return true;

    case AV_PIX_FMT_BGR8: tex_params_ = { { 1, 1, GL_RGB, GL_UNSIGNED_BYTE_2_3_3_REV, 1 } }; return true;

    case AV_PIX_FMT_RGB8: tex_params_ = { { 1, 1, GL_RGB, GL_UNSIGNED_BYTE_3_3_2, 1 } }; return true;

    case AV_PIX_FMT_NV12:
    case AV_PIX_FMT_NV21:
        tex_params_ = {
            { 1, 1, GL_RED, GL_UNSIGNED_BYTE, 1 },
            { 2, 2, GL_RG, GL_UNSIGNED_BYTE, 2 },
        };
        return true;

    case AV_PIX_FMT_ARGB:
    case AV_PIX_FMT_RGBA:
    case AV_PIX_FMT_ABGR:
    case AV_PIX_FMT_BGRA:
    case AV_PIX_FMT_0RGB:
    case AV_PIX_FMT_RGB0:
    case AV_PIX_FMT_0BGR:
    case AV_PIX_FMT_BGR0: tex_params_ = { { 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, 4 } }; return true;

    case AV_PIX_FMT_YUV420P10LE:
        tex_params_ = {
            { 1, 1, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, 2 },
            { 2, 2, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, 2 },
            { 2, 2, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, 2 },
        };
        return true;
    default: break;
    }

    LOG(ERROR) << "unsuppored pixel format: " << av::to_string(format_.pix_fmt);
    return false;
}

void TextureGLWidget::CreateTextures()
{
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

void TextureGLWidget::DeleteTextures()
{
    if (QOpenGLFunctions::isInitialized(QOpenGLFunctions::d_ptr)) {
        glDeleteTextures(3, texture_);
    }

    memset(texture_, 0, sizeof(texture_));
}

void TextureGLWidget::RemoveAllShaders() { program_.removeAllShaders(); }

void TextureGLWidget::InitializeShaders()
{
    program_.addShaderFromSourceCode(QOpenGLShader::Vertex, TEXTURE_VERTEX_SHADER);
    program_.addShaderFromSourceCode(QOpenGLShader::Fragment, GENERATOR_FRAGMENT_SHADER(format_));
    program_.link();
}

void TextureGLWidget::initializeGL()
{
    initializeOpenGLFunctions();

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDisable(GL_DEPTH_TEST);

    // vertex buffer object
    vbo_.create();
    vbo_.bind();
    vbo_.allocate(vertices, sizeof(vertices));

    // shader program
    InitializeShaders();
    program_.bind();

    // vertex & texture
    program_.setAttributeBuffer(0, GL_FLOAT, 0, 3, 5 * sizeof(float));
    program_.setAttributeBuffer(1, GL_FLOAT, 3 * sizeof(float), 2, 5 * sizeof(float));
    program_.enableAttributeArray(0);
    program_.enableAttributeArray(1);

    // matrix
    proj_id_ = program_.uniformLocation("ProjM");

    // create textures
    UpdateTextureParams();
    CreateTextures();

    config_dirty_ = false;
}

void TextureGLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);

    // keep aspect ratio
    proj_.setToIdentity();
    if (auto dar = DAR(); dar.num && dar.den) {
        auto size = QSizeF(dar.num, dar.den).scaled(w, h, Qt::KeepAspectRatio);
        proj_.scale(size.width() / w, size.height() / h);
    }
    program_.setUniformValue(proj_id_, proj_);

    repaint();
}

void TextureGLWidget::paintGL()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    std::lock_guard lock(mtx_);

    if (!frame_->data[0] || frame_->height <= 0 || frame_->width <= 0) {
        LOG(WARNING) << "invalid painting frame.";
        return;
    }

    // reconfigure
    if (config_dirty_) {
        UpdateTextureParams();

        RemoveAllShaders();
        InitializeShaders();

        DeleteTextures();
        CreateTextures();

        config_dirty_ = false;
    }

    // TODO: fixme, show black screen after hiding the window
    program_.setUniformValue(proj_id_, proj_);

    // update textures
    for (size_t idx = 0; idx < tex_params_.size(); ++idx) {
        const auto& [wd, hd, fmt, dtype, bytes] = tex_params_[idx];

        glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + idx));
        glBindTexture(GL_TEXTURE_2D, texture_[idx]);

        glPixelStorei(GL_UNPACK_ROW_LENGTH, frame_->linesize[idx] / bytes); // format_.width + padding
        glTexImage2D(GL_TEXTURE_2D, 0, fmt, format_.width / wd, format_.height / hd, 0, fmt, dtype,
                     frame_->data[idx]);
    }

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void TextureGLWidget::present(const av::frame& frame)
{
    if (!frame || !frame->data[0] || frame->width <= 0 || frame->height <= 0 ||
        frame->format == AV_PIX_FMT_NONE) {
        LOG(WARNING) << "invalid frame.";
        return;
    }

    std::lock_guard lock(mtx_);
    frame_ = frame;

    if ((format_.width != frame_->width || format_.height != frame_->height) ||
        (format_.pix_fmt != frame_->format) ||
        (format_.color.space != frame_->colorspace || format_.color.range != frame_->color_range)) {

        format_.width               = frame_->width;
        format_.height              = frame_->height;
        format_.pix_fmt             = static_cast<AVPixelFormat>(frame_->format);
        format_.sample_aspect_ratio = frame_->sample_aspect_ratio;

        format_.color = {
            frame_->colorspace,
            frame_->color_range,
            frame_->color_primaries,
            frame_->color_trc,
        };

        config_dirty_ = true;
    }

    update();
}