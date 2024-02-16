#ifndef CAPTURER_TEXTURE_WIDGET_OPENGL_H
#define CAPTURER_TEXTURE_WIDGET_OPENGL_H

#include <libcap/ffmpeg-wrapper.h>
#include <libcap/media.h>
#include <memory>
#include <mutex>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <tuple>
#include <vector>

class TextureGLWidget final : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit TextureGLWidget(QWidget *parent = nullptr);

    ~TextureGLWidget() override;

    void present(const av::frame& frame);

    std::vector<AVPixelFormat> pix_fmts() const;

    bool isSupported(AVPixelFormat) const;

    int setFormat(const av::vformat_t& vfmt);

    av::vformat_t format() const { return format_; }

    AVPixelFormat pix_fmt() const { return format_.pix_fmt; }

    AVRational SAR() const;
    AVRational DAR() const;

signals:
    void arrived();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    bool UpdateTextureParams();

    void CreateTextures();
    void DeleteTextures();

    void RemoveAllShaders();
    void InitializeShaders();

    // shader program
    QOpenGLShaderProgram program_{};

    // vertex buffer objectss
    QOpenGLBuffer vbo_{ QOpenGLBuffer::VertexBuffer };

    // YUV texture
    GLuint texture_[4]{};

    // ModelViewProjectionMatrix
    GLuint     proj_id_{};
    QMatrix4x4 proj_{};

    av::vformat_t format_{ .pix_fmt = AV_PIX_FMT_YUV420P };

    // width divisor, height divisor, pixel format, data type, pixel bytes
    std::vector<std::tuple<unsigned int, unsigned int, GLenum, GLenum, GLuint>> tex_params_{};

    // frame
    av::frame frame_{};

    std::mutex mtx_;

    std::atomic<bool> config_dirty_{ true };
};

#endif // !CAPTURER_TEXTURE_WIDGET_OPENGL_H
