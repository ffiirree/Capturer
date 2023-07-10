#include "texture-widget-opengl.h"

#include <fmt/format.h>
#include <QCoreApplication>

// clang-format off
static constexpr GLfloat coordinate[] = {
  //  x      y     z
    -1.0f, -1.0f, 0.0f, 
    +1.0f, -1.0f, 0.0f, 
    -1.0f, +1.0f, 0.0f, 
    +1.0f, +1.0f, 0.0f, 
    
  // x     y
    0.0f, 1.0f, 
    1.0f, 1.0f, 
    0.0f, 0.0f, 
    1.0f, 0.0f,
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

static const QString YUV_SHADER_PROLOGUE = R"(
    varying vec2 texCoord;

    uniform sampler2D tex0; // Y
    uniform sampler2D tex1; // U
    uniform sampler2D tex2; // V
)";

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

static const QString NV12_SHADER_PROLOGUE = R"(
    varying vec2 texCoord;

    uniform sampler2D tex0; // Y
    uniform sampler2D tex1; // U/V
)";

static const QString NV12_RA_FRAGMENT_SHADER = R"(
    void main(void)
    {
        vec3 yuv, rgb;
        
        yuv.x  = texture2D(tex0, texCoord).r;   // Y
        yuv.yz = texture2D(tex1, texCoord).ra;  // U / V

        // color transform
        yuv += offset;

        rgb.r = dot(yuv, Rcoeff);
        rgb.g = dot(yuv, Gcoeff);
        rgb.b = dot(yuv, Bcoeff);

        gl_FragColor = vec4(rgb, 1.0);
    }
)";

static const QString NV12_RG_FRAGMENT_SHADER = R"(
    void main(void)
    {
        vec3 yuv, rgb;

        yuv.x  = texture2D(tex0, texCoord).r;   // Y
        yuv.yz = texture2D(tex1, texCoord).rg;  // U/ V

        // color transform
        yuv += offset;

        rgb.r = dot(yuv, Rcoeff);
        rgb.g = dot(yuv, Gcoeff);
        rgb.b = dot(yuv, Bcoeff);

        gl_FragColor = vec4(rgb, 1.0);
    }
)";

// clang-format off
static const QString JPEG_YUV_SHADER        = JPEG_SHADER_CONSTANTS  + YUV_SHADER_PROLOGUE + YUV_FRAGMENT_SHADER;
static const QString BT601_YUV_SHADER       = BT601_SHADER_CONSTANTS + YUV_SHADER_PROLOGUE + YUV_FRAGMENT_SHADER;
static const QString BT709_YUV_SHADER       = BT709_SHADER_CONSTANTS + YUV_SHADER_PROLOGUE + YUV_FRAGMENT_SHADER;

static const QString JPEG_NV12_RA_SHADER    = JPEG_SHADER_CONSTANTS  + NV12_SHADER_PROLOGUE + NV12_RA_FRAGMENT_SHADER;
static const QString BT601_NV12_RA_SHADER   = BT601_SHADER_CONSTANTS + NV12_SHADER_PROLOGUE + NV12_RA_FRAGMENT_SHADER;
static const QString BT709_NV12_RA_SHADER   = BT709_SHADER_CONSTANTS + NV12_SHADER_PROLOGUE + NV12_RA_FRAGMENT_SHADER;

static const QString JPEG_NV12_RG_SHADER    = JPEG_SHADER_CONSTANTS  + NV12_SHADER_PROLOGUE + NV12_RG_FRAGMENT_SHADER;
static const QString BT601_NV12_RG_SHADER   = BT601_SHADER_CONSTANTS + NV12_SHADER_PROLOGUE + NV12_RG_FRAGMENT_SHADER;
static const QString BT709_NV12_RG_SHADER   = BT709_SHADER_CONSTANTS + NV12_SHADER_PROLOGUE + NV12_RG_FRAGMENT_SHADER;
// clang-format on

TextureGLWidget::TextureGLWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
}

TextureGLWidget ::~TextureGLWidget()
{
    makeCurrent();
    buffer_.destroy();
    delete_texture();
    doneCurrent();
}

void TextureGLWidget::present(AVFrame *frame)
{
    std::lock_guard lock(mtx_);

    if (!frame->data[0] || frame->format != AV_PIX_FMT_YUV420P) return;

    frame_ = frame;

    update();
}

void TextureGLWidget::initializeGL()
{
    initializeOpenGLFunctions();
    glDisable(GL_DEPTH_TEST);

    buffer_.create();
    buffer_.bind();
    buffer_.allocate(coordinate, sizeof(coordinate));

    program_.addShaderFromSourceCode(QOpenGLShader::Vertex, TEXTURE_VERTEX_SHADER);
    program_.addShaderFromSourceCode(QOpenGLShader::Fragment, BT709_YUV_SHADER);
    program_.link();
    program_.bind();

    // vertex
    program_.setAttributeBuffer("vertex", GL_FLOAT, 0, 3, 3 * sizeof(float));
    program_.enableAttributeArray("vertex");

    // texture
    program_.setAttributeBuffer("texture", GL_FLOAT, 12 * sizeof(float), 2, 2 * sizeof(float));
    program_.enableAttributeArray("texture");

    //
    uniform_[0] = program_.uniformLocation("tex0");
    uniform_[1] = program_.uniformLocation("tex1");
    uniform_[2] = program_.uniformLocation("tex2");

    // clear background
    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);
}

void TextureGLWidget::resizeGL(int w, int h)
{
    glViewport(0, 0, w, h);
    repaint();
}

void TextureGLWidget::paintGL()
{
    std::lock_guard lock(mtx_);

    if (frame_size_ != QSize{ frame_->width, frame_->height }) {
        frame_size_ = { frame_->width, frame_->height };
        delete_texture();
        create_texture();
    }

    //
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    //
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture_[0]);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLuint>(frame_->linesize[0]));
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame_size_.width(), frame_size_.height(), GL_LUMINANCE,
                    GL_UNSIGNED_BYTE, frame_->data[0]);
    glUniform1i(uniform_[0], 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, texture_[1]);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLuint>(frame_->linesize[1]));
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame_size_.width() / 2, frame_size_.height() / 2, GL_LUMINANCE,
                    GL_UNSIGNED_BYTE, frame_->data[1]);
    glUniform1i(uniform_[1], 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, texture_[2]);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLuint>(frame_->linesize[2]));
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, frame_size_.width() / 2, frame_size_.height() / 2, GL_LUMINANCE,
                    GL_UNSIGNED_BYTE, frame_->data[2]);
    glUniform1i(uniform_[2], 2);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void TextureGLWidget::create_texture()
{
    // texture Y
    glGenTextures(1, &texture_[0]);
    glBindTexture(GL_TEXTURE_2D, texture_[0]);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, frame_size_.width(), frame_size_.height(), 0, GL_LUMINANCE,
                 GL_UNSIGNED_BYTE, nullptr);

    // texture U
    glGenTextures(1, &texture_[1]);
    glBindTexture(GL_TEXTURE_2D, texture_[1]);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, frame_size_.width() / 2, frame_size_.height() / 2, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, nullptr);

    // texture V
    glGenTextures(1, &texture_[2]);
    glBindTexture(GL_TEXTURE_2D, texture_[2]);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, frame_size_.width() / 2, frame_size_.height() / 2, 0,
                 GL_LUMINANCE, GL_UNSIGNED_BYTE, nullptr);
}

void TextureGLWidget::delete_texture()
{
    if (QOpenGLFunctions::isInitialized(QOpenGLFunctions::d_ptr)) {
        glDeleteTextures(3, texture_);
    }

    memset(texture_, 0, sizeof(texture_));
}