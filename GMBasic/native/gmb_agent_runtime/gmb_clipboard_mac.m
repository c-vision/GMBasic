// gmb_clipboard_mac.m — clipboard macOS reale via NSPasteboard (AppKit).
// Compilato solo su APPLE come Objective-C. Esporta API C usate dal ComputerProvider.
#import <Cocoa/Cocoa.h>
#include <string.h>

int gmb_mac_clipboard_get(char *out, size_t cap) {
    if (!out || cap == 0) return -1;
    out[0] = 0;
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    NSString *s = [pb stringForType:NSPasteboardTypeString];
    if (!s) return -1;
    const char *utf8 = [s UTF8String];
    if (!utf8) return -1;
    size_t n = strlen(utf8);
    if (n >= cap) n = cap - 1;
    memcpy(out, utf8, n);
    out[n] = 0;
    return 0;
}

int gmb_mac_clipboard_set(const char *text) {
    if (!text) return -1;
    NSPasteboard *pb = [NSPasteboard generalPasteboard];
    [pb clearContents];
    NSString *s = [[NSString alloc] initWithUTF8String:text];
    if (!s) return -1;
    [pb setString:s forType:NSPasteboardTypeString];
    return 0;
}
