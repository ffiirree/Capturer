#include "combobox.h"

#include "platforms/window-effect.h"

#include <QListView>
#include <QPlatformSurfaceEvent>

ComboBox::ComboBox(QWidget *parent)
    : QComboBox(parent)
{
    setView(new QListView());

#ifdef Q_OS_LINUX
    setProperty("system", "linux");
#endif

#ifdef Q_OS_WIN
    view()->window()->setAttribute(Qt::WA_TranslucentBackground);
    view()->window()->setWindowFlag(Qt::NoDropShadowWindowHint);
    view()->window()->installEventFilter(this);
#endif

    connect(this, &QComboBox::currentIndexChanged, [this](int) { emit selected(currentData()); });
}

#ifdef Q_OS_WIN
bool ComboBox::eventFilter(QObject *watched, QEvent *event)
{
    if (view()->window() == watched && event->type() == QEvent::PlatformSurface) {
        if (dynamic_cast<QPlatformSurfaceEvent *>(event)->surfaceEventType() !=
            QPlatformSurfaceEvent::SurfaceAboutToBeDestroyed) {
            const auto hwnd = reinterpret_cast<HWND>(view()->window()->winId());
            windows::dwm::set_window_corner(hwnd, DWMWCP_ROUND);
            windows::dwm::blur_behind(hwnd);
            windows::dwm::blur(hwnd, windows::dwm::blur_mode_t::ACRYLIC);
        }
    }
    return false;
}
#endif

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