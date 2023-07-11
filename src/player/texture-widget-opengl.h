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

    std::vector<AVPixelFormat> formats() const;

    bool isSupported(AVPixelFormat) const;

    int setPixelFormat(AVPixelFormat pix_fmt);

    AVPixelFormat format() const { return format_.pix_fmt; }

protected:
    virtual void initializeGL();
    virtual void resizeGL(int w, int h);
    virtual void paintGL();

private:
    void create_texture();
    void delete_texture();
    void reinit_shaders();
    void update_tex_params();

    // shader program
    QOpenGLShaderProgram program_{};

    // vertex buffer object
    QOpenGLBuffer vertex_buffer_{};

    // YUV texture
    GLuint texture_[4]{};

    av::vformat_t format_{ .pix_fmt = AV_PIX_FMT_YUV420P };

    // width divisor, height divisor, pixel format, data type, pixel bytes
    std::vector<std::tuple<unsigned int, unsigned int, GLenum, GLenum, GLuint>> tex_params_{};

    // frame
    av::frame frame_{};

    std::mutex mtx_;
};

#endif // !CAPTURER_TEXTURE_WIDGET_OPENGL_H
