/* gmb_observe_test.c — A5: fonte OBSERVE TEST (`AI OBSERVE TEST "<path.bas>"`).
 *
 * UNITÀ SEPARATA da gmb_observe_sources.c perché usa il COMPILATORE C (gmb_compile_bas_file):
 * tenendola isolata, chi usa solo DATABASE/PROCESS/BROWSER non tira dentro il compilatore nel link
 * (stessa lezione di gmb_verify_require.c, 2026-09-16).
 *
 * SPECCHIO ESATTO del C# ObservationSources.ObserveTest: stesso artefatto JSON, stesso esito
 * (compila con lo STESSO strumento di `AI VERIFY … COMPILE`), -1 se non compila → errore esplicito.
 */
#include "gmb_runtime.h"
#include "gmb_observation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>

#define GMD_OBS_TEST_ARTIFACT "work/ai_observe_test.json"

extern int gmb_compile_bas_file(const char* path, uint8_t** out_data, size_t* out_len, char** diag);

double gmb_observe_test(const char* path)
{
    if (!path || !*path) return -1;
    uint8_t* data = NULL; size_t len = 0; char* diag = NULL;
    int rc = gmb_compile_bas_file(path, &data, &len, &diag);
    if (data) free(data);
    if (diag) free(diag);
    if (rc != 0) return -1;   /* non compila: errore esplicito a monte */
    char dir[1024];
    snprintf(dir, sizeof(dir), "%s", GMD_OBS_TEST_ARTIFACT);
    char* slash = strrchr(dir, '/');
    if (slash) { *slash = 0; if (dir[0]) mkdir(dir, 0755); }
    FILE* f = fopen(GMD_OBS_TEST_ARTIFACT, "w");
    if (!f) return -1;
    fprintf(f, "{\"path\":\"%s\",\"ok\":true,\"diagnostics\":\"\"}\n", path);
    fclose(f);
    return (double)gmb_obs_record_typed("test", path, GMD_OBS_TEST_ARTIFACT);
}
