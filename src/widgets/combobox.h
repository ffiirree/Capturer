#ifndef CAPTURER_COMBOBOX_H
#define CAPTURER_COMBOBOX_H

#include <QComboBox>

class ComboBox : public QComboBox
{
    Q_OBJECT
public:
    explicit ComboBox(QWidget *parent = nullptr);

    explicit ComboBox(const std::vector<std::pair<QVariant, QString>>& items, QWidget *parent = nullptr);

    template<typename T>
    requires std::integral<T>
    ComboBox& add(const std::vector<std::pair<T, QString>>& items)
    {
        for (const auto& [value, text] : items) {
            insertItem(count(), text, value);
        }
        return *this;
    }

    template<typename T>
    requires std::integral<T>
    ComboBox& add(const std::map<T, std::string>& items)
    {
        for (const auto& [value, text] : items) {
            insertItem(count(), QString::fromStdString(text), value);
        }
        return *this;
    }

    ComboBox& add(const QStringList& texts);
    ComboBox& add(const QVariant& value, const QString& text);
    ComboBox& add(const std::vector<std::pair<QVariant, QString>>& items);

    ComboBox& select(const QVariant& value);
    ComboBox& select(const std::string& value);

    template<class Slot> ComboBox& onselected(Slot slot)
    {
        connect(this, &ComboBox::selected, std::move(slot));
        return *this;
    }

signals:
    void selected(const QVariant& value);

private:
#ifdef Q_OS_WIN
    bool eventFilter(QObject *watched, QEvent *event) override;
#endif
};

#endif //! CAPTURER_COMBOBOX_H