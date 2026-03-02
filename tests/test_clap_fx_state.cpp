#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

extern "C" {
#include "dsp/clap_host.h"
}

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

typedef struct audio_fx_api_v2 {
    uint32_t api_version;
    void* (*create_instance)(const char *module_dir, const char *config_json);
    void (*destroy_instance)(void *instance);
    void (*process_block)(void *instance, int16_t *audio_inout, int frames);
    void (*set_param)(void *instance, const char *key, const char *val);
    int (*get_param)(void *instance, const char *key, char *buf, int buf_len);
} audio_fx_api_v2_t;

extern "C" audio_fx_api_v2_t* move_audio_fx_init_v2(const host_api_v1_t *host);

static void test_log(const char *msg) {
    (void)msg;
}

static int test_midi_send(const uint8_t *msg, int len) {
    (void)msg;
    (void)len;
    return 0;
}

static int copy_file(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return -1;
    FILE *out = fopen(dst, "wb");
    if (!out) {
        fclose(in);
        return -1;
    }

    char buf[4096];
    size_t n = 0;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if (fwrite(buf, 1, n, out) != n) {
            fclose(in);
            fclose(out);
            return -1;
        }
    }

    fclose(in);
    fclose(out);
    return 0;
}

int main(void) {
    printf("Testing CLAP FX state round-trip...\n");

    char tmp_template[] = "/tmp/move-anything-clap-state-XXXXXX";
    char *tmp_root = mkdtemp(tmp_template);
    assert(tmp_root != NULL);

    char plugins_dir[512];
    snprintf(plugins_dir, sizeof(plugins_dir), "%s/plugins", tmp_root);
    assert(mkdir(plugins_dir, 0755) == 0);

    char dst1[512];
    char dst2[512];
    snprintf(dst1, sizeof(dst1), "%s/test_param.clap", plugins_dir);
    snprintf(dst2, sizeof(dst2), "%s/test_fx.clap", plugins_dir);
    assert(copy_file("tests/fixtures/clap/test_param.clap", dst1) == 0);
    assert(copy_file("tests/fixtures/clap/test_fx.clap", dst2) == 0);

    clap_host_list_t scanned = {0};
    assert(clap_scan_plugins(plugins_dir, &scanned) == 0);
    assert(scanned.count >= 2);

    const char *target_plugin_id = scanned.items[1].id;
    assert(target_plugin_id && target_plugin_id[0]);

    host_api_v1_t host = {0};
    host.api_version = 1;
    host.sample_rate = 44100;
    host.frames_per_block = 128;
    host.log = test_log;
    host.midi_send_internal = test_midi_send;
    host.midi_send_external = test_midi_send;

    audio_fx_api_v2_t *api = move_audio_fx_init_v2(&host);
    assert(api != NULL);
    assert(api->create_instance != NULL);
    assert(api->set_param != NULL);
    assert(api->get_param != NULL);
    assert(api->destroy_instance != NULL);

    void *inst = api->create_instance(tmp_root, NULL);
    assert(inst != NULL);

    api->set_param(inst, "plugin_id", target_plugin_id);
    api->set_param(inst, "param_0", "0.420");

    char state_buf[4096];
    int len = api->get_param(inst, "state", state_buf, sizeof(state_buf));
    assert(len > 0);
    assert(strstr(state_buf, "\"plugin_id\"") != NULL);
    assert(strstr(state_buf, target_plugin_id) != NULL);
    assert(strstr(state_buf, "\"params\"") != NULL);

    /* Regression guard: keep knob-editable params at root preset level. */
    char hierarchy_buf[4096];
    len = api->get_param(inst, "ui_hierarchy", hierarchy_buf, sizeof(hierarchy_buf));
    assert(len > 0);
    assert(strstr(hierarchy_buf,
                  "\"root\":{\"list_param\":\"plugin_index\",\"count_param\":\"plugin_count\",\"name_param\":\"plugin_name\",\"children\":null,") != NULL);
    assert(strstr(hierarchy_buf,
                  "\"params\":[\"param_0\",\"param_1\",\"param_2\",\"param_3\",\"param_4\",\"param_5\",\"param_6\",\"param_7\",{\"level\":\"category_jump\",\"label\":\"Jump to Category\"}]") != NULL);

    api->destroy_instance(inst);
    clap_free_plugin_list(&scanned);

    printf("State round-trip test passed.\n");
    return 0;
}
