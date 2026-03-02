/*
 * Test CLAP plugin discovery category extraction
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "dsp/clap_host.h"

static const clap_plugin_info_t *find_by_name(const clap_host_list_t *list, const char *name) {
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->items[i].name, name) == 0) {
            return &list->items[i];
        }
    }
    return NULL;
}

int main(void) {
    printf("Testing CLAP category extraction...\n");

    clap_host_list_t list = {0};
    int rc = clap_scan_plugins("tests/fixtures/clap", &list);

    assert(rc == 0);
    assert(list.count >= 2);

    const clap_plugin_info_t *fx = find_by_name(&list, "Test FX");
    const clap_plugin_info_t *synth = find_by_name(&list, "Test Synth");

    assert(fx != NULL);
    assert(synth != NULL);

    /* test fixtures are not in Airwindows map, so they should fall back to Unsorted */
    assert(strcmp(fx->category, "Unsorted") == 0);
    assert(strcmp(synth->category, "Unsorted") == 0);

    clap_free_plugin_list(&list);
    printf("All tests passed!\n");
    return 0;
}
