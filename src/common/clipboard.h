#ifndef CAPTURER_CLIPBOARD_H
#define CAPTURER_CLIPBOARD_H

#include <memory>
#include <QDataStream>
#include <QMimeData>

namespace clipboard
{
    inline constexpr auto MIME_TYPE_POINT = "application/x-capturer-point";

    void init();

    QMimeData *clone(const QMimeData *copydata);

    //
    size_t size();

    bool empty();

    //
    std::shared_ptr<QMimeData> back();

    std::shared_ptr<QMimeData> at(size_t idx);

    //
    std::shared_ptr<QMimeData> push(const QPixmap& pixmap);

    std::shared_ptr<QMimeData> push(const QPixmap& pixmap, const QPoint& pos);

    std::shared_ptr<QMimeData> push(const QColor& color);

    //
    std::vector<std::shared_ptr<QMimeData>> history();

    template<typename T> QByteArray serialize(const T& obj)
    {
        QByteArray buffer;
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