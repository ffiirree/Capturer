#ifndef CAPTURER_WEBVIEW_H
#define CAPTURER_WEBVIEW_H

#include <QWidget>
#include <QWebEngineView>
#include <QWebEnginePage>
#include <QWebEngineSettings>

// Html to Image
class WebView : public QWebEngineView {
    Q_OBJECT
public:
    explicit WebView(QWidget *parent = nullptr)
        : QWebEngineView(parent)
    {
        setAttribute(Qt::WA_DontShowOnScreen);
        setAttribute(Qt::WA_DeleteOnClose);

// 'contentsSizeChanged' was introduced in Qt 5.7.
#if (QT_VERSION > 0x050700)
        connect(page(), &QWebEnginePage::contentsSizeChanged, [this](const QSizeF &size){
            emit contentsSizeChanged(size.toSize());
        });
#endif

        // font size
        QWebEngineSettings *defaultSettings = QWebEngineSettings::globalSettings();
        defaultSettings->setFontSize(QWebEngineSettings::MinimumFontSize,16);

        resize({ 1, 1 });
    }
    virtual ~WebView() override = default;

signals:
#if (QT_VERSION > 0x050700)
    void contentsSizeChanged(const QSize &size);
#endif
};

#endif // CAPTURER_WEBVIEW_H
