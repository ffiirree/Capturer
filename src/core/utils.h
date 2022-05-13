#ifndef UTILS_H
#define UTILS_H

#include <vector>
#include <memory>
#include <utility>
#include <QFile>
#include <map>
#include <queue>

#ifndef st
#define st(X) do{X}while(0)
#endif

enum Graph : uint32_t {
    NONE,
    RECTANGLE   = 0x0001,
    ELLIPSE     = 0x0002,
    ARROW       = 0x0004,
    LINE        = 0x0008,
    CURVES      = 0x0010,
    TEXT        = 0x0020,
    MOSAIC      = 0x0040,
    ERASER      = 0x0080,
    BROKEN_LINE = 0x0100,
};

enum PaintType : uint32_t {
    UNMODIFIED      = 0x0000,
    UPDATE_MASK     = 0x0001,
    DRAW_MODIFYING  = 0x0010,
    DRAW_MODIFIED   = 0x0020 | UPDATE_MASK,
    DRAW_FINISHED   = 0x0040 | UPDATE_MASK,
    REPAINT_ALL     = 0x0100 | DRAW_MODIFIED | DRAW_FINISHED,
};

using std::string;
using std::shared_ptr;
using std::make_shared;
using std::vector;
using std::pair;
using std::make_pair;
using std::map;
using std::queue;

template<typename T>
inline void LOAD_QSS(T* obj, vector<QString> files)
{
    QString style = "";
    for (auto& qss : files) {

        QFile file(qss);
        file.open(QFile::ReadOnly);

        if (file.isOpen()) {
            style += file.readAll();
            file.close();
        }
    }
    obj->setStyleSheet(style);
}

template <typename F> class defer_raii
{
public:
    // copy/move construction and any kind of assignment would lead to the cleanup function getting
    // called twice. We can't have that.
    defer_raii(defer_raii&&) = delete;
    defer_raii(const defer_raii&) = delete;
    defer_raii& operator=(const defer_raii&) = delete;
    defer_raii& operator=(defer_raii&&) = delete;

    // construct the object from the given callable
    template <typename FF> defer_raii(FF&& f) : cleanup_function(std::forward<FF>(f)) {}

    // when the object goes out of scope call the cleanup function
    ~defer_raii() { cleanup_function(); }

private:
    F cleanup_function;
};

template <typename F> defer_raii<F> defer_func(F&& f)
{
    return { std::forward<F>(f) };
}

#define DEFER_ACTUALLY_JOIN(x, y) x##y
#define DEFER_JOIN(x, y) DEFER_ACTUALLY_JOIN(x, y)
#ifdef __COUNTER__
#define DEFER_UNIQUE_VARNAME(x) DEFER_JOIN(x, __COUNTER__)
#else
#define DEFER_UNIQUE_VARNAME(x) DEFER_JOIN(x, __LINE__)
#endif

#define defer(lambda__) [[maybe_unused]] const auto& DEFER_UNIQUE_VARNAME(_defer_) = defer_func([&]() { lambda__; })

#endif // UTILS_H
