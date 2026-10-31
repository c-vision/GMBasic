/* gmb_computer_provider_windows.c — WindowsComputerProvider (SendInput + UIA) */
#include "gmb_computer_provider.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int win_screenshot(void *impl, const char *out_path) {
    (void)impl; (void)out_path;
    fprintf(stderr, "[computer/win] screenshot not yet implemented (stub, BitBlt)\n");
    return -1;
}
static int win_click(void *impl, double x, double y) {
    (void)impl; (void)x; (void)y;
    fprintf(stderr, "[computer/win] click not yet implemented (SendInput)\n");
    return -1;
}
static int win_screenrecord_start(void *impl, const char *out_path, int fps, const char *region) { (void)impl; (void)out_path; (void)fps; (void)region; fprintf(stderr, "[computer/win] screenrecord not yet implemented (ffmpeg gdigrab)\n"); return -1; }
static int win_screenrecord_stop(void *impl, char *out_json, size_t cap) { (void)impl; snprintf(out_json, cap, "{\"error\":\"not recording\"}"); return -1; }
static int win_screenrecord_status(void *impl, char *out_json, size_t cap) { (void)impl; snprintf(out_json, cap, "{\"isRecording\":false}"); return 0; }
static gmb_computer_provider_vt win_vt = {
    .name = "windows",
    .screenshot = win_screenshot,
    .click = win_click,
    .screenrecord_start = win_screenrecord_start,
    .screenrecord_stop = win_screenrecord_stop,
    .screenrecord_status = win_screenrecord_status,
};

gmb_computer_provider_t *gmb_computer_provider_windows_new(void) {
    gmb_computer_provider_t *p = (gmb_computer_provider_t*)calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->vt = &win_vt;
    strncpy(p->name, "windows", sizeof(p->name)-1);
    return p;
}
