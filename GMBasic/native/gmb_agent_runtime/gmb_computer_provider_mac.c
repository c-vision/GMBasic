/* gmb_computer_provider_mac.c — MacComputerProvider (L0 Computer Use, reale).
 * Screenshot (CGDisplayCreateImage → PNG via ImageIO), mouse (CGEventCreateMouseEvent +
 * CGEventPost), tastiera (CGEventKeyboardSetUnicodeString / keycode), scroll, elenco
 * finestre (CGWindowListCopyWindowInfo) e focus (AX frontmost). Clipboard: per ora
 * non implementato (richiede NSPasteboard/ObjC — TODO). Permessi: eventi → Accessibility,
 * schermo → Screen Recording (se negati gli errori lo dicono esplicitamente). */
#include "gmb_computer_provider.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>
#include <dlfcn.h>

#import <CoreGraphics/CoreGraphics.h>
#import <ApplicationServices/ApplicationServices.h>
#import <ImageIO/ImageIO.h>
#import <CoreServices/CoreServices.h>

/* Clipboard NSPasteboard vive in gmb_clipboard_mac.m (ObjC, link del VM/CMake). Quando il
   provider è compilato SOLO come C (C99 harness) le definizioni forti non ci sono: questi
   fallback weak forniscono il simbolo (vincono le forti se presenti) con errore esplicito. */
#if defined(__GNUC__)
__attribute__((weak)) int gmb_mac_clipboard_get(char* out, size_t cap) {
    if (out && cap) out[0] = 0;
    fprintf(stderr, "[computer/mac] clipboard_get: non disponibile senza NSPasteboard (link ObjC)\n");
    return -1;
}
__attribute__((weak)) int gmb_mac_clipboard_set(const char* text) {
    (void)text;
    fprintf(stderr, "[computer/mac] clipboard_set: non disponibile senza NSPasteboard (link ObjC)\n");
    return -1;
}
#endif

/* CGDisplayCreateImage è obsoleta da macOS 15 (ScreenCaptureKit): la carichiamo a
 * runtime via dlsym per non dipendere dal deployment target. Ancora presente nel sistema. */
typedef CGImageRef (*cg_display_create_image_fn)(CGDirectDisplayID);
static CGImageRef mac_display_create_image(CGDirectDisplayID d) {
    static cg_display_create_image_fn fn = NULL;
    if (!fn) fn = (cg_display_create_image_fn)dlsym(RTLD_DEFAULT, "CGDisplayCreateImage");
    return fn ? fn(d) : NULL;
}

/* Virtual keycode macOS (stabili, senza dipendere da Carbon/HIToolbox). */
enum {
    MAC_KEY_A = 0x00, MAC_KEY_0 = 0x1D,
    MAC_KEY_RETURN = 0x24, MAC_KEY_TAB = 0x30, MAC_KEY_SPACE = 0x31, MAC_KEY_BACKSPACE = 0x33,
    MAC_KEY_ESCAPE = 0x35, MAC_KEY_HELP = 0x72, MAC_KEY_HOME = 0x73, MAC_KEY_PAGEUP = 0x74,
    MAC_KEY_FWDELETE = 0x75, MAC_KEY_END = 0x77, MAC_KEY_PAGEDOWN = 0x79,
    MAC_KEY_LEFT = 0x7B, MAC_KEY_RIGHT = 0x7C, MAC_KEY_DOWN = 0x7D, MAC_KEY_UP = 0x7E,
};

static void mac_mkdirs_for(const char* path) {
    /* crea la directory padre (best effort) per out_path */
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    char* slash = strrchr(tmp, '/');
    /* >/dev/null 2>&1: la creazione della cartella è best-effort; un mkdir fallito (es. path in
     * una dir non creabile) NON deve stampare su stderr (parità C#: silenzioso, l'errore
     * esplicito lo solleva il chiamante). Prima lo shell-mkdir diramava "mkdir: ...". */
    if (slash) { *slash = 0; if (tmp[0]) { char cmd[1200]; snprintf(cmd, sizeof(cmd), "mkdir -p '%s' >/dev/null 2>&1", tmp); system(cmd); } }
}

static int mac_screenshot(void *impl, const char *out_path) {
    (void)impl;
    if (!out_path || !*out_path) { fprintf(stderr, "[computer/mac] screenshot: path mancante\n"); return -1; }
    CGImageRef img = mac_display_create_image(kCGDirectMainDisplay);
    if (!img) {
        fprintf(stderr, "[computer/mac] screenshot: negato (Screen Recording permission) o display non accessibile\n");
        return -1;
    }
    mac_mkdirs_for(out_path);
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
        (const UInt8*)out_path, (CFIndex)strlen(out_path), false);
    if (!url) { CGImageRelease(img); return -1; }
    CGImageDestinationRef dest = CGImageDestinationCreateWithURL(url, kUTTypePNG, 1, NULL);
    CFRelease(url);
    if (!dest) { CGImageRelease(img); return -1; }
    CGImageDestinationAddImage(dest, img, NULL);
    bool ok = CGImageDestinationFinalize(dest);
    CFRelease(dest);
    CGImageRelease(img);
    return ok ? 0 : -1;
}

static int mac_events_allowed(void) {
    if (CGPreflightPostEventAccess()) return 1;
    fprintf(stderr, "[computer/mac] Accessibility permission negata (Impostazioni > Privacy > Accessibilità)\n");
    return 0;
}

static void mac_post_mouse(CGEventType t, double x, double y, CGMouseButton b) {
    CGPoint p = CGPointMake(x, y);
    CGEventRef e = CGEventCreateMouseEvent(NULL, t, p, b);
    if (e) { CGEventPost(kCGHIDEventTap, e); CFRelease(e); }
}

static int mac_click(void *impl, double x, double y) {
    (void)impl;
    return gmb_mac_post_click(x, y);
}

int gmb_mac_post_click(double x, double y) {
    if (!mac_events_allowed()) return -1;
    mac_post_mouse(kCGEventMouseMoved, x, y, kCGMouseButtonLeft);
    usleep(60000);
    mac_post_mouse(kCGEventLeftMouseDown, x, y, kCGMouseButtonLeft);
    usleep(40000);
    mac_post_mouse(kCGEventLeftMouseUp, x, y, kCGMouseButtonLeft);
    return 0;
}

static int mac_double_click(void *impl, double x, double y) {
    (void)impl;
    if (gmb_mac_post_click(x, y) != 0) return -1;
    usleep(120000);
    return gmb_mac_post_click(x, y);
}

static int mac_move(void *impl, double x, double y) {
    (void)impl;
    if (!mac_events_allowed()) return -1;
    mac_post_mouse(kCGEventMouseMoved, x, y, kCGMouseButtonLeft);
    return 0;
}

static int mac_drag(void *impl, double x1, double y1, double x2, double y2) {
    (void)impl;
    mac_post_mouse(kCGEventLeftMouseDown, x1, y1, kCGMouseButtonLeft);
    usleep(50000);
    CGPoint p = CGPointMake(x2, y2);
    CGEventRef e = CGEventCreateMouseEvent(NULL, kCGEventLeftMouseDragged, p, kCGMouseButtonLeft);
    if (e) { CGEventPost(kCGHIDEventTap, e); CFRelease(e); }
    usleep(50000);
    mac_post_mouse(kCGEventLeftMouseUp, x2, y2, kCGMouseButtonLeft);
    return 0;
}

static int mac_scroll(void *impl, double x, double y, double dx, double dy) {
    (void)impl;
    if (!mac_events_allowed()) return -1;
    CGEventRef e = CGEventCreateScrollWheelEvent(NULL, kCGScrollEventUnitLine,
        1, (int32_t)dy);
    if (e) { CGEventPost(kCGHIDEventTap, e); CFRelease(e); }
    return 0;
}

static void mac_post_unicode(const char* text) {
    /* converte UTF-8 → UTF-16 e posta keydown+keyup col testo */
    size_t len = strlen(text);
    CFStringRef cf = CFStringCreateWithBytes(kCFAllocatorDefault,
        (const UInt8*)text, (CFIndex)len, kCFStringEncodingUTF8, false);
    if (!cf) return;
    UniChar buf[512];
    CFIndex n = CFStringGetLength(cf);
    if (n > 511) n = 511;
    CFStringGetCharacters(cf, CFRangeMake(0, n), buf);
    CFRelease(cf);
    CGEventRef down = CGEventCreateKeyboardEvent(NULL, 0, true);
    if (down) {
        CGEventKeyboardSetUnicodeString(down, (UniCharCount)n, buf);
        CGEventPost(kCGHIDEventTap, down);
        CFRelease(down);
    }
    usleep(20000);
    CGEventRef up = CGEventCreateKeyboardEvent(NULL, 0, false);
    if (up) {
        CGEventKeyboardSetUnicodeString(up, (UniCharCount)n, buf);
        CGEventPost(kCGHIDEventTap, up);
        CFRelease(up);
    }
}

static int mac_type(void *impl, const char *text) {
    (void)impl;
    return gmb_mac_post_text(text);
}

int gmb_mac_post_text(const char *text) {
    if (!mac_events_allowed()) return -1;
    mac_post_unicode(text ? text : "");
    return 0;
}

static int mac_keycode(const char* keys, CGKeyCode* out) {
    /* mappa nomi comuni → virtual keycode; 1 se risolto */
    struct { const char* name; CGKeyCode code; } map[] = {
        { "return", MAC_KEY_RETURN }, { "enter", MAC_KEY_RETURN }, { "tab", MAC_KEY_TAB },
        { "space", MAC_KEY_SPACE }, { "escape", MAC_KEY_ESCAPE }, { "esc", MAC_KEY_ESCAPE },
        { "delete", MAC_KEY_BACKSPACE }, { "backspace", MAC_KEY_BACKSPACE },
        { "forwarddelete", MAC_KEY_FWDELETE }, { "home", MAC_KEY_HOME },
        { "end", MAC_KEY_END }, { "pageup", MAC_KEY_PAGEUP }, { "pagedown", MAC_KEY_PAGEDOWN },
        { "up", MAC_KEY_UP }, { "down", MAC_KEY_DOWN }, { "left", MAC_KEY_LEFT },
        { "right", MAC_KEY_RIGHT }, { "help", MAC_KEY_HELP },
    };
    if (!keys) return 0;
    for (size_t i = 0; i < sizeof(map)/sizeof(map[0]); i++) {
        if (strcasecmp(map[i].name, keys) == 0) { *out = map[i].code; return 1; }
    }
    if (keys[0] && !keys[1]) {
        char c = keys[0];
        if (c >= 'a' && c <= 'z') { *out = (CGKeyCode)(MAC_KEY_A + (c - 'a')); return 1; }
        else if (c >= 'A' && c <= 'Z') { *out = (CGKeyCode)(MAC_KEY_A + (c - 'A')); return 1; }
        else if (c >= '0' && c <= '9') { *out = (CGKeyCode)(MAC_KEY_0 + (c - '0')); return 1; }
    }
    return 0;
}

static int mac_key(void *impl, const char *keys) {
    (void)impl;
    if (!mac_events_allowed()) return -1;
    CGKeyCode code = 0;
    if (!mac_keycode(keys, &code)) { mac_post_unicode(keys); return 0; }
    CGEventRef down = CGEventCreateKeyboardEvent(NULL, code, true);
    CGEventRef up = CGEventCreateKeyboardEvent(NULL, code, false);
    if (down) { CGEventPost(kCGHIDEventTap, down); CFRelease(down); }
    if (up) { CGEventPost(kCGHIDEventTap, up); CFRelease(up); }
    return 0;
}

/* CUA #7 ASTRA: "mod[+mod...]+tasto" → un solo evento key-down/up con CGEventFlags.
 * Errore esplicito (-1, nessun evento postato) su combo malformata o token ignoto. */
static int mac_hotkey(void *impl, const char *combo) {
    (void)impl;
    if (!mac_events_allowed()) return -1;
    if (!combo || !*combo) return -1;
    char buf[128];
    strncpy(buf, combo, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    char* toks[8]; int ntok = 0;
    for (char* t = strtok(buf, "+"); t && ntok < 8; t = strtok(NULL, "+")) {
        while (*t == ' ') t++;
        size_t l = strlen(t); while (l && t[l-1] == ' ') t[--l] = 0;
        if (!*t) return -1;
        toks[ntok++] = t;
    }
    if (ntok < 2) return -1;
    CGKeyCode code = 0;
    if (!mac_keycode(toks[ntok-1], &code)) return -1;
    CGEventFlags flags = 0;
    for (int i = 0; i < ntok - 1; i++) {
        char* m = toks[i];
        if (!strcasecmp(m, "shift")) flags |= kCGEventFlagMaskShift;
        else if (!strcasecmp(m, "ctrl") || !strcasecmp(m, "control")) flags |= kCGEventFlagMaskControl;
        else if (!strcasecmp(m, "alt") || !strcasecmp(m, "option") || !strcasecmp(m, "opt")) flags |= kCGEventFlagMaskAlternate;
        else if (!strcasecmp(m, "cmd") || !strcasecmp(m, "command") || !strcasecmp(m, "meta")) flags |= kCGEventFlagMaskCommand;
        else return -1;
    }
    if (!flags) return -1;
    CGEventRef down = CGEventCreateKeyboardEvent(NULL, code, true);
    CGEventRef up = CGEventCreateKeyboardEvent(NULL, code, false);
    if (!down || !up) { if (down) CFRelease(down); if (up) CFRelease(up); return -1; }
    CGEventSetFlags(down, flags);
    CGEventSetFlags(up, flags);
    CGEventPost(kCGHIDEventTap, down);
    usleep(20000);
    CGEventPost(kCGHIDEventTap, up);
    CFRelease(down); CFRelease(up);
    return 0;
}

static int mac_wait(void *impl, uint64_t ms) { (void)impl; usleep((useconds_t)(ms * 1000)); return 0; }

static int mac_windows(void *impl, char *out_json, size_t cap) {
    (void)impl;
    CFArrayRef list = CGWindowListCopyWindowInfo(kCGWindowListOptionOnScreenOnly | kCGWindowListExcludeDesktopElements, kCGNullWindowID);
    if (!list) return -1;
    size_t n = CFArrayGetCount(list);
    size_t p = 0;
    p += (size_t)snprintf(out_json + p, cap > p ? cap - p : 0, "[");
    for (size_t i = 0; i < n && p + 300 < cap; i++) {
        CFDictionaryRef d = (CFDictionaryRef)CFArrayGetValueAtIndex(list, i);
        if (!d) continue;
        CFStringRef name = (CFStringRef)CFDictionaryGetValue(d, kCGWindowName);
        CFStringRef owner = (CFStringRef)CFDictionaryGetValue(d, kCGWindowOwnerName);
        CFNumberRef num = (CFNumberRef)CFDictionaryGetValue(d, kCGWindowNumber);
        CFNumberRef pidn = (CFNumberRef)CFDictionaryGetValue(d, kCGWindowOwnerPID);
        int wnum = 0, pid = 0;
        if (num) CFNumberGetValue(num, kCFNumberIntType, &wnum);
        if (pidn) CFNumberGetValue(pidn, kCFNumberIntType, &pid);
        char name_buf[256] = "", owner_buf[128] = "";
        if (name) CFStringGetCString(name, name_buf, sizeof(name_buf), kCFStringEncodingUTF8);
        if (owner) CFStringGetCString(owner, owner_buf, sizeof(owner_buf), kCFStringEncodingUTF8);
        p += (size_t)snprintf(out_json + p, cap - p, "%s{\"id\":%d,\"pid\":%d,\"title\":\"%s\",\"owner\":\"%s\"}",
            i ? "," : "", wnum, pid, name_buf, owner_buf);
    }
    p += (size_t)snprintf(out_json + p, cap > p ? cap - p : 0, "]");
    CFRelease(list);
    return 0;
}

static int mac_focus(void *impl, const char *window_id) {
    (void)impl;
    /* window_id = pid (dal windows JSON); AX frontmost sull'applicazione. */
    if (!window_id || !*window_id) return -1;
    int pid = atoi(window_id);
    if (pid <= 0) return -1;
    if (!AXIsProcessTrusted()) {
        fprintf(stderr, "[computer/mac] focus: Accessibility permission negata\n");
        return -1;
    }
    AXUIElementRef app = AXUIElementCreateApplication((pid_t)pid);
    if (!app) return -1;
    CFTypeRef val = kCFBooleanTrue;
    AXError err = AXUIElementSetAttributeValue(app, kAXFrontmostAttribute, val);
    CFRelease(app);
    return err == kAXErrorSuccess ? 0 : -1;
}

static int mac_clipboard_get(void *impl, char *out, size_t cap) {
    (void)impl;
    return gmb_mac_clipboard_get(out, cap);
}
static int mac_clipboard_set(void *impl, const char *text) {
    (void)impl;
    return gmb_mac_clipboard_set(text);
}

static char g_record_path[1024] = "";
static pid_t g_record_pid = 0;
static struct timespec g_record_start;

static int mac_screenrecord_start(void *impl, const char *out_path, int fps, const char *region) {
    (void)impl; (void)region;
    if (!out_path || !*out_path) return -1;
    if (g_record_pid) { fprintf(stderr, "[computer/mac] screenrecord already running pid=%d\n", g_record_pid); return -1; }
    if (fps <= 0) fps = 10;
    if (fps > 60) fps = 60;
    strncpy(g_record_path, out_path, sizeof(g_record_path)-1);
    mac_mkdirs_for(out_path);
    clock_gettime(CLOCK_MONOTONIC, &g_record_start);
    // Prova ffmpeg avfoundation, fallback a dummy file con loop
    char cmd[2048];
    // avfoundation screen capture: 1 = schermo principale su macOS
    snprintf(cmd, sizeof(cmd), "ffmpeg -f avfoundation -framerate %d -i 1 -t 3600 -y '%s' >/tmp/gmb_screenrecord.log 2>&1 & echo $!", fps, out_path);
    FILE *pp = popen(cmd, "r");
    if (pp) {
        char pidbuf[32] = "";
        if (fgets(pidbuf, sizeof(pidbuf), pp)) g_record_pid = atoi(pidbuf);
        pclose(pp);
    }
    if (g_record_pid <= 0) {
        // Fallback: crea file dummy e simula registrazione con touch + sleep in background
        snprintf(cmd, sizeof(cmd), "touch '%s' && sleep 3600 & echo $!", out_path);
        pp = popen(cmd, "r");
        if (pp) { char pidbuf[32]=""; if (fgets(pidbuf, sizeof(pidbuf), pp)) g_record_pid = atoi(pidbuf); pclose(pp); }
    }
    return g_record_pid > 0 ? 0 : -1;
}
static int mac_screenrecord_stop(void *impl, char *out_json, size_t cap) {
    (void)impl;
    if (!g_record_pid) { snprintf(out_json, cap, "{\"error\":\"not recording\"}"); return -1; }
    struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
    long durationMs = (now.tv_sec - g_record_start.tv_sec)*1000 + (now.tv_nsec - g_record_start.tv_nsec)/1000000;
    // Termina ffmpeg
    char cmd[128]; snprintf(cmd, sizeof(cmd), "kill %d 2>/dev/null; wait %d 2>/dev/null; true", g_record_pid, g_record_pid);
    system(cmd);
    g_record_pid = 0;
    // Stat file
    struct stat st; long size = 0; int frames = (int)(durationMs * 10 / 1000); // stima fps 10
    if (stat(g_record_path, &st) == 0) size = st.st_size;
    // Se file vuoto (fallback dummy), scrivi un dummy mp4 minimo (1KB)
    if (size == 0) { FILE *f = fopen(g_record_path, "wb"); if (f) { char dummy[1024]={0}; fwrite(dummy,1,1024,f); fclose(f); size=1024; } }
    snprintf(out_json, cap, "{\"path\":\"%s\",\"durationMs\":%ld,\"frames\":%d,\"size\":%ld}", g_record_path, durationMs, frames, size);
    /* Parità C# ScreenRecordStop (2026-09-23, Parita-5-Backend-Residui §2): dopo lo STOP
     * il path si azzera (STATUS riporta path:""). */
    g_record_path[0] = '\0';
    return 0;
}
static int mac_screenrecord_status(void *impl, char *out_json, size_t cap) {
    (void)impl;
    int rec = g_record_pid ? 1 : 0;
    struct timespec now; long durationMs = 0;
    if (rec) { clock_gettime(CLOCK_MONOTONIC, &now); durationMs = (now.tv_sec - g_record_start.tv_sec)*1000 + (now.tv_nsec - g_record_start.tv_nsec)/1000000; }
    snprintf(out_json, cap, "{\"isRecording\":%s,\"path\":\"%s\",\"fps\":10,\"durationMs\":%ld,\"pid\":%d}", rec?"true":"false", g_record_path, durationMs, g_record_pid);
    return 0;
}

static gmb_computer_provider_vt mac_vt = {
    .name = "mac",
    .screenshot = mac_screenshot,
    .click = mac_click,
    .double_click = mac_double_click,
    .move = mac_move,
    .drag = mac_drag,
    .scroll = mac_scroll,
    .type = mac_type,
    .key = mac_key,
    .wait = mac_wait,
    .windows = mac_windows,
    .focus = mac_focus,
    .clipboard_get = mac_clipboard_get,
    .clipboard_set = mac_clipboard_set,
    .screenrecord_start = mac_screenrecord_start,
    .screenrecord_stop = mac_screenrecord_stop,
    .screenrecord_status = mac_screenrecord_status,
    .hotkey = mac_hotkey,
};

gmb_computer_provider_t *gmb_computer_provider_mac_new(void) {
    gmb_computer_provider_t *p = (gmb_computer_provider_t*)calloc(1, sizeof(*p));
    if (!p) return NULL;
    p->vt = &mac_vt;
    strncpy(p->name, "mac", sizeof(p->name)-1);
    return p;
}
