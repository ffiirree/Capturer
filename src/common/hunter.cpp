#include "hunter.h"

#include <algorithm>

using namespace probe::graphics;

namespace hunter
{
    static std::deque<prey_t>     __preys{};
    static std::vector<display_t> __displays{};
    static window_filter_t        __scope{ window_filter_t::visible };

    prey_t prey_t::from(const QRect& rect)
    {
        return prey_t{
            .type = prey_type_t::rectangle,
            .geometry =
                probe::geometry_t{
                    .x      = rect.left(),
                    .y      = rect.top(),
                    .width  = static_cast<uint32_t>(rect.width()),
                    .height = static_cast<uint32_t>(rect.height()),
                },
        };
    }

    prey_t prey_t::from(const probe::geometry_t& geometry)
    {
        return prey_t{
            .type     = prey_type_t::rectangle,
            .geometry = geometry,
        };
    }

    prey_t prey_t::from(const probe::graphics::window_t& win)
    {
        return prey_t{
            .type     = !win.parent ? prey_type_t::window : prey_type_t::widget,
            .geometry = win.geometry,
            .handle   = win.handle,
            .name     = win.name,
            .codename = win.classname,
        };
    }

    prey_t prey_t::from(const probe::graphics::display_t& dis)
    {
        return prey_t{
            .type     = prey_type_t::display,
            .geometry = dis.geometry,
            .handle   = dis.handle,
            .name     = dis.name,
            .codename = dis.id,
        };
    }

    prey_t hunt(const QPoint& pos)
    {
        // virtual screen or corresponding display
        auto scoped = __preys.back();

        if (!!(__scope & window_filter_t::capturable)) {
#ifdef _WIN32
            scoped = prey_t::from(display_contains(pos).value());
#else
            for (const auto& display : __displays) {
                if (display.geometry.contains(pos.x(), pos.y())) {
                    scoped = prey_t::from(display);
                    break;
                }
            }
#endif
        }

        for (auto& prey : __preys) {
            if (prey.geometry.contains(pos.x(), pos.y())) {
                prey.geometry = scoped.geometry.intersected(prey.geometry);
                return prey;
            }
        }

        return scoped;
    }

    void ready(const window_filter_t flags)
    {
        __scope = flags;
        __preys.clear();

        std::ranges::for_each(probe::graphics::windows(flags, false),
                              [&](const auto& win) { __preys.emplace_back(prey_t::from(win)); });

        __displays = probe::graphics::displays();
        std::ranges::for_each(__displays,
                              [&](const auto& dis) { __preys.emplace_back(prey_t::from(dis)); });

        if (!any(flags & window_filter_t::capturable)) {
            const auto desktop = probe::graphics::virtual_screen();

            __preys.emplace_back(prey_t{
                .type     = prey_type_t::desktop,
                .geometry = desktop.geometry,
                .handle   = desktop.handle,
                .name     = desktop.name,
                .codename = desktop.classname,
            });
        }
    }

    void clear()
    {
        __scope = window_filter_t::visible;
        __preys.clear();
        __displays.clear();
    }

    std::string to_string(const prey_type_t type)
    {
        switch (type) {
        case prey_type_t::rectangle: return "rectangle";
        case prey_type_t::widget:    return "widget";
        case prey_type_t::window:    return "window";
        case prey_type_t::display:   return "display";
        case prey_type_t::desktop:   return "desktop";
        default:                     return "unknown";
        }
    }
} // namespace hunter
