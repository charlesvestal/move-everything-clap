/*
 * Test category-based plugin browser ordering and category jumps.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "dsp/clap_host.h"
#include "chain_audio_fx/category_browser.h"

static void set_plugin(clap_plugin_info_t *p, const char *name, const char *category, int has_audio_in) {
    memset(p, 0, sizeof(*p));
    strncpy(p->name, name, sizeof(p->name) - 1);
    strncpy(p->category, category, sizeof(p->category) - 1);
    p->has_audio_in = has_audio_in;
}

int main(void) {
    clap_plugin_info_t items[6] = {0};
    clap_host_list_t list = {
        .items = items,
        .count = 6,
        .capacity = 6
    };

    /* Include one non-audio plugin to ensure FX browser filters it out. */
    set_plugin(&items[0], "Zeta Verb", "Reverb", 1);
    set_plugin(&items[1], "Alpha Verb", "Reverb", 1);
    set_plugin(&items[2], "Crunch", "Saturation", 1);
    set_plugin(&items[3], "Limiter", "Dynamics", 1);
    set_plugin(&items[4], "No Cat", "", 1);
    set_plugin(&items[5], "Synth", "Synth", 0);

    clap_fx_browser_index_t idx = {0};
    int rc = clap_fx_build_browser_index(&list, &idx);
    assert(rc == 0);

    /* Audio-only entries sorted by category, then alphabetical within category. */
    assert(idx.display_count == 5);
    assert(strcmp(list.items[idx.display_to_raw[0]].name, "Limiter") == 0);
    assert(strcmp(list.items[idx.display_to_raw[1]].name, "Alpha Verb") == 0);
    assert(strcmp(list.items[idx.display_to_raw[2]].name, "Zeta Verb") == 0);
    assert(strcmp(list.items[idx.display_to_raw[3]].name, "Crunch") == 0);
    assert(strcmp(list.items[idx.display_to_raw[4]].name, "No Cat") == 0);

    /* Category list should include Other for uncategorized items. */
    assert(idx.category_count == 4);
    assert(strcmp(idx.categories[0].name, "Dynamics") == 0);
    assert(strcmp(idx.categories[1].name, "Reverb") == 0);
    assert(strcmp(idx.categories[2].name, "Saturation") == 0);
    assert(strcmp(idx.categories[3].name, "Other") == 0);

    /* Jump target should be category start. */
    assert(clap_fx_jump_display_index_for_category(&idx, 1) == idx.categories[1].start_index);

    printf("All tests passed!\n");
    return 0;
}
