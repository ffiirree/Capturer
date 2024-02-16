#ifndef CAPTURER_CLIPBOARD_H
#define CAPTURER_CLIPBOARD_H

#include <memory>
#include <QDataStream>
#include <QMimeData>
#include <QIODevice>

namespace clipboard
{
    inline constexpr auto MIME_TYPE_POINT  = "application/x-capturer-point";
    inline constexpr auto MIME_TYPE_STATUS = "application/x-capturer-status";
    inline constexpr auto MIME_TYPE_IMAGE  = "application/x-qt-image";

    void init();

    QMimeData *clone(const QMimeData *other);

    //
    size_t size();

    bool empty();

    //
    std::shared_ptr<QMimeData> back(bool invisible = false);

    std::shared_ptr<QMimeData> at(size_t idx);

    //
    std::shared_ptr<QMimeData> push(const QPixmap& pixmap);

    std::shared_ptr<QMimeData> push(const QPixmap& pixmap, const QPoint& pos);

    std::shared_ptr<QMimeData> push(const QColor& color, const QString& text);

    std::shared_ptr<QMimeData> push(const std::shared_ptr<QMimeData>& mimedata);

    //
    std::vector<std::shared_ptr<QMimeData>> history();

    template<typename T> QByteArray serialize(const T& obj)
    {
        QByteArray  buffer;
        QDataStream stream(&buffer, QIODevice::WriteOnly);
        stream << obj;
        return buffer;
    }

    template<typename T> T deserialize(const QByteArray& buffer)
    {
        T obj{};

        QDataStream stream(buffer);
        stream >> obj;

        return obj;
    }
} // namespace clipboard

#endif //! CAPTURER_CLIPBOARD_H