#include "pdf-viewer.h"

#include "clipboard.h"
#include "config.h"
#include "logging.h"
#include "menu.h"
#include "titlebar.h"

#include <QContextMenuEvent>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMimeData>
#include <QPdfDocument>
#include <QPdfView>
#include <QUrl>
#include <QVBoxLayout>

PdfViewer::PdfViewer(const std::shared_ptr<QMimeData>& data, QWidget *parent)
    : FramelessWindow(parent, Qt::WindowMinMaxButtonsHint | Qt::WindowFullscreenButtonHint |
                                  Qt::WindowStaysOnTopHint),
      data_(data)
{
    setLayout(new QVBoxLayout());
    layout()->setSpacing(0);
    layout()->setContentsMargins({});

    title_bar_ = titlebar();
    title_bar_->setObjectName("title-bar");
    title_bar_->setHideOnFullScreen(false);
    layout()->addWidget(title_bar_);

    setWindowTitle("Pdf Viewer");

    //
    data_->setData(clipboard::MIME_TYPE_STATUS, "P");

    pdf_view_ = new QPdfView();
    document_ = new QPdfDocument();
    pdf_view_->setDocument(document_);
    pdf_view_->setPageMode(QPdfView::PageMode::MultiPage);
    pdf_view_->setDocumentMargins({});
    pdf_view_->viewport()->installEventFilter(this);

    layout()->addWidget(pdf_view_);

    if (!data->urls().empty() && data->urls()[0].isLocalFile()) {
        setWindowTitle(QFileInfo(data->urls()[0].toLocalFile()).fileName());
        document_->load(data->urls()[0].toLocalFile());

        auto dpi = QGuiApplication::primaryScreen()->physicalDotsPerInch();
        resize((document_->pagePointSize(0) / 72 * dpi).toSize());
    }

    //
    context_menu_ = new Menu(this);
    addAction(context_menu_->addAction(QIcon::fromTheme("close-m"), tr("Close"),
                                       QKeySequence(Qt::Key_Escape), this, &PdfViewer::close));
}

void PdfViewer::contextMenuEvent(QContextMenuEvent *event) { context_menu_->exec(event->globalPos()); }

bool PdfViewer::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == pdf_view_->viewport() && event->type() == QEvent::Wheel &&
        dynamic_cast<QWheelEvent *>(event)->modifiers() & Qt::CTRL) {
        const auto delta = dynamic_cast<QWheelEvent *>(event)->angleDelta().y();
        pdf_view_->setZoomFactor(delta > 0 ? pdf_view_->zoomFactor() * 1.05 : pdf_view_->zoomFactor()  / 1.05);
        return true;
    }

    return false;
}

void PdfViewer::closeEvent(QCloseEvent *event)
{
    data_->setData(clipboard::MIME_TYPE_STATUS, "N");
    return FramelessWindow::closeEvent(event);
}
