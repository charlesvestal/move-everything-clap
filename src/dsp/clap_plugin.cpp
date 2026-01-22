/*
 * CLAP Host DSP Plugin for Move Anything
 *
 * Hosts arbitrary CLAP plugins as a sound generator module.
 * MIT License - see LICENSE file.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Include plugin API */
extern "C" {
#include <stdint.h>

#define MOVE_PLUGIN_API_VERSION 1
#define MOVE_SAMPLE_RATE 44100
#define MOVE_FRAMES_PER_BLOCK 128
#define MOVE_MIDI_SOURCE_INTERNAL 0
#define MOVE_MIDI_SOURCE_EXTERNAL 2

typedef struct host_api_v1 {
    uint32_t api_version;
    int sample_rate;
    int frames_per_block;
    uint8_t *mapped_memory;
    int audio_out_offset;
    int audio_in_offset;
    void (*log)(const char *msg);
    int (*midi_send_internal)(const uint8_t *msg, int len);
    int (*midi_send_external)(const uint8_t *msg, int len);
} host_api_v1_t;

typedef struct plugin_api_v1 {
    uint32_t api_version;
    int (*on_load)(const char *module_dir, const char *json_defaults);
    void (*on_unload)(void);
    void (*on_midi)(const uint8_t *msg, int len, int source);
    void (*set_param)(const char *key, const char *val);
    int (*get_param)(const char *key, char *buf, int buf_len);
    void (*render_block)(int16_t *out_interleaved_lr, int frames);
} plugin_api_v1_t;

/* === Plugin API v2 === */
#define MOVE_PLUGIN_API_VERSION_2 2

typedef struct plugin_api_v2 {
    uint32_t api_version;
    void* (*create_instance)(const char *module_dir, const char *json_defaults);
    void (*destroy_instance)(void *instance);
    void (*on_midi)(void *instance, const uint8_t *msg, int len, int source);
    void (*set_param)(void *instance, const char *key, const char *val);
    int (*get_param)(void *instance, const char *key, char *buf, int buf_len);
    int (*get_error)(void *instance, char *buf, int buf_len);
    void (*render_block)(void *instance, int16_t *out_interleaved_lr, int frames);
} plugin_api_v2_t;

typedef plugin_api_v2_t* (*move_plugin_init_v2_fn)(const host_api_v1_t *host);
#define MOVE_PLUGIN_INIT_V2_SYMBOL "move_plugin_init_v2"

#include "clap_host.h"
}

/* Constants */
#define MAX_PLUGINS 64
#define PLUGINS_SUBDIR "plugins"

/* Plugin state */
static const host_api_v1_t *g_host = NULL;
static plugin_api_v1_t g_plugin_api;

static clap_host_list_t g_plugin_list = {0};
static clap_instance_t g_current_plugin = {0};
static int g_selected_index = -1;
static char g_module_dir[256] = "";
static int g_octave_transpose = 0;

/* Parameter bank state */
static int g_param_bank = 0;
#define PARAMS_PER_BANK 8

/* Forward declarations */
static void plugin_log(const char *msg);
static void scan_plugins(void);
static void load_selected_plugin(void);

/* Log helper */
static void plugin_log(const char *msg) {
    if (g_host && g_host->log) {
        g_host->log(msg);
    }
    fprintf(stderr, "[CLAP] %s\n", msg);
}

/* Scan for plugins in the plugins subdirectory */
static void scan_plugins(void) {
    clap_free_plugin_list(&g_plugin_list);

    char plugins_dir[512];
    snprintf(plugins_dir, sizeof(plugins_dir), "%s/%s", g_module_dir, PLUGINS_SUBDIR);

    plugin_log("Scanning for CLAP plugins...");

    if (clap_scan_plugins(plugins_dir, &g_plugin_list) == 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Found %d plugins", g_plugin_list.count);
        plugin_log(msg);
    } else {
        plugin_log("Failed to scan plugins directory");
    }
}

/* Load the currently selected plugin */
static void load_selected_plugin(void) {
    /* Unload current plugin if any */
    if (g_current_plugin.plugin) {
        clap_unload_plugin(&g_current_plugin);
    }

    if (g_selected_index < 0 || g_selected_index >= g_plugin_list.count) {
        return;
    }

    clap_plugin_info_t *info = &g_plugin_list.items[g_selected_index];

    char msg[512];
    snprintf(msg, sizeof(msg), "Loading plugin: %s", info->name);
    plugin_log(msg);

    if (clap_load_plugin(info->path, info->plugin_index, &g_current_plugin) != 0) {
        plugin_log("Failed to load plugin");
        g_selected_index = -1;
    }
}

/* === Plugin API Implementation === */

static int on_load(const char *module_dir, const char *json_defaults) {
    plugin_log("CLAP Host module loading");

    strncpy(g_module_dir, module_dir, sizeof(g_module_dir) - 1);
    g_module_dir[sizeof(g_module_dir) - 1] = '\0';

    /* Scan for available plugins */
    scan_plugins();

    /* Auto-load first plugin if available */
    if (g_plugin_list.count > 0) {
        g_selected_index = 0;
        load_selected_plugin();
    }

    return 0;
}

static void on_unload(void) {
    plugin_log("CLAP Host module unloading");

    if (g_current_plugin.plugin) {
        clap_unload_plugin(&g_current_plugin);
    }
    clap_free_plugin_list(&g_plugin_list);
}

static void on_midi(const uint8_t *msg, int len, int source) {
    if (!g_current_plugin.plugin || len < 3) return;

    uint8_t status = msg[0] & 0xF0;
    uint8_t data1 = msg[1];
    uint8_t data2 = msg[2];

    /* Apply octave transpose to note messages */
    if (status == 0x90 || status == 0x80) {
        int note = data1 + (g_octave_transpose * 12);
        if (note < 0) note = 0;
        if (note > 127) note = 127;
        uint8_t transposed[3] = {msg[0], (uint8_t)note, data2};
        clap_send_midi(&g_current_plugin, transposed, 3);
    } else {
        clap_send_midi(&g_current_plugin, msg, len);
    }
}

static void set_param(const char *key, const char *val) {
    if (!key || !val) return;

    if (strcmp(key, "selected_plugin") == 0) {
        int idx = atoi(val);
        if (idx >= 0 && idx < g_plugin_list.count && idx != g_selected_index) {
            g_selected_index = idx;
            load_selected_plugin();
        }
    }
    else if (strcmp(key, "refresh") == 0) {
        scan_plugins();
    }
    else if (strcmp(key, "octave_transpose") == 0) {
        g_octave_transpose = atoi(val);
        if (g_octave_transpose < -2) g_octave_transpose = -2;
        if (g_octave_transpose > 2) g_octave_transpose = 2;
    }
    else if (strcmp(key, "param_bank") == 0) {
        g_param_bank = atoi(val);
    }
    else if (strncmp(key, "param_", 6) == 0) {
        /* Set CLAP parameter: param_<index> */
        int param_idx = atoi(key + 6);
        double value = atof(val);
        clap_param_set(&g_current_plugin, param_idx, value);
    }
}

static int get_param(const char *key, char *buf, int buf_len) {
    if (!key || !buf || buf_len <= 0) return -1;

    if (strcmp(key, "plugin_count") == 0) {
        return snprintf(buf, buf_len, "%d", g_plugin_list.count);
    }
    else if (strncmp(key, "plugin_name_", 12) == 0) {
        int idx = atoi(key + 12);
        if (idx >= 0 && idx < g_plugin_list.count) {
            return snprintf(buf, buf_len, "%s", g_plugin_list.items[idx].name);
        }
        return -1;
    }
    else if (strncmp(key, "plugin_id_", 10) == 0) {
        int idx = atoi(key + 10);
        if (idx >= 0 && idx < g_plugin_list.count) {
            return snprintf(buf, buf_len, "%s", g_plugin_list.items[idx].id);
        }
        return -1;
    }
    else if (strcmp(key, "selected_plugin") == 0) {
        return snprintf(buf, buf_len, "%d", g_selected_index);
    }
    else if (strcmp(key, "current_plugin_name") == 0) {
        if (g_selected_index >= 0 && g_selected_index < g_plugin_list.count) {
            return snprintf(buf, buf_len, "%s", g_plugin_list.items[g_selected_index].name);
        }
        return snprintf(buf, buf_len, "None");
    }
    else if (strcmp(key, "octave_transpose") == 0) {
        return snprintf(buf, buf_len, "%d", g_octave_transpose);
    }
    else if (strcmp(key, "param_bank") == 0) {
        return snprintf(buf, buf_len, "%d", g_param_bank);
    }
    else if (strcmp(key, "param_count") == 0) {
        return snprintf(buf, buf_len, "%d", clap_param_count(&g_current_plugin));
    }
    else if (strncmp(key, "param_name_", 11) == 0) {
        int idx = atoi(key + 11);
        char name[64] = "";
        if (clap_param_info(&g_current_plugin, idx, name, sizeof(name), NULL, NULL, NULL) == 0) {
            return snprintf(buf, buf_len, "%s", name);
        }
        return -1;
    }
    else if (strncmp(key, "param_value_", 12) == 0) {
        int idx = atoi(key + 12);
        double value = clap_param_get(&g_current_plugin, idx);
        return snprintf(buf, buf_len, "%.3f", value);
    }

    return -1;
}

static void render_block(int16_t *out_interleaved_lr, int frames) {
    if (!g_current_plugin.plugin) {
        /* No plugin loaded, output silence */
        memset(out_interleaved_lr, 0, frames * 2 * sizeof(int16_t));
        return;
    }

    /* Process through CLAP plugin */
    float float_out[MOVE_FRAMES_PER_BLOCK * 2];

    if (clap_process_block(&g_current_plugin, NULL, float_out, frames) != 0) {
        memset(out_interleaved_lr, 0, frames * 2 * sizeof(int16_t));
        return;
    }

    /* Convert float to int16 */
    for (int i = 0; i < frames * 2; i++) {
        float sample = float_out[i];
        /* Clamp to [-1, 1] */
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        out_interleaved_lr[i] = (int16_t)(sample * 32767.0f);
    }
}

/* === Plugin Entry Point === */

extern "C" plugin_api_v1_t* move_plugin_init_v1(const host_api_v1_t *host) {
    g_host = host;

    g_plugin_api.api_version = MOVE_PLUGIN_API_VERSION;
    g_plugin_api.on_load = on_load;
    g_plugin_api.on_unload = on_unload;
    g_plugin_api.on_midi = on_midi;
    g_plugin_api.set_param = set_param;
    g_plugin_api.get_param = get_param;
    g_plugin_api.render_block = render_block;

    return &g_plugin_api;
}

/* =====================================================================
 * Plugin API v2 - Instance-based API
 * ===================================================================== */

typedef struct {
    char module_dir[256];
    clap_host_list_t plugin_list;
    clap_instance_t current_plugin;
    int selected_index;
    int octave_transpose;
    int param_bank;
} clap_host_instance_t;

/* v2 helper: Log with host */
static void v2_plugin_log(const char *msg) {
    if (g_host && g_host->log) {
        g_host->log(msg);
    }
    fprintf(stderr, "[CLAP v2] %s\n", msg);
}

/* v2 helper: Scan for plugins */
static void v2_scan_plugins(clap_host_instance_t *inst) {
    clap_free_plugin_list(&inst->plugin_list);

    char plugins_dir[512];
    snprintf(plugins_dir, sizeof(plugins_dir), "%s/%s", inst->module_dir, PLUGINS_SUBDIR);

    v2_plugin_log("Scanning for CLAP plugins...");

    if (clap_scan_plugins(plugins_dir, &inst->plugin_list) == 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "Found %d plugins", inst->plugin_list.count);
        v2_plugin_log(msg);
    } else {
        v2_plugin_log("Failed to scan plugins directory");
    }
}

/* v2 helper: Load selected plugin */
static void v2_load_selected_plugin(clap_host_instance_t *inst) {
    if (inst->current_plugin.plugin) {
        clap_unload_plugin(&inst->current_plugin);
    }

    if (inst->selected_index < 0 || inst->selected_index >= inst->plugin_list.count) {
        return;
    }

    clap_plugin_info_t *info = &inst->plugin_list.items[inst->selected_index];

    char msg[512];
    snprintf(msg, sizeof(msg), "Loading plugin: %s", info->name);
    v2_plugin_log(msg);

    if (clap_load_plugin(info->path, info->plugin_index, &inst->current_plugin) != 0) {
        v2_plugin_log("Failed to load plugin");
        inst->selected_index = -1;
    }
}

/* v2 API: Create instance */
static void* v2_create_instance(const char *module_dir, const char *json_defaults) {
    clap_host_instance_t *inst = (clap_host_instance_t*)calloc(1, sizeof(clap_host_instance_t));
    if (!inst) return NULL;

    strncpy(inst->module_dir, module_dir, sizeof(inst->module_dir) - 1);
    inst->module_dir[sizeof(inst->module_dir) - 1] = '\0';
    inst->selected_index = -1;

    v2_scan_plugins(inst);

    if (inst->plugin_list.count > 0) {
        inst->selected_index = 0;
        v2_load_selected_plugin(inst);
    }

    fprintf(stderr, "CLAP v2: Instance created\n");
    return inst;
}

/* v2 API: Destroy instance */
static void v2_destroy_instance(void *instance) {
    clap_host_instance_t *inst = (clap_host_instance_t*)instance;
    if (!inst) return;

    if (inst->current_plugin.plugin) {
        clap_unload_plugin(&inst->current_plugin);
    }
    clap_free_plugin_list(&inst->plugin_list);
    free(inst);

    fprintf(stderr, "CLAP v2: Instance destroyed\n");
}

/* v2 API: MIDI handler */
static void v2_on_midi(void *instance, const uint8_t *msg, int len, int source) {
    clap_host_instance_t *inst = (clap_host_instance_t*)instance;
    if (!inst || !inst->current_plugin.plugin || len < 3) return;

    uint8_t status = msg[0] & 0xF0;
    uint8_t data1 = msg[1];
    uint8_t data2 = msg[2];

    if (status == 0x90 || status == 0x80) {
        int note = data1 + (inst->octave_transpose * 12);
        if (note < 0) note = 0;
        if (note > 127) note = 127;
        uint8_t transposed[3] = {msg[0], (uint8_t)note, data2};
        clap_send_midi(&inst->current_plugin, transposed, 3);
    } else {
        clap_send_midi(&inst->current_plugin, msg, len);
    }
}

/* v2 API: Set parameter */
static void v2_set_param(void *instance, const char *key, const char *val) {
    clap_host_instance_t *inst = (clap_host_instance_t*)instance;
    if (!inst || !key || !val) return;

    if (strcmp(key, "selected_plugin") == 0) {
        int idx = atoi(val);
        if (idx >= 0 && idx < inst->plugin_list.count && idx != inst->selected_index) {
            inst->selected_index = idx;
            v2_load_selected_plugin(inst);
        }
    }
    else if (strcmp(key, "refresh") == 0) {
        v2_scan_plugins(inst);
    }
    else if (strcmp(key, "octave_transpose") == 0) {
        inst->octave_transpose = atoi(val);
        if (inst->octave_transpose < -2) inst->octave_transpose = -2;
        if (inst->octave_transpose > 2) inst->octave_transpose = 2;
    }
    else if (strcmp(key, "param_bank") == 0) {
        inst->param_bank = atoi(val);
    }
    else if (strncmp(key, "param_", 6) == 0) {
        int param_idx = atoi(key + 6);
        double value = atof(val);
        clap_param_set(&inst->current_plugin, param_idx, value);
    }
}

/* v2 API: Get parameter */
static int v2_get_param(void *instance, const char *key, char *buf, int buf_len) {
    clap_host_instance_t *inst = (clap_host_instance_t*)instance;
    if (!inst || !key || !buf || buf_len <= 0) return -1;

    if (strcmp(key, "plugin_count") == 0) {
        return snprintf(buf, buf_len, "%d", inst->plugin_list.count);
    }
    else if (strncmp(key, "plugin_name_", 12) == 0) {
        int idx = atoi(key + 12);
        if (idx >= 0 && idx < inst->plugin_list.count) {
            return snprintf(buf, buf_len, "%s", inst->plugin_list.items[idx].name);
        }
        return -1;
    }
    else if (strncmp(key, "plugin_id_", 10) == 0) {
        int idx = atoi(key + 10);
        if (idx >= 0 && idx < inst->plugin_list.count) {
            return snprintf(buf, buf_len, "%s", inst->plugin_list.items[idx].id);
        }
        return -1;
    }
    else if (strcmp(key, "selected_plugin") == 0) {
        return snprintf(buf, buf_len, "%d", inst->selected_index);
    }
    else if (strcmp(key, "current_plugin_name") == 0) {
        if (inst->selected_index >= 0 && inst->selected_index < inst->plugin_list.count) {
            return snprintf(buf, buf_len, "%s", inst->plugin_list.items[inst->selected_index].name);
        }
        return snprintf(buf, buf_len, "None");
    }
    else if (strcmp(key, "octave_transpose") == 0) {
        return snprintf(buf, buf_len, "%d", inst->octave_transpose);
    }
    else if (strcmp(key, "param_bank") == 0) {
        return snprintf(buf, buf_len, "%d", inst->param_bank);
    }
    else if (strcmp(key, "param_count") == 0) {
        return snprintf(buf, buf_len, "%d", clap_param_count(&inst->current_plugin));
    }
    else if (strncmp(key, "param_name_", 11) == 0) {
        int idx = atoi(key + 11);
        char name[64] = "";
        if (clap_param_info(&inst->current_plugin, idx, name, sizeof(name), NULL, NULL, NULL) == 0) {
            return snprintf(buf, buf_len, "%s", name);
        }
        return -1;
    }
    else if (strncmp(key, "param_value_", 12) == 0) {
        int idx = atoi(key + 12);
        double value = clap_param_get(&inst->current_plugin, idx);
        return snprintf(buf, buf_len, "%.3f", value);
    }

    return -1;
}

/* v2 API: Render audio */
static void v2_render_block(void *instance, int16_t *out_interleaved_lr, int frames) {
    clap_host_instance_t *inst = (clap_host_instance_t*)instance;
    if (!inst || !inst->current_plugin.plugin) {
        memset(out_interleaved_lr, 0, frames * 2 * sizeof(int16_t));
        return;
    }

    float float_out[MOVE_FRAMES_PER_BLOCK * 2];

    if (clap_process_block(&inst->current_plugin, NULL, float_out, frames) != 0) {
        memset(out_interleaved_lr, 0, frames * 2 * sizeof(int16_t));
        return;
    }

    for (int i = 0; i < frames * 2; i++) {
        float sample = float_out[i];
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        out_interleaved_lr[i] = (int16_t)(sample * 32767.0f);
    }
}

/* CLAP host doesn't have load errors (plugins are scanned dynamically) */
static int v2_get_error(void *instance, char *buf, int buf_len) {
    (void)instance;
    (void)buf;
    (void)buf_len;
    return 0;  /* No error */
}

/* v2 API table */
static plugin_api_v2_t g_plugin_api_v2;

extern "C" plugin_api_v2_t* move_plugin_init_v2(const host_api_v1_t *host) {
    g_host = host;

    memset(&g_plugin_api_v2, 0, sizeof(g_plugin_api_v2));
    g_plugin_api_v2.api_version = MOVE_PLUGIN_API_VERSION_2;
    g_plugin_api_v2.create_instance = v2_create_instance;
    g_plugin_api_v2.destroy_instance = v2_destroy_instance;
    g_plugin_api_v2.on_midi = v2_on_midi;
    g_plugin_api_v2.set_param = v2_set_param;
    g_plugin_api_v2.get_param = v2_get_param;
    g_plugin_api_v2.get_error = v2_get_error;
    g_plugin_api_v2.render_block = v2_render_block;

    fprintf(stderr, "CLAP v2 API initialized\n");
    return &g_plugin_api_v2;
}
