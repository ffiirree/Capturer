#ifndef CAPTURER_COMBOBOX_H
#define CAPTURER_COMBOBOX_H

#include <QComboBox>

class ComboBox : public QComboBox
{
    Q_OBJECT
public:
    explicit ComboBox(QWidget *parent = nullptr);

    template<typename T>
    requires std::integral<T>
    ComboBox& add(const std::vector<std::pair<T, QString>>& items)
    {
        for (const auto& [value, text] : items) {
            insertItem(count(), text, value);
        }
        return *this;
    }

    ComboBox& add(const QStringList& texts);
    ComboBox& add(const QVariant& value, const QString& text);
    ComboBox& add(const std::vector<std::pair<QVariant, QString>>& items);

    ComboBox& select(const QVariant& value);
    ComboBox& select(const std::string& value);

    template<class Slot> inline ComboBox& onselected(Slot slot)
    {
        connect(this, &ComboBox::selected, std::move(slot));
        return *this;
    }

signals:
    void selected(const QVariant& value);
};

#endif //! CAPTURER_COMBOBOX_H