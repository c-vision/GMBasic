/* gmb_tray_macos.m — icona nella menu bar macOS via NSStatusItem (AppKit, MRC).
 * Main thread. poll() pompa gli eventi pendenti (per daemon headless: nessun run loop proprio). */
#import <Cocoa/Cocoa.h>
#include "gmb_tray.h"

/* Icona di default GMBasic (PNG incorporato, gmb_tray_icon.c). */
extern const unsigned char gmb_tray_default_icon[];
extern const unsigned int gmb_tray_default_icon_len;

static NSStatusItem *g_item = nil;
static NSMenu *g_menu = nil;
static gmb_tray_cb g_cb = NULL;
static void *g_user = NULL;

@interface GmbTrayTarget : NSObject
- (void)menuItemClicked:(id)sender;
- (void)toggleMenu:(id)sender;
@end

@implementation GmbTrayTarget
- (void)menuItemClicked:(id)sender
{
    NSMenuItem *it = (NSMenuItem *)sender;
    if (g_cb) g_cb((int)[it tag], g_user);
}
/* Click sull'icona: apre il menu in modo SINCRONO (tracking proprio), affidabile
 * anche senza NSApp.run — il pump dell'host resta valido. */
- (void)toggleMenu:(id)sender
{
    (void)sender;
    if (g_item && g_menu) [g_item popUpStatusItemMenu:g_menu];
}
@end

static GmbTrayTarget *g_target = nil;

int gmb_tray_macos_available(void) { return 1; }
int gmb_tray_macos_is_main(void) { return [NSThread isMainThread] ? 1 : 0; }
const char *gmb_tray_macos_backend(void) { return "macos"; }

int gmb_tray_macos_create(const char *icon_path, const char *tooltip)
{
    if (![NSThread isMainThread]) return -2; /* AppKit richiede il main thread */
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyAccessory]; /* niente Dock */
        if (!g_target) g_target = [[GmbTrayTarget alloc] init];
        if (g_item) return 0;
        g_item = [[NSStatusBar systemStatusBar] statusItemWithLength:NSVariableStatusItemLength];
        [g_item retain];
        NSStatusBarButton *btn = g_item.button;
        if (icon_path && icon_path[0]) {
            NSString *p = [NSString stringWithUTF8String:icon_path];
            NSImage *img = [[NSImage alloc] initWithContentsOfFile:p];
            if (img) { img.template = YES; btn.image = img; }
            else btn.title = @"GMB";
        } else {
            /* Default: marchio GMBasic incorporato (template). */
            NSData *d = [NSData dataWithBytes:gmb_tray_default_icon length:gmb_tray_default_icon_len];
            NSImage *img = [[NSImage alloc] initWithData:d];
            if (img) {
                img.template = YES;
                img.size = NSMakeSize(18.0, 18.0);
                btn.image = img;
            } else {
                btn.title = @"GMB";
            }
        }
        if (tooltip && tooltip[0]) btn.toolTip = [NSString stringWithUTF8String:tooltip];
        g_menu = [[NSMenu alloc] init];
        /* Target/action esplicito (niente setMenu): il click apre il menu con tracking sincrono. */
        btn.target = g_target;
        btn.action = @selector(toggleMenu:);
        return 0;
    }
}

int gmb_tray_macos_item(int id, const char *label)
{
    if (![NSThread isMainThread]) return -2;
    @autoreleasepool {
        if (!g_item || !g_menu) return -1;
        NSString *l = [NSString stringWithUTF8String:(label ? label : "")];
        NSMenuItem *it = [[NSMenuItem alloc] initWithTitle:l
                                                    action:@selector(menuItemClicked:)
                                             keyEquivalent:@""];
        [it setTarget:g_target];
        [it setTag:id];
        [g_menu addItem:it];
        return 0;
    }
}

int gmb_tray_macos_sep(void)
{
    if (![NSThread isMainThread]) return -2;
    if (!g_menu) return -1;
    [g_menu addItem:[NSMenuItem separatorItem]];
    return 0;
}

int gmb_tray_macos_tooltip(const char *text)
{
    if (![NSThread isMainThread]) return -2;
    if (!g_item) return -1;
    g_item.button.toolTip = (text && text[0]) ? [NSString stringWithUTF8String:text] : nil;
    return 0;
}

void gmb_tray_macos_callback(gmb_tray_cb cb, void *user) { g_cb = cb; g_user = user; }

/* Pompa gli eventi pendenti senza possedere il run loop (integrazione con l'host/SDL). */
int gmb_tray_macos_poll(void)
{
    if (![NSThread isMainThread]) return -2;
    @autoreleasepool {
        int n = 0;
        NSEvent *e;
        while ((e = [NSApp nextEventMatchingMask:NSEventMaskAny
                                       untilDate:[NSDate distantPast]
                                          inMode:NSDefaultRunLoopMode
                                         dequeue:YES]) != nil) {
            [NSApp sendEvent:e];
            n++;
        }
        return n;
    }
}

void gmb_tray_macos_remove(void)
{
    if (![NSThread isMainThread]) return;
    if (g_item) {
        [[NSStatusBar systemStatusBar] removeStatusItem:g_item];
        [g_item release];
        g_item = nil;
        g_menu = nil;
    }
}

int gmb_tray_macos_selftest(void) { return g_item ? 1 : 0; }
