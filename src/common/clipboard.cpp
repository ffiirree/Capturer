#include "clipboard.h"

#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QPixmap>

static std::vector<std::shared_ptr<QMimeData>> __history;

class Clipboard : public QObject
{
public:
    static Clipboard& instance()
    {
        static Clipboard clipboard;
        return clipboard;
    }

    Clipboard(const Clipboard&)            = delete;
    Clipboard(Clipboard&&)                 = delete;
    Clipboard& operator=(const Clipboard&) = delete;
    Clipboard& operator=(Clipboard&&)      = delete;

private:
    explicit Clipboard(QObject *parent = nullptr)
        : QObject(parent)
    {
        connect(QApplication::clipboard(), &QClipboard::changed, [=, this](auto) {
            if (auto clipdata = QApplication::clipboard()->mimeData();
                !clipdata->hasFormat("application/x-capturer"))

                __history.emplace_back(std::shared_ptr<QMimeData>(clipboard::clone(clipdata)));
        });
    }
};

namespace clipboard
{
    void init() { Clipboard::instance(); }

    QMimeData *clone(const QMimeData *copydata)
    {
        auto mimedata = new QMimeData();

        foreach(const auto& format, copydata->formats()) {
            if (format == "application/x-qt-image") {
                mimedata->setImageData(copydata->imageData());
                continue;
            }

            mimedata->setData(format, copydata->data(format));
        }

        return mimedata;
    }

    std::shared_ptr<QMimeData> push(const QPixmap& pixmap)
    {
        auto mimedata = new QMimeData();
        mimedata->setImageData(QVariant(pixmap));
        mimedata->setData("application/x-capturer", "pixmap");

        QApplication::clipboard()->setMimeData(mimedata);
        return __history.emplace_back(std::shared_ptr<QMimeData>(clone(mimedata)));
    }

    std::shared_ptr<QMimeData> push(const QPixmap& pixmap, const QPoint& pos)
    {
        auto mimedata = new QMimeData();
        mimedata->setImageData(QVariant(pixmap));

        mimedata->setData(MIME_TYPE_POINT, serialize(pos));
        mimedata->setData("application/x-capturer", "pixmap");



        QApplication::clipboard()->setMimeData(mimedata);
        return __history.emplace_back(std::shared_ptr<QMimeData>(clone(mimedata)));
    }

    std::shared_ptr<QMimeData> push(const QColor& color)
    {
        auto mimedata = new QMimeData();
        mimedata->setColorData(QVariant(color));
        mimedata->setData("application/x-capturer", "color");

        QApplication::clipboard()->setMimeData(mimedata);
        return __history.emplace_back(std::shared_ptr<QMimeData>(clone(mimedata)));
    }

    size_t size() { return __history.size(); }

    bool empty() { return __history.empty(); }

    std::shared_ptr<QMimeData> back() { return __history.back(); }

    std::shared_ptr<QMimeData> at(size_t idx) { return __history.at(idx); }

    std::vector<std::shared_ptr<QMimeData>> history() { return __history; }
} // namespace clipboard
