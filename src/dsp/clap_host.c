/*
 * CLAP Host Core - Plugin discovery, loading, and processing
 */
#include "clap_host.h"
#include "clap/clap.h"
#include "clap/factory/plugin-factory.h"
#include "clap/ext/audio-ports.h"
#include "clap/ext/note-ports.h"
#include "clap/ext/params.h"
#include "clap/ext/thread-check.h"
#include "clap/ext/state.h"
#include "clap/ext/latency.h"
#include "clap/ext/tail.h"
#include "clap/ext/track-info.h"
#include "clap/ext/gui.h"
#include "clap/ext/voice-info.h"
#include "clap/ext/note-name.h"
#include "clap/ext/audio-ports-config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <dirent.h>
#include <pthread.h>

/* Sample rate for activation */
#define HOST_SAMPLE_RATE 44100.0
#define HOST_MIN_FRAMES 1
#define HOST_MAX_FRAMES 4096

/* MIDI event queue */
#define MAX_MIDI_EVENTS 256
typedef struct {
    uint8_t data[3];
    int len;
} midi_event_t;

static midi_event_t s_midi_queue[MAX_MIDI_EVENTS];
static int s_midi_queue_count = 0;
static pthread_mutex_t s_midi_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Track main thread ID for thread check */
static pthread_t s_main_thread;
static int s_main_thread_set = 0;

/* Host callbacks (minimal implementation) */
static void host_log(const clap_host_t *host, clap_log_severity severity, const char *msg) {
    fprintf(stderr, "[CLAP] %s\n", msg);
}

static const clap_host_log_t s_host_log = {
    .log = host_log
};

/* Thread check extension - prevents crashes from thread assertions */
static bool host_is_main_thread(const clap_host_t *host) {
    if (!s_main_thread_set) return true;
    return pthread_equal(pthread_self(), s_main_thread);
}

static bool host_is_audio_thread(const clap_host_t *host) {
    /* In our single-threaded host, audio runs on main thread */
    return true;
}

static const clap_host_thread_check_t s_host_thread_check = {
    .is_main_thread = host_is_main_thread,
    .is_audio_thread = host_is_audio_thread
};

/* State extension - stub implementation */
static void host_state_mark_dirty(const clap_host_t *host) {
    /* No-op: we don't track dirty state */
}

static const clap_host_state_t s_host_state = {
    .mark_dirty = host_state_mark_dirty
};

/* Latency extension - stub implementation */
static void host_latency_changed(const clap_host_t *host) {
    /* No-op: we don't compensate for latency */
}

static const clap_host_latency_t s_host_latency = {
    .changed = host_latency_changed
};

/* Tail extension - stub implementation */
static void host_tail_changed(const clap_host_t *host) {
    /* No-op: we don't handle tail */
}

static const clap_host_tail_t s_host_tail = {
    .changed = host_tail_changed
};

/* Params extension - stub implementation */
static void host_params_rescan(const clap_host_t *host, clap_param_rescan_flags flags) {
    /* No-op: we re-query params on demand */
}

static void host_params_clear(const clap_host_t *host, clap_id param_id, clap_param_clear_flags flags) {
    /* No-op */
}

static void host_params_request_flush(const clap_host_t *host) {
    /* No-op: we don't support async param flush */
}

static const clap_host_params_t s_host_params = {
    .rescan = host_params_rescan,
    .clear = host_params_clear,
    .request_flush = host_params_request_flush
};

/* Track info extension - stub implementation */
static bool host_track_info_get(const clap_host_t *host, clap_track_info_t *info) {
    if (!info) return false;
    memset(info, 0, sizeof(*info));
    info->flags = 0;  /* No track info available */
    strncpy(info->name, "Move Track", sizeof(info->name) - 1);
    return true;
}

static const clap_host_track_info_t s_host_track_info = {
    .get = host_track_info_get
};

/* GUI extension - stub implementation (no GUI support) */
static void host_gui_resize_hints_changed(const clap_host_t *host) {}
static bool host_gui_request_resize(const clap_host_t *host, uint32_t width, uint32_t height) { return false; }
static bool host_gui_request_show(const clap_host_t *host) { return false; }
static bool host_gui_request_hide(const clap_host_t *host) { return false; }
static void host_gui_closed(const clap_host_t *host, bool was_destroyed) {}

static const clap_host_gui_t s_host_gui = {
    .resize_hints_changed = host_gui_resize_hints_changed,
    .request_resize = host_gui_request_resize,
    .request_show = host_gui_request_show,
    .request_hide = host_gui_request_hide,
    .closed = host_gui_closed
};

/* Note name extension - stub implementation */
static void host_note_name_changed(const clap_host_t *host) {}

static const clap_host_note_name_t s_host_note_name = {
    .changed = host_note_name_changed
};

/* Audio ports config extension - stub implementation */
static void host_audio_ports_config_rescan(const clap_host_t *host) {}

static const clap_host_audio_ports_config_t s_host_audio_ports_config = {
    .rescan = host_audio_ports_config_rescan
};

static void host_request_restart(const clap_host_t *host) {}
static void host_request_process(const clap_host_t *host) {}
static void host_request_callback(const clap_host_t *host) {}

static const void *host_get_extension(const clap_host_t *host, const char *extension_id) {
    /* Core extensions */
    if (!strcmp(extension_id, CLAP_EXT_LOG)) return &s_host_log;
    if (!strcmp(extension_id, CLAP_EXT_THREAD_CHECK)) return &s_host_thread_check;
    if (!strcmp(extension_id, CLAP_EXT_STATE)) return &s_host_state;
    if (!strcmp(extension_id, CLAP_EXT_LATENCY)) return &s_host_latency;
    if (!strcmp(extension_id, CLAP_EXT_TAIL)) return &s_host_tail;
    if (!strcmp(extension_id, CLAP_EXT_PARAMS)) return &s_host_params;
    if (!strcmp(extension_id, CLAP_EXT_TRACK_INFO)) return &s_host_track_info;
    if (!strcmp(extension_id, CLAP_EXT_TRACK_INFO_COMPAT)) return &s_host_track_info;
    if (!strcmp(extension_id, CLAP_EXT_GUI)) return &s_host_gui;
    if (!strcmp(extension_id, CLAP_EXT_NOTE_NAME)) return &s_host_note_name;
    if (!strcmp(extension_id, CLAP_EXT_AUDIO_PORTS_CONFIG)) return &s_host_audio_ports_config;
    /* Return NULL for unimplemented extensions - plugins should handle gracefully */
    return NULL;
}

static const clap_host_t s_host = {
    .clap_version = CLAP_VERSION,
    .host_data = NULL,
    .name = "Move Anything CLAP Host",
    .vendor = "Move Anything",
    .url = "",
    .version = "1.0.0",
    .get_extension = host_get_extension,
    .request_restart = host_request_restart,
    .request_process = host_request_process,
    .request_callback = host_request_callback
};

/* Helper: check if string ends with suffix */
static int ends_with(const char *str, const char *suffix) {
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) return 0;
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

/* Helper: add plugin to list */
static int list_add(clap_host_list_t *list, const clap_plugin_info_t *info) {
    if (list->count >= list->capacity) {
        int new_cap = list->capacity == 0 ? 16 : list->capacity * 2;
        if (new_cap > CLAP_HOST_MAX_PLUGINS) new_cap = CLAP_HOST_MAX_PLUGINS;
        if (list->count >= new_cap) return -1;

        clap_plugin_info_t *new_items = (clap_plugin_info_t *)realloc(list->items, new_cap * sizeof(clap_plugin_info_t));
        if (!new_items) return -1;
        list->items = new_items;
        list->capacity = new_cap;
    }
    list->items[list->count++] = *info;
    return 0;
}

/* Scan a single .clap file and add plugins to list */
static int scan_clap_file(const char *path, clap_host_list_t *list) {
    void *handle = dlopen(path, RTLD_LOCAL | RTLD_LAZY);
    if (!handle) {
        fprintf(stderr, "[CLAP] dlopen failed for %s: %s\n", path, dlerror());
        return -1;
    }

    const clap_plugin_entry_t *entry = (const clap_plugin_entry_t *)dlsym(handle, "clap_entry");
    if (!entry) {
        fprintf(stderr, "[CLAP] No clap_entry in %s\n", path);
        dlclose(handle);
        return -1;
    }

    /* Initialize entry */
    if (!entry->init(path)) {
        fprintf(stderr, "[CLAP] entry->init failed for %s\n", path);
        dlclose(handle);
        return -1;
    }

    /* Get plugin factory */
    const clap_plugin_factory_t *factory =
        (const clap_plugin_factory_t *)entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    if (!factory) {
        fprintf(stderr, "[CLAP] No plugin factory in %s\n", path);
        entry->deinit();
        dlclose(handle);
        return -1;
    }

    /* Enumerate plugins */
    uint32_t count = factory->get_plugin_count(factory);
    for (uint32_t i = 0; i < count; i++) {
        const clap_plugin_descriptor_t *desc = factory->get_plugin_descriptor(factory, i);
        if (!desc) continue;

        clap_plugin_info_t info = {0};
        strncpy(info.id, desc->id ? desc->id : "", sizeof(info.id) - 1);
        strncpy(info.name, desc->name ? desc->name : "", sizeof(info.name) - 1);
        strncpy(info.vendor, desc->vendor ? desc->vendor : "", sizeof(info.vendor) - 1);
        strncpy(info.path, path, sizeof(info.path) - 1);
        info.plugin_index = i;

        /* Create temporary instance to query ports */
        const clap_plugin_t *plugin = factory->create_plugin(factory, &s_host, desc->id);
        if (plugin && plugin->init(plugin)) {
            /* Query audio ports */
            const clap_plugin_audio_ports_t *audio_ports =
                (const clap_plugin_audio_ports_t *)plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS);
            if (audio_ports) {
                info.has_audio_in = audio_ports->count(plugin, true) > 0;
                info.has_audio_out = audio_ports->count(plugin, false) > 0;
            }

            /* Query note ports */
            const clap_plugin_note_ports_t *note_ports =
                (const clap_plugin_note_ports_t *)plugin->get_extension(plugin, CLAP_EXT_NOTE_PORTS);
            if (note_ports) {
                info.has_midi_in = note_ports->count(plugin, true) > 0;
                info.has_midi_out = note_ports->count(plugin, false) > 0;
            }

            plugin->destroy(plugin);
        }

        list_add(list, &info);
    }

    entry->deinit();
    dlclose(handle);
    return 0;
}

int clap_scan_plugins(const char *dir, clap_host_list_t *out) {
    /* Record main thread for thread check extension */
    if (!s_main_thread_set) {
        s_main_thread = pthread_self();
        s_main_thread_set = 1;
    }

    /* Add plugins directory to LD_LIBRARY_PATH so plugins can find bundled libs */
    const char *current_path = getenv("LD_LIBRARY_PATH");
    char new_path[2048];
    if (current_path && current_path[0]) {
        snprintf(new_path, sizeof(new_path), "%s:%s", dir, current_path);
    } else {
        snprintf(new_path, sizeof(new_path), "%s", dir);
    }
    setenv("LD_LIBRARY_PATH", new_path, 1);

    DIR *d = opendir(dir);
    if (!d) {
        fprintf(stderr, "[CLAP] Cannot open directory: %s\n", dir);
        return -1;
    }

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (!ends_with(ent->d_name, ".clap")) continue;

        char path[1280];
        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);
        scan_clap_file(path, out);
    }

    closedir(d);
    return 0;
}

void clap_free_plugin_list(clap_host_list_t *list) {
    if (list->items) {
        free(list->items);
        list->items = NULL;
    }
    list->count = 0;
    list->capacity = 0;
}

int clap_load_plugin(const char *path, int plugin_index, clap_instance_t *out) {
    memset(out, 0, sizeof(*out));
    fprintf(stderr, "[CLAP] Loading: %s index %d\n", path, plugin_index);

    void *handle = dlopen(path, RTLD_LOCAL | RTLD_NOW);
    if (!handle) {
        fprintf(stderr, "[CLAP] dlopen failed: %s\n", dlerror());
        return -1;
    }
    fprintf(stderr, "[CLAP] dlopen OK\n");

    const clap_plugin_entry_t *entry = (const clap_plugin_entry_t *)dlsym(handle, "clap_entry");
    if (!entry) {
        fprintf(stderr, "[CLAP] No clap_entry symbol\n");
        dlclose(handle);
        return -1;
    }
    fprintf(stderr, "[CLAP] entry OK\n");

    if (!entry->init(path)) {
        fprintf(stderr, "[CLAP] entry->init failed\n");
        dlclose(handle);
        return -1;
    }
    fprintf(stderr, "[CLAP] entry->init OK\n");

    const clap_plugin_factory_t *factory =
        (const clap_plugin_factory_t *)entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    if (!factory) {
        fprintf(stderr, "[CLAP] No plugin factory\n");
        entry->deinit();
        dlclose(handle);
        return -1;
    }
    fprintf(stderr, "[CLAP] factory OK\n");

    const clap_plugin_descriptor_t *desc = factory->get_plugin_descriptor(factory, plugin_index);
    if (!desc) {
        fprintf(stderr, "[CLAP] Invalid plugin index\n");
        entry->deinit();
        dlclose(handle);
        return -1;
    }
    fprintf(stderr, "[CLAP] descriptor OK: %s\n", desc->name ? desc->name : "(null)");

    const clap_plugin_t *plugin = factory->create_plugin(factory, &s_host, desc->id);
    if (!plugin) {
        fprintf(stderr, "[CLAP] create_plugin failed\n");
        entry->deinit();
        dlclose(handle);
        return -1;
    }
    fprintf(stderr, "[CLAP] create_plugin OK\n");

    fprintf(stderr, "[CLAP] calling plugin->init...\n");
    if (!plugin->init(plugin)) {
        fprintf(stderr, "[CLAP] plugin->init failed\n");
        plugin->destroy(plugin);
        entry->deinit();
        dlclose(handle);
        return -1;
    }
    fprintf(stderr, "[CLAP] plugin->init OK\n");

    /* Activate the plugin */
    fprintf(stderr, "[CLAP] calling plugin->activate...\n");
    if (!plugin->activate(plugin, HOST_SAMPLE_RATE, HOST_MIN_FRAMES, HOST_MAX_FRAMES)) {
        fprintf(stderr, "[CLAP] plugin->activate failed\n");
        plugin->destroy(plugin);
        entry->deinit();
        dlclose(handle);
        return -1;
    }
    fprintf(stderr, "[CLAP] plugin->activate OK\n");

    /* Start processing */
    fprintf(stderr, "[CLAP] calling plugin->start_processing...\n");
    if (!plugin->start_processing(plugin)) {
        fprintf(stderr, "[CLAP] plugin->start_processing failed\n");
        plugin->deactivate(plugin);
        plugin->destroy(plugin);
        entry->deinit();
        dlclose(handle);
        return -1;
    }
    fprintf(stderr, "[CLAP] plugin->start_processing OK\n");

    out->handle = handle;
    out->entry = entry;
    out->factory = factory;
    out->plugin = plugin;
    out->activated = true;
    out->processing = true;
    strncpy(out->path, path, sizeof(out->path) - 1);

    return 0;
}

void clap_unload_plugin(clap_instance_t *inst) {
    if (!inst->plugin) return;

    const clap_plugin_t *plugin = (const clap_plugin_t *)inst->plugin;
    const clap_plugin_entry_t *entry = (const clap_plugin_entry_t *)inst->entry;

    if (inst->processing) {
        plugin->stop_processing(plugin);
        inst->processing = false;
    }
    if (inst->activated) {
        plugin->deactivate(plugin);
        inst->activated = false;
    }
    plugin->destroy(plugin);

    if (entry) entry->deinit();
    if (inst->handle) dlclose(inst->handle);

    memset(inst, 0, sizeof(*inst));
}

/* Static buffers for CLAP process (avoid per-call allocation) */
static float *s_in_bufs[2] = {NULL, NULL};
static float *s_out_bufs[2] = {NULL, NULL};
static int s_buf_frames = 0;

/* CLAP event storage for current process block */
static clap_event_note_t s_note_events[MAX_MIDI_EVENTS];
static int s_note_event_count = 0;

/* Param event storage for current process block */
#define MAX_PARAM_EVENTS 32
static clap_event_param_value_t s_param_events[MAX_PARAM_EVENTS];
static int s_param_event_count = 0;

/* Event list callbacks - returns combined note + param events */
static uint32_t s_events_size(const clap_input_events_t *list) {
    return (uint32_t)(s_note_event_count + s_param_event_count);
}

static const clap_event_header_t *s_events_get(const clap_input_events_t *list, uint32_t index) {
    /* Note events first */
    if (index < (uint32_t)s_note_event_count) {
        return &s_note_events[index].header;
    }
    /* Then param events */
    index -= s_note_event_count;
    if (index < (uint32_t)s_param_event_count) {
        return &s_param_events[index].header;
    }
    return NULL;
}

static bool s_empty_push(const clap_output_events_t *list, const clap_event_header_t *event) { return true; }

/* Convert MIDI queue to CLAP note events */
static void prepare_midi_events(void) {
    pthread_mutex_lock(&s_midi_mutex);

    s_note_event_count = 0;
    for (int i = 0; i < s_midi_queue_count && s_note_event_count < MAX_MIDI_EVENTS; i++) {
        midi_event_t *m = &s_midi_queue[i];
        if (m->len < 3) continue;

        uint8_t status = m->data[0] & 0xF0;
        uint8_t channel = m->data[0] & 0x0F;
        uint8_t note = m->data[1];
        uint8_t velocity = m->data[2];

        clap_event_note_t *evt = &s_note_events[s_note_event_count];

        if (status == 0x90 && velocity > 0) {
            /* Note on */
            evt->header.size = sizeof(clap_event_note_t);
            evt->header.time = 0;
            evt->header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            evt->header.type = CLAP_EVENT_NOTE_ON;
            evt->header.flags = 0;
            evt->note_id = -1;
            evt->port_index = 0;
            evt->channel = channel;
            evt->key = note;
            evt->velocity = velocity / 127.0;
            s_note_event_count++;
        } else if (status == 0x80 || (status == 0x90 && velocity == 0)) {
            /* Note off */
            evt->header.size = sizeof(clap_event_note_t);
            evt->header.time = 0;
            evt->header.space_id = CLAP_CORE_EVENT_SPACE_ID;
            evt->header.type = CLAP_EVENT_NOTE_OFF;
            evt->header.flags = 0;
            evt->note_id = -1;
            evt->port_index = 0;
            evt->channel = channel;
            evt->key = note;
            evt->velocity = velocity / 127.0;
            s_note_event_count++;
        }
    }

    s_midi_queue_count = 0;
    pthread_mutex_unlock(&s_midi_mutex);
}

/* Convert instance param queue to CLAP param events */
static void prepare_param_events(clap_instance_t *inst) {
    s_param_event_count = 0;

    for (int i = 0; i < inst->param_queue_count && s_param_event_count < MAX_PARAM_EVENTS; i++) {
        clap_event_param_value_t *evt = &s_param_events[s_param_event_count];
        evt->header.size = sizeof(clap_event_param_value_t);
        evt->header.time = 0;
        evt->header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        evt->header.type = CLAP_EVENT_PARAM_VALUE;
        evt->header.flags = 0;
        evt->param_id = inst->param_queue[i].param_id;
        evt->cookie = NULL;
        evt->note_id = -1;
        evt->port_index = -1;
        evt->channel = -1;
        evt->key = -1;
        evt->value = inst->param_queue[i].value;
        s_param_event_count++;
    }

    inst->param_queue_count = 0;
}

static void ensure_buffers(int frames) {
    if (frames <= s_buf_frames) return;

    for (int i = 0; i < 2; i++) {
        free(s_in_bufs[i]);
        free(s_out_bufs[i]);
        s_in_bufs[i] = (float *)calloc(frames, sizeof(float));
        s_out_bufs[i] = (float *)calloc(frames, sizeof(float));
    }
    s_buf_frames = frames;
}

int clap_process_block(clap_instance_t *inst, const float *in, float *out, int frames) {
    if (!inst->plugin || !inst->processing) {
        return -1;
    }

    const clap_plugin_t *plugin = (const clap_plugin_t *)inst->plugin;

    /* Check if plugin has audio output - if not, output silence */
    const clap_plugin_audio_ports_t *audio_ports =
        (const clap_plugin_audio_ports_t *)plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS);

    uint32_t num_outputs = 0;
    uint32_t num_inputs = 0;
    if (audio_ports) {
        num_outputs = audio_ports->count(plugin, false);
        num_inputs = audio_ports->count(plugin, true);
    }

    /* If no audio output, just output silence */
    if (num_outputs == 0) {
        memset(out, 0, frames * 2 * sizeof(float));
        return 0;
    }

    ensure_buffers(frames);

    /* De-interleave input if provided and plugin has inputs */
    if (in && num_inputs > 0) {
        for (int i = 0; i < frames; i++) {
            s_in_bufs[0][i] = in[i * 2];
            s_in_bufs[1][i] = in[i * 2 + 1];
        }
    } else {
        memset(s_in_bufs[0], 0, frames * sizeof(float));
        memset(s_in_bufs[1], 0, frames * sizeof(float));
    }

    /* Clear output buffers */
    memset(s_out_bufs[0], 0, frames * sizeof(float));
    memset(s_out_bufs[1], 0, frames * sizeof(float));

    /* Setup audio buffers */
    clap_audio_buffer_t audio_in = {
        .data32 = s_in_bufs,
        .data64 = NULL,
        .channel_count = 2,
        .latency = 0,
        .constant_mask = 0
    };

    clap_audio_buffer_t audio_out = {
        .data32 = s_out_bufs,
        .data64 = NULL,
        .channel_count = 2,
        .latency = 0,
        .constant_mask = 0
    };

    /* Prepare MIDI events from queue */
    prepare_midi_events();

    /* Prepare param events from instance queue */
    prepare_param_events(inst);

    /* Event lists with queued MIDI and param events */
    clap_input_events_t in_events = {
        .ctx = NULL,
        .size = s_events_size,
        .get = s_events_get
    };
    clap_output_events_t out_events = {
        .ctx = NULL,
        .try_push = s_empty_push
    };

    /* Setup process struct */
    clap_process_t process = {
        .steady_time = -1,
        .frames_count = (uint32_t)frames,
        .transport = NULL,
        .audio_inputs = (num_inputs > 0) ? &audio_in : NULL,
        .audio_outputs = &audio_out,
        .audio_inputs_count = (num_inputs > 0) ? 1 : 0,
        .audio_outputs_count = 1,
        .in_events = &in_events,
        .out_events = &out_events
    };

    /* Process */
    clap_process_status status = plugin->process(plugin, &process);
    if (status == CLAP_PROCESS_ERROR) {
        return -1;
    }

    /* Interleave output */
    for (int i = 0; i < frames; i++) {
        out[i * 2] = s_out_bufs[0][i];
        out[i * 2 + 1] = s_out_bufs[1][i];
    }

    return 0;
}

int clap_param_count(clap_instance_t *inst) {
    if (!inst->plugin) return 0;

    const clap_plugin_t *plugin = (const clap_plugin_t *)inst->plugin;
    const clap_plugin_params_t *params =
        (const clap_plugin_params_t *)plugin->get_extension(plugin, CLAP_EXT_PARAMS);
    if (!params) return 0;

    return params->count(plugin);
}

int clap_param_info(clap_instance_t *inst, int index, char *name, int name_len, double *min, double *max, double *def) {
    if (!inst->plugin) return -1;

    const clap_plugin_t *plugin = (const clap_plugin_t *)inst->plugin;
    const clap_plugin_params_t *params =
        (const clap_plugin_params_t *)plugin->get_extension(plugin, CLAP_EXT_PARAMS);
    if (!params) return -1;

    clap_param_info_t info;
    if (!params->get_info(plugin, index, &info)) return -1;

    if (name && name_len > 0) {
        strncpy(name, info.name, name_len - 1);
        name[name_len - 1] = '\0';
    }
    if (min) *min = info.min_value;
    if (max) *max = info.max_value;
    if (def) *def = info.default_value;

    return 0;
}

int clap_param_set(clap_instance_t *inst, int index, double value) {
    if (!inst || !inst->plugin) return -1;

    const clap_plugin_t *plugin = (const clap_plugin_t *)inst->plugin;
    const clap_plugin_params_t *params =
        (const clap_plugin_params_t *)plugin->get_extension(plugin, CLAP_EXT_PARAMS);
    if (!params) return -1;

    /* Get param info to find the actual param_id */
    clap_param_info_t info;
    if (!params->get_info(plugin, index, &info)) return -1;

    /* Queue the param change for next process block */
    if (inst->param_queue_count < CLAP_MAX_PARAM_CHANGES) {
        inst->param_queue[inst->param_queue_count].param_id = info.id;
        inst->param_queue[inst->param_queue_count].value = value;
        inst->param_queue_count++;
    }

    return 0;
}

double clap_param_get(clap_instance_t *inst, int index) {
    if (!inst->plugin) return 0.0;

    const clap_plugin_t *plugin = (const clap_plugin_t *)inst->plugin;
    const clap_plugin_params_t *params =
        (const clap_plugin_params_t *)plugin->get_extension(plugin, CLAP_EXT_PARAMS);
    if (!params) return 0.0;

    clap_param_info_t info;
    if (!params->get_info(plugin, index, &info)) return 0.0;

    double value = 0.0;
    if (params->get_value(plugin, info.id, &value)) {
        return value;
    }
    return info.default_value;
}

int clap_send_midi(clap_instance_t *inst, const uint8_t *msg, int len) {
    if (!msg || len < 1 || len > 3) return -1;

    pthread_mutex_lock(&s_midi_mutex);
    if (s_midi_queue_count < MAX_MIDI_EVENTS) {
        midi_event_t *evt = &s_midi_queue[s_midi_queue_count++];
        evt->len = len;
        for (int i = 0; i < len; i++) {
            evt->data[i] = msg[i];
        }
    }
    pthread_mutex_unlock(&s_midi_mutex);

    (void)inst;
    return 0;
}
