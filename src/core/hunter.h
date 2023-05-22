#ifndef CAPTURE_HUNTER_H
#define CAPTURE_HUNTER_H

#include "probe/enum.h"
#include "probe/graphics.h"

#include <deque>

namespace hunter
{
    enum class prey_type_t
    {
        rectangle = 0x00,
        widget,
        window,
        display,
        desktop,
    };

    // widget / window / display / desktop
    struct prey_t
    {
        prey_type_t type{};
        probe::geometry_t geometry{};
        uint64_t handle{};

        std::string name{};
        std::string codename{}; // id / class name

        static prey_t from(const QRect&);
        static prey_t from(const probe::geometry_t&);
        static prey_t from(const probe::graphics::window_t&);
        static prey_t from(const probe::graphics::display_t&);
    };

    prey_t hunt(const QPoint&);

    void ready(probe::graphics::window_filter_t = probe::graphics::window_filter_t::visible);
    void clear();

    std::string to_string(prey_type_t);
} // namespace hunter

#endif // CAPTURE_HUNTER_H
