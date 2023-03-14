#ifdef __linux__

#include <mutex>
#include <fmt/format.h>
#include <logging.h>
#include "utils.h"
#include "linux-pulse.h"

static std::mutex pulse_mtx;
static pa_threaded_mainloop *pulse_loop = nullptr;
static pa_context *pulse_ctx = nullptr;

static void pulse_context_state_callback(pa_context *ctx, void *userdata) {
    switch (pa_context_get_state(ctx)) {
        // There are just here for reference
        case PA_CONTEXT_UNCONNECTED:
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
        default:
            break;
        case PA_CONTEXT_FAILED:
        case PA_CONTEXT_TERMINATED:
            LOG(INFO) << "PA_CONTEXT_TERMINATED";
            break;
        case PA_CONTEXT_READY:
            LOG(INFO) << "PA_CONTEXT_READY";
            break;
    }

    pulse_signal(0);
}

static const char *pulse_source_state_to_string(int state) {
    switch (state) {
        case PA_SOURCE_INVALID_STATE:
        default:
            return "INVALID";

        case PA_SOURCE_RUNNING:
            return "RUNNING";
        case PA_SOURCE_IDLE:
            return "IDLE";
        case PA_SOURCE_SUSPENDED:
            return "SUSPENDED";
        case PA_SOURCE_INIT:
            return "INIT";
        case PA_SOURCE_UNLINKED:
            return "UNLINKED";
    }
}

static void pulse_source_info_callback(pa_context *ctx, const pa_source_info *i, int eol, void *userdata) {
    if (eol == 0) {
        LOG(INFO) << "Audio source: " << i->name << ", " << i->description;
        reinterpret_cast<std::vector<PulseInfo> *>(userdata)->emplace_back(
                i->name,
                i->description,
                i->sample_spec.format,
                i->sample_spec.rate,
                i->sample_spec.channels,
                i->state,
                i->monitor_of_sink != PA_INVALID_INDEX
        );
    }

    pulse_signal(0);
}

static void pulse_sink_info_callback(pa_context *ctx, const pa_sink_info *i, int eol, void *userdata) {
    if (eol == 0) {
        LOG(INFO) << "Audio sink: " << i->name << ", " << i->description;
        reinterpret_cast<std::vector<PulseInfo> *>(userdata)->emplace_back(
                i->name,
                i->description,
                i->sample_spec.format,
                i->sample_spec.rate,
                i->sample_spec.channels,
                i->state,
                false
        );
    }

    pulse_signal(0);
}

void pulse_init() {
    std::lock_guard lock(pulse_mtx);

    if (pulse_loop == nullptr) {
        pulse_loop = pa_threaded_mainloop_new();
        pa_threaded_mainloop_start(pulse_loop);

        pulse_loop_lock();
        defer(pulse_loop_unlock());

        pulse_ctx = pa_context_new(pa_threaded_mainloop_get_api(pulse_loop), "FFMPEG_EXAMPLES_TEST");

        pa_context_set_state_callback(pulse_ctx, pulse_context_state_callback, nullptr);

        // A context must be connected to a server before any operation can be issued.
        // Calling pa_context_connect() will initiate the connection procedure.
        pa_context_connect(pulse_ctx, NULL, PA_CONTEXT_NOAUTOSPAWN, NULL);
    }
}

void pulse_unref() {
    std::lock_guard lock(pulse_mtx);

    pulse_loop_lock();

    if (pulse_ctx != nullptr) {
        pa_context_disconnect(pulse_ctx);
        pa_context_unref(pulse_ctx);

        pulse_ctx = nullptr;
    }

    pulse_loop_unlock();

    if (pulse_loop != nullptr) {
        pa_threaded_mainloop_stop(pulse_loop);
        pa_threaded_mainloop_free(pulse_loop);

        pulse_loop = nullptr;
    }
}

void pulse_loop_lock() {
    pa_threaded_mainloop_lock(pulse_loop);
}

void pulse_loop_unlock() {
    pa_threaded_mainloop_unlock(pulse_loop);
}

bool pulse_context_is_ready() {
    pulse_loop_lock();
    defer(pulse_loop_unlock());

    if (!PA_CONTEXT_IS_GOOD(pa_context_get_state(pulse_ctx))) {
        return false;
    }

    while (pa_context_get_state(pulse_ctx) != PA_CONTEXT_READY) {
        pa_threaded_mainloop_wait(pulse_loop);
    }

    return true;
}

void pulse_signal(int wait_for_accept) {
    pa_threaded_mainloop_signal(pulse_loop, wait_for_accept);
}

std::vector<PulseInfo> pulse_get_source_info_list() {
    if (!pulse_context_is_ready()) {
        return {};
    }

    pulse_loop_lock();
    defer(pulse_loop_unlock());

    std::vector<PulseInfo> list;
    auto op = pa_context_get_source_info_list(
            pulse_ctx,
            pulse_source_info_callback,
            &list);

    if (!op) {
        return {};
    }

    while (pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(pulse_loop);
    }
    pa_operation_unref(op);

    return list;
}

std::vector<PulseInfo> pulse_get_sink_info_list() {
    if (!pulse_context_is_ready()) {
        return {};
    }

    pulse_loop_lock();
    defer(pulse_loop_unlock());

    std::vector<PulseInfo> list;

    auto op = pa_context_get_sink_info_list(
            pulse_ctx,
            pulse_sink_info_callback,
            &list);

    if (!op) {
        return {};
    }

    while (pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
        pa_threaded_mainloop_wait(pulse_loop);
    }
    pa_operation_unref(op);

    return list;
}

#endif // !__linux__