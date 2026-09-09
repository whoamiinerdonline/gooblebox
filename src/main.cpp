#include <cstddef>
#include <iostream>
#include <list>
#include <memory>
#include <vector>
#include <cstdio>
#include <cstdarg>
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
}

static FILE *log_file = nullptr;

static void log_callback(wlr_log_importance importance, const char *fmt, va_list args) {
    if (!log_file) {
        log_file = fopen("gooblebox.log", "w"); // режим "w" очищает файл
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
    wl_listener new_output_listener;
    struct wlr_allocator *allocator;

    wl_listener new_input_listener;

    static void newOutputHandler(wl_listener *listener, void *data);
    static void frameHandler(wl_listener *listener, void *data);

    static void newInputHandler(wl_listener *listener, void *data);
    static void keyboardKeyHandler(wl_listener *listener, void *data);

    struct keyboard_state{
        struct wlr_keyboard *key;
        wl_listener key_listener;
    };

    struct output_state {
        struct wlr_output *output;
        wl_listener frame_listen;
    };

    std::list<output_state> outputs;
    std::list<keyboard_state> keys;
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

        new_output_listener.notify = &compositor::newOutputHandler;
        wl_signal_add(&backend->events.new_output, &new_output_listener);

        new_input_listener.notify = &compositor::newOutputHandler;
        wl_signal_add(&backend->events.new_input, &new_input_listener);

        g_compositor = this;

        if (!wlr_backend_start(backend)) return false;

        return true;
    }

    void run() {
        wl_display_run(display);
    }

    ~compositor() {
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

    // whileonly keyboards
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

    // только на нажатие (не на отпускание)
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

int main() {
    wlr_log_init(WLR_DEBUG, log_callback);
    compositor comp;
    if (!comp.init()) {
        return 1;
    }
    comp.run();
    return 0;
}
