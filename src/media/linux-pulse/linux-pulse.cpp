#ifdef __linux__

#include "linux-pulse.h"

#include "probe/defer.h"
#include "utils.h"

#include <fmt/format.h>
#include <logging.h>
#include <mutex>

static std::mutex pulse_mtx;
static pa_threaded_mainloop *pulse_loop = nullptr;
static pa_context *pulse_ctx            = nullptr;

static void pulse_context_state_callback(pa_context *ctx, void *)
{
    switch (pa_context_get_state(ctx)) {
    // There are just here for reference
    case PA_CONTEXT_UNCONNECTED:
    case PA_CONTEXT_CONNECTING:
    case PA_CONTEXT_AUTHORIZING:
    case PA_CONTEXT_SETTING_NAME:
    default: break;
    case PA_CONTEXT_FAILED:
    case PA_CONTEXT_TERMINATED: DLOG(INFO) << "PA_CONTEXT_TERMINATED"; break;
    case PA_CONTEXT_READY: DLOG(INFO) << "PA_CONTEXT_READY"; break;
    }

    pulse::signal(0);
}

static const char *pulse_source_state_to_string(int state)
{
    // clang-format off
    switch (state) {
    case PA_SOURCE_RUNNING:         return "RUNNING";
    case PA_SOURCE_IDLE:            return "IDLE";
    case PA_SOURCE_SUSPENDED:       return "SUSPENDED";
    case PA_SOURCE_INIT:            return "INIT";
    case PA_SOURCE_UNLINKED:        return "UNLINKED";
    case PA_SOURCE_INVALID_STATE:
    default:                        return "INVALID";
    }
    // clang=format on
}

static void pulse_server_info_callback(pa_context *, const pa_server_info *i, void *userdata)
{
    auto info = reinterpret_cast<PulseServerInfo *>(userdata);

    info->name                = i->server_name;
    info->version             = i->server_version;
    info->default_sink_name   = i->default_sink_name;
    info->default_source_name = i->default_source_name;

    DLOG(INFO) << fmt::format("Pulse server: {} ({}), default sink = {}, default source = {}", info->name,
                              info->version, info->default_sink_name, info->default_source_name);

    pulse::signal(0);
}

namespace pulse
{
    void init()
    {
        std::lock_guard lock(pulse_mtx);

        if (pulse_loop == nullptr) {
            pulse_loop = ::pa_threaded_mainloop_new();
            ::pa_threaded_mainloop_start(pulse_loop);

            pulse::loop_lock();
            defer(pulse::loop_unlock());

            pulse_ctx =
                ::pa_context_new(::pa_threaded_mainloop_get_api(pulse_loop), "CAPTURER-PULSE-MODULE");

            ::pa_context_set_state_callback(pulse_ctx, pulse_context_state_callback, nullptr);

            // A context must be connected to a server before any operation can be issued.
            // Calling pa_context_connect() will initiate the connection procedure.
            ::pa_context_connect(pulse_ctx, nullptr, PA_CONTEXT_NOAUTOSPAWN, nullptr);
        }
    }

    void unref()
    {
        std::lock_guard lock(pulse_mtx);

        pulse::loop_lock();

        if (pulse_ctx != nullptr) {
            ::pa_context_disconnect(pulse_ctx);
            ::pa_context_unref(pulse_ctx);

            pulse_ctx = nullptr;
        }

        pulse::loop_unlock();

        if (pulse_loop != nullptr) {
            ::pa_threaded_mainloop_stop(pulse_loop);
            ::pa_threaded_mainloop_free(pulse_loop);

            pulse_loop = nullptr;
        }
    }

    void loop_lock() { ::pa_threaded_mainloop_lock(pulse_loop); }

    void loop_unlock() { ::pa_threaded_mainloop_unlock(pulse_loop); }

    bool context_is_ready()
    {
        pulse::loop_lock();
        defer(pulse::loop_unlock());

        if (!PA_CONTEXT_IS_GOOD(::pa_context_get_state(pulse_ctx))) {
            return false;
        }

        while (::pa_context_get_state(pulse_ctx) != PA_CONTEXT_READY) {
            ::pa_threaded_mainloop_wait(pulse_loop);
        }

        return true;
    }

    void signal(int wait_for_accept) { ::pa_threaded_mainloop_signal(pulse_loop, wait_for_accept); }

    std::vector<av::device_t> source_list()
    {
        if (!pulse::context_is_ready()) {
            return {};
        }

        pulse::loop_lock();
        defer(pulse::loop_unlock());

        std::vector<av::device_t> list;
        auto op = ::pa_context_get_source_info_list(
            pulse_ctx,
            [](auto, const pa_source_info *i, int eol, void *userdata) {
                if (eol == 0) {
                    DLOG(INFO) << fmt::format("Audio source: {} ({}), rate = {}, channels = {}, state = {}",
                                              i->name, i->description, i->sample_spec.rate,
                                              i->sample_spec.channels,
                                              pulse_source_state_to_string(i->state));

                    reinterpret_cast<std::vector<av::device_t> *>(userdata)->push_back(av::device_t{
                        .name        = i->description,
                        .id          = i->name,
                        .description = i->description,
                        .driver      = i->driver,
                        .type        = av::device_type_t::audio | av::device_type_t::source |
                                (i->monitor_of_sink != PA_INVALID_INDEX ? av::device_type_t::monitor
                                                                        : av::device_type_t::none),
                        .format = av::device_format_t::pulse,
                        .state  = static_cast<uint64_t>(i->state),
                    });
                }
                pulse::signal(0);
            },
            &list);

        if (!op) {
            return {};
        }

        while (::pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
            ::pa_threaded_mainloop_wait(pulse_loop);
        }
        ::pa_operation_unref(op);

        return list;
    }

    std::vector<av::device_t> sink_list()
    {
        if (!pulse::context_is_ready()) {
            return {};
        }

        pulse::loop_lock();
        defer(pulse::loop_unlock());

        std::vector<av::device_t> list;

        auto op = ::pa_context_get_sink_info_list(
            pulse_ctx,
            [](auto, const pa_sink_info *i, int eol, void *userdata) {
                if (eol == 0) {
                    DLOG(INFO) << fmt::format("Audio sink: {} ({}), rate = {}, channels = {}, state = {}",
                                              i->name, i->description, i->sample_spec.rate,
                                              i->sample_spec.channels,
                                              pulse_source_state_to_string(i->state));

                    reinterpret_cast<std::vector<av::device_t> *>(userdata)->push_back(av::device_t{
                        .name        = i->description,
                        .id          = i->name,
                        .description = i->description,
                        .driver      = i->driver,
                        .type        = av::device_type_t::audio | av::device_type_t::sink,
                        .format      = av::device_format_t::pulse,
                        .state       = static_cast<uint64_t>(i->state),
                    });
                }

                pulse::signal(0);
            },
            &list);

        if (!op) {
            return {};
        }

        while (::pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
            ::pa_threaded_mainloop_wait(pulse_loop);
        }
        ::pa_operation_unref(op);

        return list;
    }

    std::optional<av::device_t> default_source()
    {
        PulseServerInfo server{};
        if (server_info(server) < 0) {
            return std::nullopt;
        }

        if (!pulse::context_is_ready()) {
            return std::nullopt;
        }

        pulse::loop_lock();
        defer(pulse::loop_unlock());

        av::device_t dev{};
         auto op = ::pa_context_get_source_info_by_name(
            pulse_ctx, server.default_source_name.c_str(),
            [](auto, const pa_source_info *i, int eol, void *userdata) {
                if (eol == 0) {
                    auto dev         = reinterpret_cast<av::device_t *>(userdata);
                    dev->name        = i->description;
                    dev->id          = i->name;
                    dev->description = i->description;
                    dev->driver      = i->driver;
                    dev->type        = av::device_type_t::audio | av::device_type_t::source |
                                (i->monitor_of_sink != PA_INVALID_INDEX ? av::device_type_t::monitor
                                                                        : av::device_type_t::none);

                    dev->format = av::device_format_t::pulse;
                    dev->state  = static_cast<uint64_t>(i->state);
                }

                pulse::signal(0);
            },
            &dev);

        if (!op) {
            return std::nullopt;
        }

        while (::pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
            ::pa_threaded_mainloop_wait(pulse_loop);
        }
        ::pa_operation_unref(op);

        if (dev.id.empty()) return std::nullopt;

        return dev;
    }

    std::optional<av::device_t> default_sink()
    {
        PulseServerInfo server{};
        if (server_info(server) < 0) {
            return std::nullopt;
        }

        if (!pulse::context_is_ready()) {
            return std::nullopt;
        }

        pulse::loop_lock();
        defer(pulse::loop_unlock());

        av::device_t dev{};
        auto op = ::pa_context_get_sink_info_by_name(
            pulse_ctx, server.default_sink_name.c_str(),
            [](auto, const pa_sink_info *i, int eol, void *userdata) {
                if (eol == 0) {
                    auto dev         = reinterpret_cast<av::device_t *>(userdata);
                    dev->name        = i->description;
                    dev->id          = i->name;
                    dev->description = i->description;
                    dev->driver      = i->driver;
                    dev->type        = av::device_type_t::audio | av::device_type_t::sink;
                    dev->format      = av::device_format_t::pulse;
                    dev->state       = static_cast<uint64_t>(i->state);
                }

                pulse::signal(0);
            },
            &dev);

        if (!op) {
            return std::nullopt;
        }

        while (::pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
            ::pa_threaded_mainloop_wait(pulse_loop);
        }
        ::pa_operation_unref(op);

        if (dev.id.empty()) return std::nullopt;

        return dev;
    }

    int server_info(PulseServerInfo& info)
    {
        if (!pulse::context_is_ready()) {
            return -1;
        }

        pulse::loop_lock();
        defer(pulse::loop_unlock());

        auto op = pa_context_get_server_info(pulse_ctx, pulse_server_info_callback, &info);
        if (!op) {
            return -1;
        }

        while (::pa_operation_get_state(op) == PA_OPERATION_RUNNING)
            ::pa_threaded_mainloop_wait(pulse_loop);
        ::pa_operation_unref(op);

        return 0;
    }
} // namespace pulse

#endif // !__linux__