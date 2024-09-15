#ifndef CAPTURER_TEXTURE_WIDGET_OPENGL_H
#define CAPTURER_TEXTURE_WIDGET_OPENGL_H

#include "libcap/ffmpeg-wrapper.h"
#include "libcap/media.h"

#include <memory>
#include <mutex>
#include <QOpenGLFunctions_4_4_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <tuple>
#include <vector>

#ifdef QT_DEBUG
class QOpenGLDebugLogger;
#endif

class TextureGLWidget final : public QOpenGLWidget, protected QOpenGLFunctions_4_4_Core
{
    Q_OBJECT
public:
    explicit TextureGLWidget(QWidget *parent = nullptr);

    ~TextureGLWidget() override;

    void present(const av::frame& frame);

    static std::vector<AVPixelFormat> pix_fmts();

    [[nodiscard]] bool isSupported(AVPixelFormat) const;

    int setFormat(const av::vformat_t& vfmt);

    av::vformat_t format() const { return format_; }

    [[nodiscard]] AVPixelFormat pix_fmt() const { return format_.pix_fmt; }

    [[nodiscard]] AVRational SAR() const;
    [[nodiscard]] AVRational DAR() const;

signals:
    void updateRequest();

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

private:
    bool UpdateTextureParams();

    void CreateTextures();
    void DeleteTextures();

    void cleanup();

    // shader program
    QOpenGLShaderProgram *program_{};

    // vertex buffer objects
    GLuint vao_{}; // vertex array object
    GLuint vbo_{}; // vertex buffer object

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

#ifdef QT_DEBUG
    QOpenGLDebugLogger *debugger_{};
#endif
};

#endif // !CAPTURER_TEXTURE_WIDGET_OPENGL_H
