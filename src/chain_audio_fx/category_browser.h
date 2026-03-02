#ifndef CLAP_FX_CATEGORY_BROWSER_H
#define CLAP_FX_CATEGORY_BROWSER_H

#include "dsp/clap_host.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CLAP_FX_MAX_CATEGORIES 64

typedef struct clap_fx_category {
    char name[64];
    int start_index;
    int count;
} clap_fx_category_t;

typedef struct clap_fx_browser_index {
    int display_count;
    int display_to_raw[CLAP_HOST_MAX_PLUGINS];
    int raw_to_display[CLAP_HOST_MAX_PLUGINS];
    int category_count;
    clap_fx_category_t categories[CLAP_FX_MAX_CATEGORIES];
} clap_fx_browser_index_t;

int clap_fx_build_browser_index(const clap_host_list_t *list, clap_fx_browser_index_t *out);
int clap_fx_category_index_for_display(const clap_fx_browser_index_t *index, int display_index);
int clap_fx_jump_display_index_for_category(const clap_fx_browser_index_t *index, int category_index);
const char *clap_fx_category_name_for_display(const clap_fx_browser_index_t *index, int display_index);
int clap_fx_write_category_items_json(const clap_fx_browser_index_t *index, char *buf, int buf_len);

#ifdef __cplusplus
}
#endif

#endif
