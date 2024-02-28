#include "combobox.h"

#include <QListView>

ComboBox::ComboBox(QWidget *parent)
    : QComboBox(parent)
{
    setView(new QListView());
    view()->window()->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    view()->window()->setAttribute(Qt::WA_TranslucentBackground);

    connect(this, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this](int) { emit selected(currentData()); });
}

ComboBox::ComboBox(const std::vector<std::pair<QVariant, QString>>& items, QWidget *parent)
    : ComboBox(parent)
{
    add(items);
}

ComboBox& ComboBox::add(const std::vector<std::pair<QVariant, QString>>& items)
{
    for (const auto& [value, text] : items) {
        insertItem(count(), text, value);
    }
    return *this;
}

ComboBox& ComboBox::add(const QStringList& texts)
{
    for (const auto& text : texts) {
        auto index = count();
        insertItem(index, text, index);
    }
    return *this;
}

ComboBox& ComboBox::add(const QVariant& value, const QString& text)
{
    insertItem(count(), text, value);
    return *this;
}

ComboBox& ComboBox::select(const QVariant& value)
{
    // trigger if we force the index to be 0 while the current index also is 0.
    auto idx = findData(value);
    if (idx < 0 && currentIndex() == 0) {
        idx = 0;
        emit selected(itemData(0));
    }

    setCurrentIndex(idx);
    return *this;
}

ComboBox& ComboBox::select(const std::string& value)
{
    // trigger if we force the index to be 0 while the current index also is 0.
    auto idx = findData(QString::fromUtf8(value.c_str()));
    if (idx < 0 && currentIndex() == 0) {
        idx = 0;
        emit selected(itemData(0));
    }

    setCurrentIndex(idx);
    return *this;
}