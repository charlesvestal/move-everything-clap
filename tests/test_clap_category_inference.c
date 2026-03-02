/*
 * Test fallback category inference when CLAP subtype features are missing.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Implemented in clap_host.c */
void clap_infer_category_from_metadata(const char *name,
                                       const char *description,
                                       const char *const *features,
                                       char *category,
                                       int category_len);

int main(void) {
    char category[64] = {0};

    const char *reverb_features[] = {"audio-effect", "reverb", NULL};
    clap_infer_category_from_metadata("NonMappedVerbity", "", reverb_features, category, sizeof(category));
    assert(strcmp(category, "Unsorted") == 0);

    const char *audio_only[] = {"audio-effect", NULL};
    clap_infer_category_from_metadata("UnknownThing", "airwindows Dynamics", audio_only, category, sizeof(category));
    assert(strcmp(category, "Unsorted") == 0);

    clap_infer_category_from_metadata("MegaChorus", "", audio_only, category, sizeof(category));
    assert(strcmp(category, "Unsorted") == 0);

    /* Airwindows names should use curated registry categories. */
    clap_infer_category_from_metadata("airwindows Chamber", "", audio_only, category, sizeof(category));
    assert(strcmp(category, "Reverb") == 0);

    clap_infer_category_from_metadata("airwindows ADClip7", "", audio_only, category, sizeof(category));
    assert(strcmp(category, "Clipping") == 0);

    clap_infer_category_from_metadata("airwindows Console5Channel", "", audio_only, category, sizeof(category));
    assert(strcmp(category, "Consoles") == 0);

    /* Bare names should also resolve through the Airwindows map. */
    clap_infer_category_from_metadata("Console5Channel", "", audio_only, category, sizeof(category));
    assert(strcmp(category, "Consoles") == 0);

    clap_infer_category_from_metadata("ConsoleHBuss", "", audio_only, category, sizeof(category));
    assert(strcmp(category, "Other") == 0);

    clap_infer_category_from_metadata("airwindows ClipOnly", "", audio_only, category, sizeof(category));
    assert(strcmp(category, "Clipping") == 0);

    clap_infer_category_from_metadata("airwindows NC-17", "", audio_only, category, sizeof(category));
    assert(strcmp(category, "Saturation") == 0);

    clap_infer_category_from_metadata("PlainProcessor", "", audio_only, category, sizeof(category));
    assert(strcmp(category, "Unsorted") == 0);

    printf("All tests passed!\n");
    return 0;
}
