/* gmb_computer_provider_linux.c — LinuxComputerProvider (XTest/AT-SPI/Wayland) */
#include "gmb_computer_provider.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static int linux_screenshot(void *impl, const char *out_path) {
    (void)impl; (void)out_path;
    fprintf(stderr, "[computer/linux] screenshot not yet implemented (X11/Wayland)\n");
    return -1;
}
static int linux_click(void *impl, double x, double y) {
    (void)impl; (void)x; (void)y;
    fprintf(stderr, "[computer/linux] click not yet implemented (XTest/libei)\n");
    return -1;
}
static int linux_screenrecord_start(void *impl, const char *out_path, int fps, const char *region) { (void)impl; (void)out_path; (void)fps; (void)region; fprintf(stderr, "[computer/linux] screenrecord not yet implemented (ffmpeg x11grab/pipewire)\n"); return -1; }
static int linux_screenrecord_stop(void *impl, char *out_json, size_t cap) { (void)impl; snprintf(out_json, cap, "{\"error\":\"not recording\"}"); return -1; }
static int linux_screenrecord_status(void *impl, char *out_json, size_t cap) { (void)impl; snprintf(out_json, cap, "{\"isRecording\":false}"); return 0; }
static gmb_computer_provider_vt linux_vt = {
    .name = "linux",
    .screenshot = linux_screenshot,
    .click = linux_click,
    .screenrecord_start = linux_screenrecord_start,
    .screenrecord_stop = linux_screenrecord_stop,
    .screenrecord_status = linux_screenrecord_status,
};

gmb_computer_provider_t *gmb_computer_provider_linux_new(void) {
    gmb_computer_provider_t *p = (gmb_computer_provider_t*)calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->vt = &linux_vt;
    strncpy(p->name, "linux", sizeof(p->name)-1);
    return p;
}
