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

    static void newOutputHandler(wl_listener *listener, void *data);
    static void frameHandler(wl_listener *listener, void *data);

    struct output_state {
        struct wlr_output *output;
        wl_listener frame_listen;
    };

    std::list<output_state> outputs;
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

    struct wlr_output_state output_state;
    wlr_output_state_init(&output_state);

    wlr_output_state_set_enabled(&output_state, true);

    struct wlr_output_mode *mode = wlr_output_preferred_mode(output);
    if (mode) {
        wlr_output_state_set_mode(&output_state, mode);
    }

    wlr_output_commit_state(output, &output_state);
    wlr_output_state_finish(&output_state);

    wlr_output_layout_add_auto(g_compositor->layout, output);

    // Создаём элемент прямо в списке
    g_compositor->outputs.emplace_back();

    // Получаем ссылку на последний элемент
    output_state& os = g_compositor->outputs.back();
    os.output = output;
    os.frame_listen.notify = &compositor::frameHandler;

    // Регистрируем слушатель
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

int main() {
    wlr_log_init(WLR_DEBUG, log_callback);
    compositor comp;
    if (!comp.init()) {
        return 1;
    }
    comp.run();
    return 0;
}
