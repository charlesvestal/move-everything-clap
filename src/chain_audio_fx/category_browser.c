#include "category_browser.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct sort_entry {
    int raw_index;
    char name[256];
    char category[64];
} sort_entry_t;

static int ascii_casecmp(const char *a, const char *b) {
    unsigned char ca;
    unsigned char cb;
    while (*a && *b) {
        ca = (unsigned char)tolower((unsigned char)*a);
        cb = (unsigned char)tolower((unsigned char)*b);
        if (ca != cb) return (int)ca - (int)cb;
        a++;
        b++;
    }
    ca = (unsigned char)tolower((unsigned char)*a);
    cb = (unsigned char)tolower((unsigned char)*b);
    return (int)ca - (int)cb;
}

static int is_other_category(const char *category) {
    return ascii_casecmp(category, "Other") == 0;
}

static int sort_entries(const void *lhs, const void *rhs) {
    const sort_entry_t *a = (const sort_entry_t*)lhs;
    const sort_entry_t *b = (const sort_entry_t*)rhs;

    int a_other = is_other_category(a->category);
    int b_other = is_other_category(b->category);
    if (a_other != b_other) {
        return a_other - b_other;
    }

    int cat_cmp = ascii_casecmp(a->category, b->category);
    if (cat_cmp != 0) return cat_cmp;

    return ascii_casecmp(a->name, b->name);
}

int clap_fx_build_browser_index(const clap_host_list_t *list, clap_fx_browser_index_t *out) {
    if (!list || !out) return -1;

    memset(out, 0, sizeof(*out));
    for (int i = 0; i < CLAP_HOST_MAX_PLUGINS; i++) {
        out->raw_to_display[i] = -1;
    }

    sort_entry_t entries[CLAP_HOST_MAX_PLUGINS] = {0};
    int entry_count = 0;

    for (int i = 0; i < list->count && i < CLAP_HOST_MAX_PLUGINS; i++) {
        const clap_plugin_info_t *info = &list->items[i];
        if (!info->has_audio_in) continue;

        entries[entry_count].raw_index = i;
        strncpy(entries[entry_count].name, info->name, sizeof(entries[entry_count].name) - 1);

        if (info->category[0]) {
            strncpy(entries[entry_count].category, info->category, sizeof(entries[entry_count].category) - 1);
        } else {
            strncpy(entries[entry_count].category, "Other", sizeof(entries[entry_count].category) - 1);
        }

        entry_count++;
    }

    qsort(entries, entry_count, sizeof(entries[0]), sort_entries);

    out->display_count = entry_count;
    int current_category = -1;
    char last_category[64] = "";

    for (int display = 0; display < entry_count; display++) {
        int raw_index = entries[display].raw_index;
        out->display_to_raw[display] = raw_index;
        out->raw_to_display[raw_index] = display;

        if (ascii_casecmp(last_category, entries[display].category) != 0) {
            if (out->category_count >= CLAP_FX_MAX_CATEGORIES) break;
            current_category = out->category_count;
            out->category_count++;
            strncpy(out->categories[current_category].name, entries[display].category,
                    sizeof(out->categories[current_category].name) - 1);
            out->categories[current_category].start_index = display;
            out->categories[current_category].count = 1;
            strncpy(last_category, entries[display].category, sizeof(last_category) - 1);
        } else if (current_category >= 0) {
            out->categories[current_category].count++;
        }
    }

    return 0;
}

int clap_fx_category_index_for_display(const clap_fx_browser_index_t *index, int display_index) {
    if (!index || display_index < 0 || display_index >= index->display_count) return -1;

    for (int i = 0; i < index->category_count; i++) {
        int start = index->categories[i].start_index;
        int end = start + index->categories[i].count;
        if (display_index >= start && display_index < end) {
            return i;
        }
    }

    return -1;
}

int clap_fx_jump_display_index_for_category(const clap_fx_browser_index_t *index, int category_index) {
    if (!index || category_index < 0 || category_index >= index->category_count) return -1;
    return index->categories[category_index].start_index;
}

const char *clap_fx_category_name_for_display(const clap_fx_browser_index_t *index, int display_index) {
    int category_index = clap_fx_category_index_for_display(index, display_index);
    if (category_index < 0) return "";
    return index->categories[category_index].name;
}

int clap_fx_write_category_items_json(const clap_fx_browser_index_t *index, char *buf, int buf_len) {
    if (!index || !buf || buf_len <= 0) return -1;

    int offset = snprintf(buf, buf_len, "[");
    if (offset < 0 || offset >= buf_len) return -1;

    for (int i = 0; i < index->category_count; i++) {
        const clap_fx_category_t *cat = &index->categories[i];
        if (i > 0) {
            int wrote = snprintf(buf + offset, buf_len - offset, ",");
            if (wrote < 0 || wrote >= buf_len - offset) return -1;
            offset += wrote;
        }

        int wrote = snprintf(buf + offset, buf_len - offset,
                             "{\"index\":%d,\"label\":\"%s\"}",
                             i, cat->name);
        if (wrote < 0 || wrote >= buf_len - offset) return -1;
        offset += wrote;
    }

    int wrote = snprintf(buf + offset, buf_len - offset, "]");
    if (wrote < 0 || wrote >= buf_len - offset) return -1;
    offset += wrote;

    return offset;
}
