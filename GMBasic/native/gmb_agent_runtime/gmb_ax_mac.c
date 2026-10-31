/* gmb_ax_mac.c — Accessibility macOS reale per UI.* semantico.
 * Cerca nell'albero AX dell'app frontmost un elemento il cui titolo/descrizione/valore
 * contiene il target, poi PRESS (AXUIElementPerformAction) con fallback click al centro
 * (CGEvent via gmb_mac_post_click). TYPE = focus + gmb_mac_post_text. TREE = dump JSON
 * ricorsivo dell'albero (per osservazione). Richiede Accessibilità (errore esplicito). */
#include "gmb_ax.h"
#include "gmb_computer_provider.h"
#include <ApplicationServices/ApplicationServices.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AX_MAX_BUDGET 4000
#define AX_MAX_DEPTH  40

typedef struct { char* b; size_t cap; size_t p; } ax_buf_t;

static void axb_put(ax_buf_t* B, const char* s) {
    while (*s && B->p + 1 < B->cap) B->b[B->p++] = *s++;
    B->b[B->p] = 0;
}

static CFStringRef u8_to_cf(const char* s) {
    return CFStringCreateWithBytes(NULL, (const UInt8*)(s ? s : ""),
        (CFIndex)(s ? strlen(s) : 0), kCFStringEncodingUTF8, false);
}

static void axb_json_str(ax_buf_t* B, const char* label, CFStringRef s) {
    axb_put(B, "\"");
    axb_put(B, label);
    axb_put(B, "\":\"");
    if (s) {
        CFIndex n = CFStringGetLength(s);
        UniChar buf[256];
        CFIndex pos = 0;
        while (pos < n && B->p + 8 < B->cap) {
            CFIndex take = (n - pos < 128) ? (n - pos) : 128;
            CFStringGetCharacters(s, CFRangeMake(pos, take), buf);
            for (CFIndex i = 0; i < take && B->p + 8 < B->cap; i++) {
                UniChar c = buf[i];
                if (c == '"') { axb_put(B, "\\\""); }
                else if (c == '\\') { axb_put(B, "\\\\"); }
                else if (c == '\n') { axb_put(B, "\\n"); }
                else if (c == '\r') { axb_put(B, "\\r"); }
                else if (c == '\t') { axb_put(B, "\\t"); }
                else if (c < 0x20) { char tmp[8]; snprintf(tmp, sizeof(tmp), "\\u%04x", (unsigned)c); axb_put(B, tmp); }
                else if (c < 0x80) { char t = (char)c; axb_put(B, &t); }
                else {
                    if (c >= 0xD800 && c <= 0xDBFF && i + 1 < take) {
                        UniChar lo = buf[i + 1];
                        if (lo >= 0xDC00 && lo <= 0xDFFF) {
                            uint32_t cp = 0x10000 + (((uint32_t)(c - 0xD800)) << 10) + (lo - 0xDC00);
                            char t[4] = { (char)(0xF0 | (cp >> 18)), (char)(0x80 | ((cp >> 12) & 0x3F)),
                                          (char)(0x80 | ((cp >> 6) & 0x3F)), (char)(0x80 | (cp & 0x3F)) };
                            axb_put(B, t); axb_put(B, t + 1); axb_put(B, t + 2); axb_put(B, t + 3);
                            i++;
                            continue;
                        }
                    }
                    char t[2] = { (char)(0xC0 | (c >> 6)), (char)(0x80 | (c & 0x3F)) };
                    axb_put(B, t); axb_put(B, t + 1);
                }
            }
            pos += take;
        }
    }
    axb_put(B, "\"");
}

static int attr_str(AXUIElementRef el, CFStringRef attr, CFStringRef* out) {
    CFTypeRef v = NULL;
    if (AXUIElementCopyAttributeValue(el, attr, &v) != kAXErrorSuccess) return -1;
    if (!v || CFGetTypeID(v) != CFStringGetTypeID()) { if (v) CFRelease(v); return -1; }
    *out = (CFStringRef)v;
    return 0;
}

static void attr_str_copy_c(AXUIElementRef el, CFStringRef attr, char* out, size_t cap) {
    CFStringRef s = NULL;
    out[0] = 0;
    if (attr_str(el, attr, &s) == 0) {
        CFStringGetCString(s, out, (CFIndex)cap, kCFStringEncodingUTF8);
        CFRelease(s);
    }
}

static int attr_has(AXUIElementRef el, CFStringRef attr, CFStringRef needle) {
    CFStringRef s = NULL;
    if (attr_str(el, attr, &s) != 0) return 0;
    int has = (CFStringGetLength(needle) > 0) &&
        (CFStringFind(s, needle, kCFCompareCaseInsensitive).location != kCFNotFound);
    CFRelease(s);
    return has;
}

static AXUIElementRef ax_front_app(void) {
    AXUIElementRef sys = AXUIElementCreateSystemWide();
    CFTypeRef v = NULL;
    AXError e = AXUIElementCopyAttributeValue(sys, kAXFocusedApplicationAttribute, &v);
    CFRelease(sys);
    if (e != kAXErrorSuccess || !v) return NULL;
    return (AXUIElementRef)v;
}

static void role_of(AXUIElementRef el, char* buf, size_t cap) {
    attr_str_copy_c(el, kAXRoleAttribute, buf, cap);
    if (!buf[0]) snprintf(buf, cap, "%s", "?");
}

static int is_text_role(const char* role) {
    return strstr(role, "TextArea") || strstr(role, "TextField") || strstr(role, "ComboBox") ||
           strstr(role, "SearchField") || strstr(role, "SecureTextField");
}

/* ---- ricerca primo match ---- */
typedef struct { AXUIElementRef el; char role[40]; char title[512]; } ax_match_t;

static int ax_find_one(AXUIElementRef node, CFStringRef needle, int want_field, ax_match_t* m, int* budget, int depth) {
    if (depth > AX_MAX_DEPTH) return 0;
    char role[40] = "?";
    role_of(node, role, sizeof(role));
    int hit = 0;
    if (want_field ? is_text_role(role) : 1) {
        hit = attr_has(node, kAXTitleAttribute, needle) ||
              attr_has(node, kAXDescriptionAttribute, needle) ||
              attr_has(node, want_field ? kAXPlaceholderValueAttribute : kAXValueAttribute, needle);
    }
    if (hit) {
        m->el = CFRetain(node);
        snprintf(m->role, sizeof(m->role), "%s", role);
        attr_str_copy_c(node, kAXTitleAttribute, m->title, sizeof(m->title));
        return 1;
    }
    /* i menu dell'app vivono sotto kAXMenuBarAttribute: provali PRIMA dei children
       (i window-tree consumano il budget; per UI.CLICK i menu sono i target più comuni) */
    if (*budget > 0) {
        CFTypeRef mb = NULL;
        if (AXUIElementCopyAttributeValue(node, kAXMenuBarAttribute, &mb) == kAXErrorSuccess && mb) {
            if (CFGetTypeID(mb) == AXUIElementGetTypeID()) {
                (*budget)--;
                if (ax_find_one((AXUIElementRef)mb, needle, want_field, m, budget, depth + 1)) {
                    CFRelease(mb);
                    return 1;
                }
            }
            CFRelease(mb);
        }
    }
    CFTypeRef chv = NULL;
    int found = 0;
    if (AXUIElementCopyAttributeValue(node, kAXChildrenAttribute, &chv) == kAXErrorSuccess && chv) {
        if (CFGetTypeID(chv) == CFArrayGetTypeID()) {
            CFArrayRef arr = (CFArrayRef)chv;
            CFIndex n = CFArrayGetCount(arr);
            for (CFIndex i = 0; i < n && !found; i++) {
                if (*budget <= 0) break;
                (*budget)--;
                AXUIElementRef child = (AXUIElementRef)CFArrayGetValueAtIndex(arr, i);
                if (child && CFGetTypeID(child) == AXUIElementGetTypeID())
                    found = ax_find_one(child, needle, want_field, m, budget, depth + 1);
            }
        }
        CFRelease(chv);
    }
    return found;
}

static int ax_press_or_point_click(AXUIElementRef el, char* err, size_t errcap) {
    if (err) err[0] = 0;
    if (!el) return -1;
    if (AXIsProcessTrusted()) {
        if (AXUIElementPerformAction(el, kAXPressAction) == kAXErrorSuccess) return 0;
    }
    /* fallback: click al centro (CGEvent) */
    CFTypeRef pv = NULL, sv = NULL;
    double cx = -1, cy = -1, cw = 0, ch = 0;
    if (AXUIElementCopyAttributeValue(el, kAXPositionAttribute, &pv) == kAXErrorSuccess && pv &&
        CFGetTypeID(pv) == AXValueGetTypeID()) {
        CGPoint pt;
        if (AXValueGetValue(pv, kAXValueCGPointType, &pt)) { cx = pt.x; cy = pt.y; }
        CFRelease(pv);
    }
    if (AXUIElementCopyAttributeValue(el, kAXSizeAttribute, &sv) == kAXErrorSuccess && sv &&
        CFGetTypeID(sv) == AXValueGetTypeID()) {
        CGSize sz;
        if (AXValueGetValue(sv, kAXValueCGSizeType, &sz)) { cw = sz.width; ch = sz.height; }
        CFRelease(sv);
    }
    if (cx >= 0 && cy >= 0) {
        if (gmb_mac_post_click(cx + cw / 2.0, cy + ch / 2.0) == 0) return 0;
        if (err) snprintf(err, errcap, "Accessibility permission negata");
        return -1;
    }
    if (err) snprintf(err, errcap, "elemento senza posizione");
    return -1;
}

int gmb_ax_available(void) { return AXIsProcessTrusted() ? 1 : 0; }

char *gmb_ax_find_json(const char *target) {
    CFStringRef needle = u8_to_cf(target);
    AXUIElementRef app = ax_front_app();
    if (!needle || !app) { if (needle) CFRelease(needle); if (app) CFRelease(app); return strdup("{\"found\":false}"); }
    ax_match_t m; memset(&m, 0, sizeof(m));
    int budget = AX_MAX_BUDGET;
    char* out;
    if (ax_find_one(app, needle, 0, &m, &budget, 0)) {
        char buf[1200];
        snprintf(buf, sizeof(buf), "{\"found\":true,\"target\":\"%s\",\"role\":\"%s\",\"title\":\"%s\",\"level\":\"AX\"}",
            target, m.role, m.title);
        out = strdup(buf);
        CFRelease(m.el);
    } else out = strdup("{\"found\":false}");
    CFRelease(app);
    CFRelease(needle);
    return out;
}

char *gmb_ax_findall_json(const char *target) {
    /* TODO multi-match completo: per ora il primo match è sufficiente per click/type */
    return gmb_ax_find_json(target);
}

char *gmb_ax_click_json(const char *target) {
    CFStringRef needle = u8_to_cf(target);
    AXUIElementRef app = ax_front_app();
    if (!needle || !app) { if (needle) CFRelease(needle); if (app) CFRelease(app); return strdup("{\"ok\":false}"); }
    ax_match_t m; memset(&m, 0, sizeof(m));
    int budget = AX_MAX_BUDGET;
    char* out;
    if (ax_find_one(app, needle, 0, &m, &budget, 0)) {
        char err[256] = "";
        int rc = ax_press_or_point_click(m.el, err, sizeof(err));
        char buf[1000];
        if (rc == 0)
            snprintf(buf, sizeof(buf), "{\"ok\":true,\"role\":\"%s\",\"title\":\"%s\",\"method\":\"ax\"}", m.role, m.title);
        else
            snprintf(buf, sizeof(buf), "{\"ok\":false,\"error\":\"%s\"}", err[0] ? err : "press fallita");
        out = strdup(buf);
        CFRelease(m.el);
    } else out = strdup("{\"ok\":false,\"error\":\"elemento non trovato\"}");
    CFRelease(app);
    CFRelease(needle);
    return out;
}

char *gmb_ax_type_json(const char *target, const char *text) {
    CFStringRef needle = u8_to_cf(target);
    AXUIElementRef app = ax_front_app();
    if (!needle || !app) { if (needle) CFRelease(needle); if (app) CFRelease(app); return strdup("{\"ok\":false}"); }
    ax_match_t m; memset(&m, 0, sizeof(m));
    int budget = AX_MAX_BUDGET;
    char* out;
    if (!ax_find_one(app, needle, 1, &m, &budget, 0)) {
        budget = AX_MAX_BUDGET;
        memset(&m, 0, sizeof(m));
        ax_find_one(app, needle, 0, &m, &budget, 0);
    }
    if (m.el) {
        if (AXIsProcessTrusted())
            AXUIElementSetAttributeValue(m.el, kAXFocusedAttribute, kCFBooleanTrue);
        if (gmb_mac_post_text(text ? text : "") == 0) {
            char buf[1000];
            snprintf(buf, sizeof(buf), "{\"ok\":true,\"role\":\"%s\",\"title\":\"%s\",\"method\":\"focus+type\"}", m.role, m.title);
            out = strdup(buf);
        } else out = strdup("{\"ok\":false,\"error\":\"Accessibility negata (type)\"}");
        CFRelease(m.el);
    } else out = strdup("{\"ok\":false,\"error\":\"campo non trovato\"}");
    CFRelease(app);
    CFRelease(needle);
    return out;
}

/* ---- dump ricorsivo albero AX (osservazione) ---- */
static void ax_tree_walk(AXUIElementRef el, ax_buf_t* B, int* budget, int depth) {
    if (depth > AX_MAX_DEPTH || *budget <= 0) return;
    (*budget)--;
    CFStringRef role = NULL, title = NULL, desc = NULL;
    attr_str(el, kAXRoleAttribute, &role);
    attr_str(el, kAXTitleAttribute, &title);
    attr_str(el, kAXDescriptionAttribute, &desc);
    axb_put(B, "{\"role\":");
    axb_json_str(B, "", role);
    axb_put(B, ",\"title\":");
    axb_json_str(B, "", title);
    axb_put(B, ",\"desc\":");
    axb_json_str(B, "", desc);
    if (role) CFRelease(role);
    if (title) CFRelease(title);
    if (desc) CFRelease(desc);

    CFTypeRef chv = NULL;
    if (AXUIElementCopyAttributeValue(el, kAXChildrenAttribute, &chv) == kAXErrorSuccess && chv) {
        if (CFGetTypeID(chv) == CFArrayGetTypeID()) {
            CFArrayRef arr = (CFArrayRef)chv;
            CFIndex n = CFArrayGetCount(arr);
            if (n > 0 && *budget > 0) {
                axb_put(B, ",\"children\":[");
                int first = 1;
                for (CFIndex i = 0; i < n && *budget > 0; i++) {
                    AXUIElementRef child = (AXUIElementRef)CFArrayGetValueAtIndex(arr, i);
                    if (!child || CFGetTypeID(child) != AXUIElementGetTypeID()) continue;
                    axb_put(B, first ? "" : ",");
                    first = 0;
                    ax_tree_walk(child, B, budget, depth + 1);
                }
                axb_put(B, "]");
            }
        }
        CFRelease(chv);
    }
    axb_put(B, "}");
}

char *gmb_ax_tree_json(void) {
    AXUIElementRef app = ax_front_app();
    if (!app) return strdup("[]");
    ax_buf_t B; memset(&B, 0, sizeof(B));
    B.cap = 32 * 1024;
    B.b = (char*)malloc(B.cap);
    if (!B.b) { CFRelease(app); return strdup("[]"); }
    B.b[0] = 0;
    int budget = 1500;
    ax_tree_walk(app, &B, &budget, 0);
    CFRelease(app);
    return B.b;
}
