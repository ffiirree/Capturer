#ifndef CAPTURER_TEXTURE_WIDGET_OPENGL_H
#define CAPTURER_TEXTURE_WIDGET_OPENGL_H

#include <libcap/media.h>
#include <memory>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>
#include <tuple>
#include <vector>

class TextureGLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit TextureGLWidget(QWidget *parent = nullptr);
    ~TextureGLWidget();

    void present(AVFrame *frame);

    std::vector<AVPixelFormat> pix_fmts() const;

    bool isSupported(AVPixelFormat) const;

    int setFormat(const av::vformat_t& vfmt);

    av::vformat_t format() const { return format_; }

    AVPixelFormat pix_fmt() const { return format_.pix_fmt; }

    AVRational SAR() const;
    AVRational DAR() const;

protected:
    virtual void initializeGL();
    virtual void resizeGL(int w, int h);
    virtual void paintGL();

private:
    void create_textures();
    void delete_textures();
    void reinit_shaders();
    bool update_tex_params();

    // shader program
    QOpenGLShaderProgram program_{};

    // vertex buffer objectss
    QOpenGLBuffer vbo_{ QOpenGLBuffer::VertexBuffer };

    // YUV texture
    GLuint texture_[4]{};

    // ModelViewProjectionMatrix
    GLuint proj_id_{};
    QMatrix4x4 proj_{};

    av::vformat_t format_{ .pix_fmt = AV_PIX_FMT_YUV420P };

    // width divisor, height divisor, pixel format, data type, pixel bytes
    std::vector<std::tuple<unsigned int, unsigned int, GLenum, GLenum, GLuint>> tex_params_{};

    // frame
    av::frame frame_{};

    std::mutex mtx_;
};

#endif // !CAPTURER_TEXTURE_WIDGET_OPENGL_H
