/* gmb_computer_provider.c — Registry cross-OS */
#include "gmb_computer_provider.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

gmb_computer_provider_t *gmb_computer_provider_resolve(const char *requested) {
    if (!requested || !*requested || strcmp(requested, "auto") == 0) {
#if defined(__APPLE__)
        return gmb_computer_provider_mac_new();
#elif defined(_WIN32)
        return gmb_computer_provider_windows_new();
#elif defined(__linux__)
        return gmb_computer_provider_linux_new();
#else
        return gmb_computer_provider_mac_new();
#endif
    }
    if (strcasecmp(requested, "mac") == 0 || strcasecmp(requested, "macos") == 0) return gmb_computer_provider_mac_new();
    if (strcasecmp(requested, "windows") == 0 || strcasecmp(requested, "win") == 0) return gmb_computer_provider_windows_new();
    if (strcasecmp(requested, "linux") == 0) return gmb_computer_provider_linux_new();
    if (strncasecmp(requested, "remote", 6) == 0) return gmb_computer_provider_remote_new(requested);
    return NULL;
}

void gmb_computer_provider_free(gmb_computer_provider_t *p) {
    if (p) free(p);
}

gmb_computer_provider_t *gmb_computer_provider_remote_new(const char *url) {
    (void)url;
    fprintf(stderr, "[computer/remote] not yet implemented\n");
    return NULL;
}
