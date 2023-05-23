#ifndef COMBOBOX_H
#define COMBOBOX_H

#include <QComboBox>
#include <QListView>

class ComboBox : public QComboBox
{
    Q_OBJECT
public:
    explicit ComboBox(QWidget *parent = nullptr)
        : QComboBox(parent)
    {
        setView(new QListView());
        view()->window()->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
        view()->window()->setAttribute(Qt::WA_TranslucentBackground);

        connect(this, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
                [this](int) { emit selected(currentData()); });
    }

    inline ComboBox& add(const std::vector<std::pair<QVariant, QString>>& items)
    {
        for (auto& [value, text] : items) {
            insertItem(count(), text, value);
        }
        return *this;
    }

    inline ComboBox& add(const QStringList& texts)
    {
        for (auto& text : texts) {
            auto index = count();
            insertItem(index, text, index);
        }
        return *this;
    }

    inline ComboBox& add(const QVariant& value, const QString& text)
    {
        insertItem(count(), text, value);
        return *this;
    }

    inline ComboBox& select(const QVariant& value)
    {
        setCurrentIndex(std::max(0, findData(value)));
        return *this;
    }

    inline ComboBox& select(const std::string& value)
    {
        setCurrentIndex(std::max(0, findData(QString::fromUtf8(value.c_str()))));
        return *this;
    }

    template<class Slot> inline ComboBox& onselected(Slot slot)
    {
        connect(this, &ComboBox::selected, std::move(slot));
        return *this;
    }

signals:
    void selected(const QVariant& value);
};

#endif // !COMBOBOX_H