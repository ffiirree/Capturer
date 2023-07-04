#include "clipboard.h"

#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QPixmap>
#include <ranges>

static std::vector<std::shared_ptr<QMimeData>> history_;

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
        connect(QApplication::clipboard(), &QClipboard::changed, [=](auto) {
            if (auto clipboard_data = QApplication::clipboard()->mimeData();
                !clipboard_data->hasFormat(clipboard::MIME_TYPE_STATUS)) {
                auto cloned = std::shared_ptr<QMimeData>(clipboard::clone(clipboard_data));
                cloned->setData(clipboard::MIME_TYPE_STATUS, "N");
                history_.emplace_back(cloned);
            }
        });
    }
};

namespace clipboard
{
    void init() { Clipboard::instance(); }

    QMimeData *clone(const QMimeData *other)
    {
        auto mimedata = new QMimeData();

        foreach(const auto& format, other->formats()) {
            if (format == MIME_TYPE_IMAGE) {
                mimedata->setImageData(other->imageData());
                continue;
            }

            mimedata->setData(format, other->data(format));
        }

        return mimedata;
    }

    std::shared_ptr<QMimeData> push(const QPixmap& pixmap)
    {
        auto mimedata = new QMimeData();
        mimedata->setImageData(QVariant(pixmap));
        mimedata->setData(clipboard::MIME_TYPE_STATUS, "N");

        QApplication::clipboard()->setMimeData(mimedata);
        return history_.emplace_back(std::shared_ptr<QMimeData>(clone(mimedata)));
    }

    std::shared_ptr<QMimeData> push(const QPixmap& pixmap, const QPoint& pos)
    {
        auto mimedata = new QMimeData();
        mimedata->setImageData(QVariant(pixmap));

        mimedata->setData(MIME_TYPE_POINT, serialize(pos));
        mimedata->setData(MIME_TYPE_STATUS, "N");

        QApplication::clipboard()->setMimeData(mimedata);
        return history_.emplace_back(std::shared_ptr<QMimeData>(clone(mimedata)));
    }

    std::shared_ptr<QMimeData> push(const QColor& color)
    {
        auto mimedata = new QMimeData();
        mimedata->setColorData(QVariant(color));
        mimedata->setData(MIME_TYPE_STATUS, "N");

        QApplication::clipboard()->setMimeData(mimedata);
        return history_.emplace_back(std::shared_ptr<QMimeData>(clone(mimedata)));
    }

    std::shared_ptr<QMimeData> push(const std::shared_ptr<QMimeData>& mimedata)
    {
        auto cloned = std::shared_ptr<QMimeData>(clone(mimedata.get()));
        cloned->setData(MIME_TYPE_STATUS, "N");

        QApplication::clipboard()->setMimeData(clone(cloned.get()));
        return history_.emplace_back(cloned);
    }

    size_t size() { return history_.size(); }

    bool empty() { return history_.empty(); }

    std::shared_ptr<QMimeData> back(bool invisible)
    {
        if (!invisible && !history_.empty()) {
            return history_.back();
        }

        for (auto iter : std::views::reverse(history_)) {
            if (iter->data(MIME_TYPE_STATUS) == "N") {
                return iter;
            }
        }

        return nullptr;
    }

    std::shared_ptr<QMimeData> at(size_t idx) { return history_.at(idx); }

    std::vector<std::shared_ptr<QMimeData>> history() { return history_; }
} // namespace clipboard
