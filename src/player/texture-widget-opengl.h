#ifndef CAPTURER_TEXTURE_WIDGET_OPENGL_H
#define CAPTURER_TEXTURE_WIDGET_OPENGL_H

#include <libcap/media.h>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLWidget>

class TextureGLWidget : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit TextureGLWidget(QWidget *parent = nullptr);
    ~TextureGLWidget();

    void present(AVFrame *frame);

protected:
    virtual void initializeGL();
    virtual void resizeGL(int w, int h);
    virtual void paintGL();

private:
    void create_texture();
    void delete_texture();

    // shader program
    QOpenGLShaderProgram program_{};

    // vertex buffer object
    QOpenGLBuffer buffer_{};

    // YUV texture
    GLuint texture_[3]{};

    // uniform location
    GLuint uniform_[3]{};

    // frame size
    QSize frame_size_{ 0, 0 };

    // frame
    av::frame frame_{};

    std::mutex mtx_;
};

#endif // !CAPTURER_TEXTURE_WIDGET_OPENGL_H
