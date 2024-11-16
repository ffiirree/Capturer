#ifndef CAPTURER_PDF_VIEWER_H
#define CAPTURER_PDF_VIEWER_H

#include "framelesswindow.h"

#include <memory>
#include <QMimeData>

class QPdfView;
class QPdfDocument;

class PdfViewer final : public FramelessWindow
{
    Q_OBJECT

public:
    explicit PdfViewer(const std::shared_ptr<QMimeData>& data, QWidget *parent = nullptr);

private:
    bool eventFilter(QObject *watched, QEvent *event) override;

    void contextMenuEvent(QContextMenuEvent *) override;
    void closeEvent(QCloseEvent *) override;

    std::shared_ptr<QMimeData> data_{};

    TitleBar     *title_bar_{};
    QMenu        *context_menu_{};
    QPdfView     *pdf_view_{};
    QPdfDocument *document_{};
};

#endif //! CAPTURER_PDF_VIEWER_H
