/*
 * CLAP Host Core - Plugin discovery, loading, and processing
 *
 * Shared by both the main sound generator module and the audio FX chain wrapper.
 */

#ifndef CLAP_HOST_H
#define CLAP_HOST_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum plugins per directory - increased for large bundles like Airwindows (498 plugins) */
#define CLAP_HOST_MAX_PLUGINS 512

/* Plugin metadata from scanning */
typedef struct clap_plugin_info {
    char id[256];
    char name[256];
    char vendor[256];
    char path[1024];       /* Full path to .clap file */
    int  plugin_index;     /* Index within the .clap bundle */
    bool has_audio_in;
    bool has_audio_out;
    bool has_midi_in;
    bool has_midi_out;
} clap_plugin_info_t;

/* List of discovered plugins */
typedef struct clap_host_list {
    clap_plugin_info_t *items;
    int count;
    int capacity;
} clap_host_list_t;

/* Per-instance param change queue */
#define CLAP_MAX_PARAM_CHANGES 32
typedef struct {
    uint32_t param_id;
    double value;
} clap_param_change_t;

/* Loaded plugin instance */
typedef struct clap_instance {
    void *handle;                    /* dlopen handle */
    const void *plugin;              /* clap_plugin_t* */
    const void *factory;             /* clap_plugin_factory_t* */
    const void *entry;               /* clap_plugin_entry_t* */
    char path[1024];
    bool activated;
    bool processing;
    /* Per-instance param change queue */
    clap_param_change_t param_queue[CLAP_MAX_PARAM_CHANGES];
    int param_queue_count;
} clap_instance_t;

/*
 * Scan a directory for .clap plugin files
 *
 * dir: Path to directory containing .clap files
 * out: Output list (caller should zero-initialize)
 * Returns: 0 on success, -1 on error
 */
int clap_scan_plugins(const char *dir, clap_host_list_t *out);

/*
 * Free a plugin list
 */
void clap_free_plugin_list(clap_host_list_t *list);

/*
 * Load a plugin instance
 *
 * path: Full path to .clap file
 * plugin_index: Index of plugin within the bundle (usually 0)
 * out: Output instance
 * Returns: 0 on success, -1 on error
 */
int clap_load_plugin(const char *path, int plugin_index, clap_instance_t *out);

/*
 * Unload a plugin instance
 */
void clap_unload_plugin(clap_instance_t *inst);

/*
 * Process an audio block
 *
 * inst: Loaded plugin instance
 * in: Input audio (float stereo interleaved), or NULL for synths
 * out: Output audio (float stereo interleaved)
 * frames: Number of frames to process
 * Returns: 0 on success, -1 on error
 */
int clap_process_block(clap_instance_t *inst, const float *in, float *out, int frames);

/*
 * Get parameter count
 */
int clap_param_count(clap_instance_t *inst);

/*
 * Get parameter info
 */
int clap_param_info(clap_instance_t *inst, int index, char *name, int name_len, double *min, double *max, double *def);

/*
 * Set parameter value
 */
int clap_param_set(clap_instance_t *inst, int index, double value);

/*
 * Get parameter value
 */
double clap_param_get(clap_instance_t *inst, int index);

/*
 * Send MIDI event to plugin
 */
int clap_send_midi(clap_instance_t *inst, const uint8_t *msg, int len);

#ifdef __cplusplus
}
#endif

#endif /* CLAP_HOST_H */
