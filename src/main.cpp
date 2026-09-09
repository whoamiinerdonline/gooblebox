#include <cstddef>
#include <cstdint>
#include <iostream>
#include <list>
#include <memory>
#include <vector>
#include <cstdio>
#include <cstdarg>
#include <wayland-server-protocol.h>

extern "C" {
    #include <wayland-server.h>
    #include <wayland-server-core.h>
    #include <wlr/backend.h>
    #include <wlr/render/allocator.h>
    #include <wlr/util/log.h>
    #include <wlr/types/wlr_output.h>
    #include <wlr/render/wlr_renderer.h>
    #include <wlr/types/wlr_output_layout.h>
    #include <wlr/render/pass.h>
    #include <wlr/util/box.h>

    // for key
    #include <wlr/types/wlr_input_device.h>
    #include <wlr/types/wlr_keyboard.h>
    #include <xkbcommon/xkbcommon.h>

    // for cursor and seat
    #include <wlr/types/wlr_cursor.h>
    #include <wlr/types/wlr_xcursor_manager.h>
    #include <wlr/types/wlr_seat.h>
    #include <wlr/types/wlr_pointer.h>
}

static FILE *log_file = nullptr;

static void log_callback(wlr_log_importance importance, const char *fmt, va_list args) {
    if (!log_file) {
        log_file = fopen("gooblebox.log", "w");
        if (!log_file) {
            fprintf(stderr, "Failed to open log file\n");
            return;
        }
    }
    vfprintf(log_file, fmt, args);
    fprintf(log_file, "\n");
    fflush(log_file);
}

static class compositor *g_compositor = nullptr;

class compositor {
private:
    wl_display *display;
    struct wlr_backend *backend;
    struct wlr_renderer *render;
    struct wlr_output_layout *layout;
    struct wlr_allocator *allocator;

    wl_listener new_input_listener;
    wl_listener new_output_listener;

    // cursor
    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *cursor_mgr;
    struct wlr_seat *seat;

    static void newOutputHandler(wl_listener *listener, void *data);
    static void frameHandler(wl_listener *listener, void *data);
    static void newInputHandler(wl_listener *listener, void *data);
    static void keyboardKeyHandler(wl_listener *listener, void *data);
    static void pointerMotionHandler(wl_listener *listener, void *data);

    struct keyboard_state {
        struct wlr_keyboard *key;
        wl_listener key_listener;
    };

    struct output_state {
        struct wlr_output *output;
        wl_listener frame_listen;
    };

    struct pointer_state {
        struct wlr_pointer *pointer;
        wl_listener motion_listener;
    };

    std::list<output_state> outputs;
    std::list<keyboard_state> keys;
    std::list<pointer_state> pointers;

public:
    bool init() {
        display = wl_display_create();
        if (!display) {
            std::cerr << "display not found" << std::endl;
            return false;
        }

        backend = wlr_backend_autocreate(wl_display_get_event_loop(display), NULL);
        if (!backend) {
            std::cerr << "backend not found" << std::endl;
            return false;
        }

        render = wlr_renderer_autocreate(backend);
        if (!render) {
            std::cerr << "renderer not found" << std::endl;
            return false;
        }

        layout = wlr_output_layout_create(display);
        if (!layout) {
            std::cerr << "layout not found" << std::endl;
            return false;
        }

        allocator = wlr_allocator_autocreate(backend, render);
        if (!allocator) {
            std::cerr << "allocator not found" << std::endl;
            return false;
        }

        cursor = wlr_cursor_create();
        wlr_cursor_attach_output_layout(cursor, layout);

        cursor_mgr = wlr_xcursor_manager_create("default", 24);
        wlr_xcursor_manager_load(cursor_mgr, 1.0);

        seat = wlr_seat_create(display, "seat0");
        uint32_t caps = WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD;
        wlr_seat_set_capabilities(seat, caps);

        new_output_listener.notify = &compositor::newOutputHandler;
        wl_signal_add(&backend->events.new_output, &new_output_listener);

        new_input_listener.notify = &compositor::newInputHandler;
        wl_signal_add(&backend->events.new_input, &new_input_listener);

        g_compositor = this;

        if (!wlr_backend_start(backend)) return false;

        return true;
    }

    void run() {
        wl_display_run(display);
    }

    ~compositor() {
        if (seat) wlr_seat_destroy(seat);
        if (cursor_mgr) wlr_xcursor_manager_destroy(cursor_mgr);
        if (cursor) wlr_cursor_destroy(cursor);

        wlr_output_layout_destroy(layout);
        wlr_renderer_destroy(render);
        wlr_backend_destroy(backend);
        wlr_allocator_destroy(allocator);
        wl_display_destroy(display);

        if (log_file) {
            fclose(log_file);
            log_file = nullptr;
        }
    }
};

void compositor::newOutputHandler(wl_listener *listener, void *data) {
    wlr_output *output = static_cast<wlr_output *>(data);
    wlr_output_init_render(output, g_compositor->allocator, g_compositor->render);

    struct wlr_output_state out_state;
    wlr_output_state_init(&out_state);

    wlr_output_state_set_enabled(&out_state, true);

    struct wlr_output_mode *mode = wlr_output_preferred_mode(output);
    if (mode) {
        wlr_output_state_set_mode(&out_state, mode);
    }

    wlr_output_commit_state(output, &out_state);
    wlr_output_state_finish(&out_state);

    wlr_output_layout_add_auto(g_compositor->layout, output);

    g_compositor->outputs.emplace_back();
    output_state& os = g_compositor->outputs.back();

    os.output = output;
    os.frame_listen.notify = &compositor::frameHandler;
    wl_signal_add(&output->events.frame, &os.frame_listen);
}

void compositor::frameHandler(wl_listener *listener, void *data) {
    wlr_output *output = static_cast<wlr_output *>(data);

    int width, height;
    wlr_output_effective_resolution(output, &width, &height);

    struct wlr_output_state state;
    wlr_output_state_init(&state);

    int buffer_age = 0;
    wlr_render_pass *pass = wlr_output_begin_render_pass(output, &state, &buffer_age, nullptr);

    if (!pass) {
        wlr_output_state_finish(&state);
        return;
    }

    struct wlr_box box = { .x = 0, .y = 0, .width = width, .height = height };
    struct wlr_render_rect_options rect_options = {
        .box = box,
        .color = {0.2f, 0.2f, 0.2f, 1.0f}
    };
    wlr_render_pass_add_rect(pass, &rect_options);

    wlr_render_pass_submit(pass);
    wlr_output_commit_state(output, &state);
    wlr_output_state_finish(&state);
}

void compositor::newInputHandler(wl_listener *listener, void *data) {
    struct wlr_input_device *device = static_cast<struct wlr_input_device *>(data);

    if (device->type == WLR_INPUT_DEVICE_KEYBOARD) {
        struct wlr_keyboard *keyboard = wlr_keyboard_from_input_device(device);

        struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
        struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, nullptr, XKB_KEYMAP_COMPILE_NO_FLAGS);
        wlr_keyboard_set_keymap(keyboard, keymap);
        xkb_keymap_unref(keymap);
        xkb_context_unref(context);

        g_compositor->keys.emplace_back();
        keyboard_state& ks = g_compositor->keys.back();
        ks.key = keyboard;
        ks.key_listener.notify = &compositor::keyboardKeyHandler;

        wl_signal_add(&keyboard->events.key, &ks.key_listener);
    }
    else if (device->type == WLR_INPUT_DEVICE_POINTER) {
        struct wlr_pointer *pointer = wlr_pointer_from_input_device(device);

        wlr_cursor_attach_input_device(g_compositor->cursor, device);

        g_compositor->pointers.emplace_back();
        pointer_state& ps = g_compositor->pointers.back();
        ps.pointer = pointer;
        ps.motion_listener.notify = &compositor::pointerMotionHandler;

        wl_signal_add(&pointer->events.motion, &ps.motion_listener);
    }
}

void compositor::keyboardKeyHandler(wl_listener *listener, void *data) {
    struct wlr_keyboard_key_event *event = static_cast<struct wlr_keyboard_key_event *>(data);
    struct wlr_keyboard *keyboard = nullptr;

    for (auto& ks : g_compositor->keys) {
        if (&ks.key_listener == listener) {
            keyboard = ks.key;
            break;
        }
    }
    if (!keyboard) return;

    if (event->state == WL_KEYBOARD_KEY_STATE_PRESSED) {
        uint32_t keycode = event->keycode + 8;
        const xkb_keysym_t *syms;
        int nsyms = xkb_state_key_get_syms(keyboard->xkb_state, keycode, &syms);

        for (int i = 0; i < nsyms; i++) {
            if (syms[i] == XKB_KEY_Escape) {
                std::cerr << "Escape pressed, terminating compositor..." << std::endl;
                wl_display_terminate(g_compositor->display);
            }
        }
    }
}

void compositor::pointerMotionHandler(wl_listener *listener, void *data) {
    struct wlr_pointer_motion_event *event = static_cast<struct wlr_pointer_motion_event *>(data);

    struct wlr_pointer *pointer = nullptr;
    for (auto& ps : g_compositor->pointers) {
        if (&ps.motion_listener == listener) {
            pointer = ps.pointer;
            break;
        }
    }
    if (!pointer) return;

    wlr_cursor_move(g_compositor->cursor, &pointer->base, event->delta_x, event->delta_y);

    wlr_cursor_set_xcursor(g_compositor->cursor, g_compositor->cursor_mgr, "left_ptr");

    wlr_seat_pointer_notify_motion(g_compositor->seat, event->time_msec,
                                   g_compositor->cursor->x, g_compositor->cursor->y);
}

int main() {
    wlr_log_init(WLR_DEBUG, log_callback);
    compositor comp;
    if (!comp.init()) {
        return 1;
    }
    comp.run();
    return 0;
}
