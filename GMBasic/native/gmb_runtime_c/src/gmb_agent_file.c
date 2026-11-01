/* gmb_agent_file.c — F6 multimodale (2026-09-11): agent ask con input multimodale.
 *
 * Separato da gmb_agent.c per NON creare una dipendenza link-time da gmb_doc_c su tutto il
 * runtime: gmb_agent.o è tirato anche da test non-documento (via gmb_agent_ask); questo file
 * è tirato solo quando il programma usa AI.AGENTASKFILE (il harness C99Native linka gmb_doc_c
 * e gmb_agent_runtime in quel caso). §0bis: chiama solo il runtime condiviso.
 */
#include "gmb_runtime.h"
#include "gmb_state_hook.h"   /* hook opzionale di stato (client managed) */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* gmb_doc_extract vive in gmb_doc_c (header non incluso qui): dichiarazione locale. */
char* gmb_doc_extract(const char* path);

/* gmb_agent_ask_file(name, path, question): instrada il file a vision (immagine), STT (audio)
 * o estrazione documento (pdf/testo), poi passa il testo a gmb_agent_ask. File mancante /
 * contenuto non estraibile → errore esplicito (heap, il chiamante copia). */
char* gmb_agent_ask_file(const char* name, const char* path, const char* question) {
    { char* r = gmb_state_hook_query3("agent.askfile", name, path, question); if (r) return r; }
    char* content = NULL;
    char* prompt = NULL;
    char* result = NULL;
    const char* dot;
    size_t n;
    if (!path || !path[0]) { char m[512]; snprintf(m, sizeof(m), "ERROR: file non trovato: %s", path ? path : ""); return strdup(m); }
    {
        FILE* f = fopen(path, "rb");
        if (!f) { char m[1024]; snprintf(m, sizeof(m), "ERROR: file non trovato: %s", path); return strdup(m); }
        fclose(f);
    }
    dot = strrchr(path, '.');
    if (dot && (strcasecmp(dot, ".png") == 0 || strcasecmp(dot, ".jpg") == 0
             || strcasecmp(dot, ".jpeg") == 0 || strcasecmp(dot, ".bmp") == 0
             || strcasecmp(dot, ".gif") == 0 || strcasecmp(dot, ".webp") == 0
             || strcasecmp(dot, ".tif") == 0 || strcasecmp(dot, ".tiff") == 0)) {
        content = gmb_ai_analyze_image(path, "Descrivi il contenuto dell'immagine.");
    } else if (dot && (strcasecmp(dot, ".wav") == 0 || strcasecmp(dot, ".mp3") == 0
             || strcasecmp(dot, ".m4a") == 0 || strcasecmp(dot, ".ogg") == 0
             || strcasecmp(dot, ".flac") == 0 || strcasecmp(dot, ".aac") == 0)) {
        content = strdup(gmb_ai_audio_transcribe(path, "", 0));
    } else {
        content = gmb_doc_extract(path);
    }
    if (!content || !content[0]) {
        free(content);
        { char m[1024]; snprintf(m, sizeof(m), "ERROR: contenuto non estraibile: %s", path); return strdup(m); }
    }
    n = (question ? strlen(question) : 0) + strlen(content) + 4;
    prompt = (char*)malloc(n);
    if (!prompt) { free(content); return strdup("ERROR: memoria"); }
    if (question && question[0]) snprintf(prompt, n, "%s\n\n%s", question, content);
    else snprintf(prompt, n, "%s", content);
    free(content);
    result = gmb_agent_ask(name, prompt);
    free(prompt);
    return result;
}
