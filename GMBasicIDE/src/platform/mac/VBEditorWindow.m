#import "VBEditorWindow.h"
#import "VBTheme.h"
#import "VBToolboxView.h"
#import "VBProjectOutlineView.h"
#import "VBPropertyPanel.h"
#import "VBColorPaletteView.h"
#import "VBFormLayoutView.h"
#import "VBCodeEditor.h"
#import "VBGmbRunner.h"
#import "VBGmbSourceWindow.h"
#import "VBTerminalWindow.h"
#import "VBDesignerView.h"
#import "VBDesignerSelfTest.h"
#import "VBTabOrderWindow.h"
#import "VBRuntimeWindow.h"
#import "VBMenuBarView.h"
#import "VBToolbarView.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "core/vb_code.h"
#include "core/vb_parse.h"
#include "core/vb_util.h"
#include "core/vb_gmb.h"

#include <dispatch/dispatch.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static NSMutableArray<VBGmbSourceWindowController *> *gGmbSourceWindows;

static NSString *StringFromC(const char *text)
{
    return text ? [NSString stringWithUTF8String:text] : @"";
}

static const char *ExtensionForKind(VbItemKind kind)
{
    switch (kind) {
        case VB_ITEM_FORM: return "frm";
        case VB_ITEM_MODULE: return "bas";
        case VB_ITEM_CLASS: return "cls";
        case VB_ITEM_USERCONTROL: return "ctl";
        default: return "txt";
    }
}

static BOOL IsBooleanKey(NSString *key)
{
    static NSSet *keys;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        keys = [NSSet setWithObjects:@"Visible", @"Enabled", @"Locked", @"TabStop", @"MultiLine",
                @"Default", @"Cancel", @"AutoSize", @"WordWrap", @"TabStop", @"FillStyle",
                @"Appearance", @"BorderStyle", @"ColumnHeaders", @"AllowAddNew", @"AllowDelete",
                @"AllowUpdate", @"AllowRowSizing", @"AllowColSizing", @"ReadOnly", @"Exclusive", nil];
    });
    return [keys containsObject:key];
}

static BOOL IsIntegerKey(NSString *key)
{
    static NSSet *keys;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        keys = [NSSet setWithObjects:@"Left", @"Top", @"Width", @"Height", @"ClientWidth", @"ClientHeight",
                @"TabIndex", @"Interval", @"Min", @"Max", @"Alignment", @"BorderStyle", @"Value",
                @"X1", @"Y1", @"X2", @"Y2", @"Shape", @"FillStyle", @"ScrollBars", @"Font.Size",
                @"Font.Weight", @"Font.Charset", @"DefColWidth", @"RowHeight", @"RecordsetType",
                @"BOFAction", @"EOFAction", @"Options", nil];
    });
    return [keys containsObject:key];
}

static int CountControls(VbControl *control)
{
    int count = 0;
    if (!control) return 0;
    for (int i = 0; i < control->nchildren; i++) {
        count += 1 + CountControls(control->children[i]);
    }
    return count;
}

static void CollectControls(VbControl *control, NSMutableArray *out)
{
    if (!control) return;
    [out addObject:[NSValue valueWithPointer:control]];
    for (int i = 0; i < control->nchildren; i++) {
        CollectControls(control->children[i], out);
    }
}

#pragma mark - MDI child window

@interface VBMdiChildView : NSView
@property (nonatomic, copy) NSString *titleText;
@property (nonatomic, strong) NSView *content;
@property (nonatomic) BOOL active;
- (void)setTitleText:(NSString *)title content:(NSView *)content;
@end

@implementation VBMdiChildView

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (!self) return nil;
    self.wantsLayer = NO;
    return self;
}

- (BOOL)isFlipped { return YES; }

- (void)setTitleText:(NSString *)title content:(NSView *)content
{
    self.titleText = title;
    if (content) {
        if (_content && _content.superview == self) [_content removeFromSuperview];
        _content = content;
        [self addSubview:content];
    }
    [self layoutContent];
    [self setNeedsDisplay:YES];
}

- (void)setFrameSize:(NSSize)newSize
{
    [super setFrameSize:newSize];
    [self layoutContent];
}

- (void)layoutContent
{
    if (!_content) return;
    _content.frame = NSMakeRect(2, VB_TITLE_HEIGHT + 1,
                                MAX(0, NSWidth(self.bounds) - 4),
                                MAX(0, NSHeight(self.bounds) - VB_TITLE_HEIGHT - 3));
}

- (void)layout
{
    [super layout];
    [self layoutContent];
}

- (void)drawRect:(NSRect)dirtyRect
{
    (void)dirtyRect;
    VBFillFace(self.bounds);
    [[VBColorShadow() colorWithAlphaComponent:0.9] setFill];
    NSRectFill(NSMakeRect(0, 0, NSWidth(self.bounds), 1));
    NSRectFill(NSMakeRect(0, 0, 1, NSHeight(self.bounds)));
    [[VBColorLight() colorWithAlphaComponent:0.9] setFill];
    NSRectFill(NSMakeRect(NSWidth(self.bounds) - 1, 0, 1, NSHeight(self.bounds)));
    NSRectFill(NSMakeRect(0, NSHeight(self.bounds) - 1, NSWidth(self.bounds), 1));
    VBDrawTitleBar(NSMakeRect(1, 1, NSWidth(self.bounds) - 2, VB_TITLE_HEIGHT - 1),
                   self.titleText ?: @"", self.active, YES, NO, NO);

    // The client area frame of an MDI child is a sunken 3D border.
    [[VBColorShadow() colorWithAlphaComponent:0.8] setFill];
    NSRectFill(NSMakeRect(1, VB_TITLE_HEIGHT, 1, NSHeight(self.bounds) - VB_TITLE_HEIGHT - 1));
}

@end

#pragma mark - Editor

@interface VBStatusPanelView : NSView
@end

@implementation VBStatusPanelView
- (void)drawRect:(NSRect)dirtyRect
{
    (void)dirtyRect;
    VBDrawStatusPanel(self.bounds);
}
@end

@interface VBEditorWindowController () <VBToolboxViewDelegate, VBProjectOutlineViewDelegate,
                                        VBPropertyPanelDelegate, VBCodeEditorDelegate,
                                        VBDesignerViewDelegate, VBRuntimeWindowDelegate,
                                        VBToolbarViewDelegate>
@end

@implementation VBEditorWindowController {
    VBToolboxView *_toolbox;
    VBProjectOutlineView *_outline;
    VBPropertyPanel *_properties;
    NSWindow *_colorPaletteWindow;
    NSWindow *_formLayoutWindow;
    VBFormLayoutView *_formLayoutView;
    VBFormLayoutView *_formLayoutDock;
    NSWindow *_objectBrowserWindow;
    NSTextView *_objectBrowserText;
    BOOL _paletteBack;
    VBCodeEditor *_codeEditor;
    VBDesignerView *_designer;
    NSScrollView *_designerScroll;
    VBMdiChildView *_workspace;
    VBMdiChildView *_designerWindow;
    VBMdiChildView *_codeWindow;
    VBMenuBarView *_menuBar;
    VBToolbarView *_toolbar;
    VBToolbarView *_formatBar;
    NSScrollView *_toolScroll;
    NSSplitView *_split;
    NSSplitView *_rightSplit;
    NSView *_statusBar;
    NSTextField *_status;
    NSTextField *_statusMode;
    NSTextField *_statusPos;
    VBRuntimeWindow *_runtime;
    VBRuntimeWindow *_runtimeListTest;
    VBRuntimeWindow *_runtimeTabTest;
    VBRuntimeWindow *_runtimeAccessKeyTest;
    VBRuntimeWindow *_runtimeFocusTest;
    VBRuntimeWindow *_runtimeErrorTest;
    VBRuntimeWindow *_runtimeBreakTest;
    VBRuntimeWindow *_runtimeBreakpointTest;
    VBRuntimeWindow *_runtimeErrorSnapshot;
    BOOL _showCode;
    BOOL _projectDirty;
    BOOL _formatMenuStateValid;
    BOOL _formatCanAlign;
    BOOL _formatCanSpace;
    BOOL _formatCanOrder;
    BOOL _formatHasSelection;
    NSMutableDictionary<NSValue *, NSString *> *_moduleCodes;
    NSMutableSet<NSValue *> *_dirtyModules;
    NSMutableArray<VBTerminalWindow *> *_terminals;
}

- (instancetype)init
{
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 1200, 800)
                                                   styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                              NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable)
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    window.title = @"GMBasicIDE";
    window.movableByWindowBackground = NO;
    window.backgroundColor = VBColorFace();
    window.minSize = NSMakeSize(940, 640);
    window.tabbingMode = NSWindowTabbingModeDisallowed;

    self = [super initWithWindow:window];
    if (!self) return nil;

    _moduleCodes = [NSMutableDictionary dictionary];
    _dirtyModules = [NSMutableSet set];
    [self buildInterface];
    [window center];
    window.acceptsMouseMovedEvents = YES;
    return self;
}

- (BOOL)hasProject { return _project != NULL; }
- (BOOL)isRunning { return _runtime != nil; }

#pragma mark - Menus


static NSImage *VBMenuImage(NSString *title)
{
    static NSMutableDictionary<NSString *, NSImage *> *cache;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ cache = [NSMutableDictionary dictionary]; });
    NSImage *cached = cache[title];
    if (cached) return cached;
    NSString *icon = nil;
    if ([title hasPrefix:@"New Project"] || [title hasPrefix:@"Add Project"]) icon = @"addproject";
    else if ([title hasPrefix:@"Open Project"] || [title isEqualToString:@"Open Project..."]) icon = @"open";
    else if ([title hasPrefix:@"Save"] || [title hasPrefix:@"Save Form"]) icon = @"save";
    else if ([title hasPrefix:@"Print"]) icon = @"print";
    else if ([title hasPrefix:@"Make Project"]) icon = @"play";
    else if ([title isEqualToString:@"Undo"]) icon = @"undo";
    else if ([title isEqualToString:@"Redo"]) icon = @"redo";
    else if ([title isEqualToString:@"Cut"]) icon = @"cut";
    else if ([title isEqualToString:@"Copy"]) icon = @"copy";
    else if ([title isEqualToString:@"Paste"]) icon = @"paste";
    else if ([title hasPrefix:@"Find"] || [title hasPrefix:@"Replace"]) icon = @"find";
    else if ([title hasPrefix:@"Add Procedure"]) icon = @"addprocedure";
    else if ([title hasPrefix:@"Add Form"] || [title hasPrefix:@"Add MDI"]) icon = @"addform";
    else if ([title hasPrefix:@"Add Module"]) icon = @"addmodule";
    else if ([title hasPrefix:@"Add Class"]) icon = @"addclassmodule";
    else if ([title hasPrefix:@"Add User Control"]) icon = @"addusercontrol";
    else if ([title hasPrefix:@"Add Property Page"]) icon = @"addpropertypage";
    else if ([title hasPrefix:@"Add File"]) icon = @"insertableobjects";
    else if ([title hasPrefix:@"References"]) icon = @"references";
    else if ([title hasPrefix:@"Components"]) icon = @"controls";
    else if ([title hasPrefix:@"Properties"]) icon = @"properties";
    else if ([title isEqualToString:@"Object Browser"]) icon = @"objectbrowser";
    else if ([title isEqualToString:@"Form Layout Window"]) icon = @"formlayout";
    else if ([title isEqualToString:@"Toolbox"]) icon = @"toolbox";
    else if ([title isEqualToString:@"Project Explorer"]) icon = @"view_projectexplorer";
    else if ([title isEqualToString:@"Code"] || [title isEqualToString:@"Show Code"]) icon = @"viewcode";
    else if ([title isEqualToString:@"Object"] || [title isEqualToString:@"Show Designer"]) icon = @"viewobject";
    else if ([title hasPrefix:@"Debug Project"] || [title isEqualToString:@"Start"] ||
             [title hasPrefix:@"Start With"]) icon = @"play";
    else if ([title isEqualToString:@"Break"]) icon = @"pause";
    else if ([title isEqualToString:@"End"]) icon = @"stop";
    else if ([title isEqualToString:@"Restart"]) icon = @"play";
    else if ([title hasPrefix:@"Step Into"]) icon = @"stepinto";
    else if ([title hasPrefix:@"Step Over"]) icon = @"stepover";
    else if ([title hasPrefix:@"Step Out"]) icon = @"stepout";
    else if ([title hasPrefix:@"Toggle Breakpoint"]) icon = @"togglebreakpoint";
    else if ([title hasPrefix:@"Clear All Breakpoints"]) icon = @"setnextstatement";
    else if ([title hasPrefix:@"Menu Editor"]) icon = @"menubar";
    else if ([title hasPrefix:@"Options"]) icon = @"controls";
    else if ([title hasPrefix:@"Terminal"]) icon = @"codeeditor";
    else if ([title hasPrefix:@"Theme"]) icon = @"avalonia_visualbasic";
    else if ([title hasPrefix:@"Contents"]) icon = @"contents";
    else if ([title hasPrefix:@"Technical Support"]) icon = @"technicalsupport";
    else if ([title hasPrefix:@"About"]) icon = @"about";
    if (!icon) return nil;
    NSString *path = [[NSBundle mainBundle] pathForResource:icon ofType:@"gif" inDirectory:@"icons"];
    NSImage *image = path ? [[NSImage alloc] initWithContentsOfFile:path] : nil;
    if (image) cache[title] = image;
    return image;
}

- (NSMenuItem *)menuItem:(NSString *)title action:(SEL)action key:(NSString *)key
{
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:key ?: @""];
    item.target = action ? self : nil;
    item.image = VBMenuImage(title);
    return item;
}

- (NSMenuItem *)responderItem:(NSString *)title action:(SEL)action key:(NSString *)key
{
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:key ?: @""];
    item.target = nil;
    item.image = VBMenuImage(title);
    return item;
}

- (NSMenuItem *)disabledItem:(NSString *)title key:(NSString *)key
{
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:nil keyEquivalent:key ?: @""];
    item.enabled = NO;
    item.image = VBMenuImage(title);
    return item;
}

- (NSMenu *)fileMenu
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"File"];
    [menu addItem:[self menuItem:@"New Project" action:@selector(gmbNewProject:) key:@"n"]];
    [menu addItem:[self menuItem:@"Open Project..." action:@selector(gmbOpenProject:) key:@"o"]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self disabledItem:@"Add Project..." key:@""]];
    [menu addItem:[self disabledItem:@"Remove Project" key:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self menuItem:@"Save Project" action:@selector(saveProject) key:@"s"]];
    [menu addItem:[self menuItem:@"Save Project As..." action:@selector(saveProjectAs) key:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self menuItem:@"Save Form" action:@selector(saveCurrentForm) key:@""]];
    [menu addItem:[self menuItem:@"Save Form As..." action:@selector(saveCurrentFormAs) key:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self menuItem:@"Print..." action:@selector(printCurrentView:) key:@"p"]];
    [menu addItem:[self menuItem:@"Print Setup..." action:@selector(printSetup:) key:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self menuItem:@"Make Project" action:@selector(gmbBuildProject:) key:@""]];
    [menu addItem:[self disabledItem:@"Make Project Group..." key:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self responderItem:@"Exit" action:@selector(terminate:) key:@"q"]];
    return menu;
}

- (NSMenu *)editMenu
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Edit"];
    [menu addItem:[self responderItem:@"Undo" action:@selector(undo:) key:@"z"]];
    [menu addItem:[self responderItem:@"Redo" action:@selector(redo:) key:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self responderItem:@"Cut" action:@selector(cut:) key:@"x"]];
    [menu addItem:[self responderItem:@"Copy" action:@selector(copy:) key:@"c"]];
    [menu addItem:[self responderItem:@"Paste" action:@selector(paste:) key:@"v"]];
    [menu addItem:[self disabledItem:@"Paste Link" key:@""]];
    [menu addItem:[self responderItem:@"Remove" action:@selector(delete:) key:@""]];
    [menu addItem:[self responderItem:@"Delete" action:@selector(delete:) key:@""]];
    [menu addItem:[self responderItem:@"Select All" action:@selector(selectAll:) key:@"a"]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self menuItem:@"Find..." action:@selector(showFindPanel:) key:@"f"]];
    [menu addItem:[self menuItem:@"Find Next" action:@selector(findNext:) key:@""]];
    [menu addItem:[self menuItem:@"Find Previous" action:@selector(findPrevious:) key:@""]];
    [menu addItem:[self menuItem:@"Replace..." action:@selector(showFindPanel:) key:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self menuItem:@"Indent" action:@selector(indentSelection:) key:@""]];
    [menu addItem:[self menuItem:@"Outdent" action:@selector(outdentSelection:) key:@""]];
    [menu addItem:[self menuItem:@"Insert File..." action:@selector(insertSourceFile:) key:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self disabledItem:@"List Properties/Methods" key:@""]];
    [menu addItem:[self disabledItem:@"List Constants" key:@""]];
    [menu addItem:[self disabledItem:@"Quick Info" key:@""]];
    [menu addItem:[self disabledItem:@"Parameter Info" key:@""]];
    NSMenuItem *complete = [[NSMenuItem alloc] initWithTitle:@"Complete Word"
                                                     action:@selector(completeWord:)
                                              keyEquivalent:@" "];
    complete.keyEquivalentModifierMask = NSEventModifierFlagControl;
    complete.target = self;
    [menu addItem:complete];
    [menu addItem:[NSMenuItem separatorItem]];
    {
        NSMenu *bookmarks = [[NSMenu alloc] initWithTitle:@"Bookmarks"];
        [bookmarks addItem:[self menuItem:@"Toggle Bookmark" action:@selector(toggleBookmark:) key:@""]];
        [bookmarks addItem:[self menuItem:@"Next Bookmark" action:@selector(nextBookmark:) key:@""]];
        [bookmarks addItem:[self menuItem:@"Previous Bookmark" action:@selector(previousBookmark:) key:@""]];
        [bookmarks addItem:[self menuItem:@"Clear All Bookmarks" action:@selector(clearBookmarks:) key:@""]];
        NSMenuItem *root = [[NSMenuItem alloc] initWithTitle:@"Bookmarks" action:nil keyEquivalent:@""];
        root.submenu = bookmarks;
        [menu addItem:root];
    }
    return menu;
}

- (NSMenu *)viewMenu
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"View"];
    [menu addItem:[self menuItem:@"Code" action:@selector(showCode) key:@"F7"]];
    [menu addItem:[self menuItem:@"Object" action:@selector(showDesigner) key:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self menuItem:@"Definition" action:@selector(jumpToDefinition:) key:@""]];
    [menu addItem:[self menuItem:@"Last Position" action:@selector(jumpToLastPosition:) key:@""]];
    [menu addItem:[self menuItem:@"Object Browser" action:@selector(showObjectBrowser:) key:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self disabledItem:@"Immediate Window" key:@""]];
    [menu addItem:[self disabledItem:@"Locals Window" key:@""]];
    [menu addItem:[self disabledItem:@"Watch Window" key:@""]];
    [menu addItem:[self disabledItem:@"Call Stack..." key:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self menuItem:@"Project Explorer" action:@selector(showProjectExplorer:) key:@""]];
    [menu addItem:[self menuItem:@"Properties Window" action:@selector(showPropertiesWindow:) key:@""]];
    [menu addItem:[self menuItem:@"Properties Alphabetical" action:@selector(propertiesAlphabetical:) key:@""]];
    [menu addItem:[self menuItem:@"Properties Categorized" action:@selector(propertiesCategorized:) key:@""]];
    [menu addItem:[self menuItem:@"Form Layout Window" action:@selector(showFormLayout:) key:@""]];
    [menu addItem:[self disabledItem:@"Property Pages" key:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self menuItem:@"Toolbox" action:@selector(showToolbox:) key:@""]];
    [menu addItem:[self menuItem:@"Tab Order..." action:@selector(showTabOrder:) key:@""]];
    [menu addItem:[self menuItem:@"Color Palette (ForeColor)" action:@selector(showColorPaletteFore:) key:@""]];
    [menu addItem:[self menuItem:@"Color Palette (BackColor)" action:@selector(showColorPaletteBackColor:) key:@""]];
    {
        NSMenu *toolbars = [[NSMenu alloc] initWithTitle:@"Toolbars"];
        NSMenuItem *standard = [[NSMenuItem alloc] initWithTitle:@"Standard" action:@selector(toggleStandardToolbar:) keyEquivalent:@""];
        standard.target = self;
        standard.state = NSControlStateValueOn;
        [toolbars addItem:standard];
        NSMenuItem *format = [[NSMenuItem alloc] initWithTitle:@"Format" action:@selector(toggleFormatToolbar:) keyEquivalent:@""];
        format.target = self;
        format.state = NSControlStateValueOn;
        [toolbars addItem:format];
        NSMenuItem *root = [[NSMenuItem alloc] initWithTitle:@"Toolbars" action:nil keyEquivalent:@""];
        root.submenu = toolbars;
        [menu addItem:root];
    }
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self disabledItem:@"Visual Component Manager" key:@""]];
    return menu;
}

- (NSMenu *)projectMenu
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Project"];
    [menu addItem:[self menuItem:@"Add Form" action:@selector(newForm) key:@""]];
    [menu addItem:[self disabledItem:@"Add MDI Form" key:@""]];
    [menu addItem:[self menuItem:@"Add Module" action:@selector(gmbAddModule) key:@""]];
    [menu addItem:[self menuItem:@"Add Class Module" action:@selector(gmbAddClass) key:@""]];
    [menu addItem:[self menuItem:@"Add User Control" action:@selector(gmbAddUserControl) key:@""]];
    [menu addItem:[self disabledItem:@"Add Property Page" key:@""]];
    [menu addItem:[self disabledItem:@"Add User Document" key:@""]];
    [menu addItem:[self disabledItem:@"More ActiveX Designers..." key:@""]];
    [menu addItem:[self menuItem:@"Add File..." action:@selector(gmbAddFile) key:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self menuItem:@"Remove Form" action:@selector(removeCurrentForm:) key:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self menuItem:@"References..." action:@selector(showReferences:) key:@""]];
    [menu addItem:[self menuItem:@"Components..." action:@selector(showComponents:) key:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self menuItem:@"Export Form..." action:@selector(gmbExportForm:) key:@""]];
    [menu addItem:[self menuItem:@"Export All Forms..." action:@selector(gmbExportAllForms:) key:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self menuItem:@"Properties..." action:@selector(projectProperties:) key:@""]];
    return menu;
}

- (NSMenuItem *)formatSubmenuItem:(NSString *)title menu:(NSMenu *)submenu
{
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:nil keyEquivalent:@""];
    item.submenu = submenu;
    return item;
}

- (NSMenu *)formatMenu
{
    BOOL canAlign = [_designer canAlignSelection];
    BOOL canSpace = [_designer canSpaceSelection];
    BOOL canOrder = [_designer canReorderSelection];
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Format"];

    NSMenu *align = [[NSMenu alloc] initWithTitle:@"Align"];
    NSArray<NSDictionary *> *alignItems = @[
        @{@"title": @"Lefts", @"action": @"alignLefts:", @"key": @""},
        @{@"title": @"Centers", @"action": @"alignCenters:", @"key": @""},
        @{@"title": @"Rights", @"action": @"alignRights:", @"key": @""},
        @{@"title": @"Tops", @"action": @"alignTops:", @"key": @""},
        @{@"title": @"Middles", @"action": @"alignMiddles:", @"key": @""},
        @{@"title": @"Bottoms", @"action": @"alignBottoms:", @"key": @""},
    ];
    for (NSDictionary *spec in alignItems) {
        NSMenuItem *item = [self menuItem:spec[@"title"] action:NSSelectorFromString(spec[@"action"]) key:spec[@"key"]];
        item.enabled = canAlign;
        [align addItem:item];
    }

    NSMenu *size = [[NSMenu alloc] initWithTitle:@"Make Same Size"];
    NSArray<NSDictionary *> *sizeItems = @[
        @{@"title": @"Width", @"action": @"sameWidth:"},
        @{@"title": @"Height", @"action": @"sameHeight:"},
        @{@"title": @"Both", @"action": @"sameSize:"},
    ];
    for (NSDictionary *spec in sizeItems) {
        NSMenuItem *item = [self menuItem:spec[@"title"] action:NSSelectorFromString(spec[@"action"]) key:@""];
        item.enabled = canAlign;
        [size addItem:item];
    }

    NSMenu *horizontal = [[NSMenu alloc] initWithTitle:@"Horizontal Spacing"];
    NSArray<NSDictionary *> *spacingItems = @[
        @{@"title": @"Make Equal", @"action": @"spaceHorizontalEqual:"},
        @{@"title": @"Increase", @"action": @"spaceHorizontalIncrease:"},
        @{@"title": @"Decrease", @"action": @"spaceHorizontalDecrease:"},
        @{@"title": @"Remove", @"action": @"spaceHorizontalRemove:"},
    ];
    for (NSDictionary *spec in spacingItems) {
        NSMenuItem *item = [self menuItem:spec[@"title"] action:NSSelectorFromString(spec[@"action"]) key:@""];
        item.enabled = canSpace;
        [horizontal addItem:item];
    }

    NSMenu *vertical = [[NSMenu alloc] initWithTitle:@"Vertical Spacing"];
    NSArray<NSDictionary *> *verticalItems = @[
        @{@"title": @"Make Equal", @"action": @"spaceVerticalEqual:"},
        @{@"title": @"Increase", @"action": @"spaceVerticalIncrease:"},
        @{@"title": @"Decrease", @"action": @"spaceVerticalDecrease:"},
        @{@"title": @"Remove", @"action": @"spaceVerticalRemove:"},
    ];
    for (NSDictionary *spec in verticalItems) {
        NSMenuItem *item = [self menuItem:spec[@"title"] action:NSSelectorFromString(spec[@"action"]) key:@""];
        item.enabled = canSpace;
        [vertical addItem:item];
    }

    NSMenuItem *alignItem = [self formatSubmenuItem:@"Align" menu:align];
    alignItem.enabled = canAlign;
    [menu addItem:alignItem];
    NSMenuItem *sizeItem = [self formatSubmenuItem:@"Make Same Size" menu:size];
    sizeItem.enabled = canAlign;
    [menu addItem:sizeItem];
    NSMenuItem *horizontalItem = [self formatSubmenuItem:@"Horizontal Spacing" menu:horizontal];
    horizontalItem.enabled = canSpace;
    [menu addItem:horizontalItem];
    NSMenuItem *verticalItem = [self formatSubmenuItem:@"Vertical Spacing" menu:vertical];
    verticalItem.enabled = canSpace;
    [menu addItem:verticalItem];
    [menu addItem:[NSMenuItem separatorItem]];
    NSMenuItem *center = [self menuItem:@"Center in Form" action:@selector(centerSelectionInForm:) key:@""];
    center.enabled = _designer.selectionCount > 0;
    [menu addItem:center];
    [menu addItem:[NSMenuItem separatorItem]];
    NSMenu *order = [[NSMenu alloc] initWithTitle:@"Order"];
    NSMenuItem *toFront = [self menuItem:@"Bring to Front" action:@selector(bringToFront:) key:@""];
    toFront.enabled = canOrder;
    [order addItem:toFront];
    NSMenuItem *toBack = [self menuItem:@"Send to Back" action:@selector(sendToBack:) key:@""];
    toBack.enabled = canOrder;
    [order addItem:toBack];
    NSMenuItem *orderItem = [self formatSubmenuItem:@"Order" menu:order];
    orderItem.enabled = canOrder;
    [menu addItem:orderItem];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self menuItem:@"Size to Grid" action:@selector(sizeSelectionToGrid:) key:@""]];
    NSMenuItem *lock = [[NSMenuItem alloc] initWithTitle:@"Lock Controls" action:@selector(toggleLockControls:) keyEquivalent:@""];
    lock.target = self;
    [menu addItem:lock];
    return menu;
}

#pragma mark - Format actions

- (void)updateFormatMenuState
{
    BOOL canAlign = [_designer canAlignSelection];
    BOOL canSpace = [_designer canSpaceSelection];
    BOOL canOrder = [_designer canReorderSelection];
    BOOL hasSelection = _designer.selectionCount > 0;
    if (_formatMenuStateValid && canAlign == _formatCanAlign && canSpace == _formatCanSpace &&
        canOrder == _formatCanOrder && hasSelection == _formatHasSelection) {
        return;
    }
    _formatMenuStateValid = YES;
    _formatCanAlign = canAlign;
    _formatCanSpace = canSpace;
    _formatCanOrder = canOrder;
    _formatHasSelection = hasSelection;
    [self buildMenuBar];
}

- (void)refreshFormatState
{
    [self updateFormatMenuState];
    [self updateToolbarState];
}

- (void)alignLefts:(id)sender { (void)sender; [_designer alignSelection:VBDesignerAlignLeft]; [self refreshFormatState]; }
- (void)alignCenters:(id)sender { (void)sender; [_designer alignSelection:VBDesignerAlignCenter]; [self refreshFormatState]; }
- (void)alignRights:(id)sender { (void)sender; [_designer alignSelection:VBDesignerAlignRight]; [self refreshFormatState]; }
- (void)alignTops:(id)sender { (void)sender; [_designer alignSelection:VBDesignerAlignTop]; [self refreshFormatState]; }
- (void)alignMiddles:(id)sender { (void)sender; [_designer alignSelection:VBDesignerAlignMiddle]; [self refreshFormatState]; }
- (void)alignBottoms:(id)sender { (void)sender; [_designer alignSelection:VBDesignerAlignBottom]; [self refreshFormatState]; }
- (void)sameWidth:(id)sender { (void)sender; [_designer makeSameSize:VBDesignerSizeWidth]; [self refreshFormatState]; }
- (void)sameHeight:(id)sender { (void)sender; [_designer makeSameSize:VBDesignerSizeHeight]; [self refreshFormatState]; }
- (void)sameSize:(id)sender { (void)sender; [_designer makeSameSize:VBDesignerSizeBoth]; [self refreshFormatState]; }
- (void)spaceHorizontalEqual:(id)sender
{
    (void)sender;
    [_designer spaceSelection:VBDesignerSpacingMakeEqual horizontal:YES];
    [self refreshFormatState];
}
- (void)spaceHorizontalIncrease:(id)sender
{
    (void)sender;
    [_designer spaceSelection:VBDesignerSpacingIncrease horizontal:YES];
    [self refreshFormatState];
}
- (void)spaceHorizontalDecrease:(id)sender
{
    (void)sender;
    [_designer spaceSelection:VBDesignerSpacingDecrease horizontal:YES];
    [self refreshFormatState];
}
- (void)spaceHorizontalRemove:(id)sender
{
    (void)sender;
    [_designer spaceSelection:VBDesignerSpacingRemove horizontal:YES];
    [self refreshFormatState];
}
- (void)spaceVerticalEqual:(id)sender
{
    (void)sender;
    [_designer spaceSelection:VBDesignerSpacingMakeEqual horizontal:NO];
    [self refreshFormatState];
}
- (void)spaceVerticalIncrease:(id)sender
{
    (void)sender;
    [_designer spaceSelection:VBDesignerSpacingIncrease horizontal:NO];
    [self refreshFormatState];
}
- (void)spaceVerticalDecrease:(id)sender
{
    (void)sender;
    [_designer spaceSelection:VBDesignerSpacingDecrease horizontal:NO];
    [self refreshFormatState];
}
- (void)spaceVerticalRemove:(id)sender
{
    (void)sender;
    [_designer spaceSelection:VBDesignerSpacingRemove horizontal:NO];
    [self refreshFormatState];
}
- (void)centerSelectionHorizontally:(id)sender
{
    (void)sender;
    if ([_designer centerSelectionHorizontally]) [self refreshFormatState];
}

- (void)centerSelectionVertically:(id)sender
{
    (void)sender;
    if ([_designer centerSelectionVertically]) [self refreshFormatState];
}

- (void)sizeSelectionToGrid:(id)sender
{
    (void)sender;
    if ([_designer sizeSelectionToGrid]) {
        _projectDirty = YES;
        [_properties reload];
        [self updateTitle];
    }
}

- (void)toggleLockControls:(NSMenuItem *)sender
{
    _designer.locked = !_designer.locked;
    sender.state = _designer.locked ? NSControlStateValueOn : NSControlStateValueOff;
}

- (void)centerSelectionInForm:(id)sender
{
    (void)sender;
    [_designer centerSelectionInForm];
    [self refreshFormatState];
}
- (void)bringToFront:(id)sender
{
    (void)sender;
    [_designer bringSelectionToFront];
    [self refreshFormatState];
}
- (void)sendToBack:(id)sender
{
    (void)sender;
    [_designer sendSelectionToBack];
    [self refreshFormatState];
}

- (void)showTabOrder:(id)sender
{
    (void)sender;
    VbForm *form = (_currentItem && _currentItem->form) ? _currentItem->form : NULL;
    VbControl *container;
    if (!form || !form->root) {
        NSBeep();
        return;
    }
    container = form->root;
    if (_designer.selected && _designer.selected != form->root) {
        VbControl *selected = _designer.selected;
        /* A selected container (Frame, PictureBox) edits its own controls. */
        if (vb_control_is_container(selected->type)) container = selected;
        else if (selected->parent) container = selected->parent;
    }
    if ([VBTabOrderWindow runForContainer:container]) {
        form->dirty = 1;
        _projectDirty = YES;
        [_designer refresh];
        [_properties reload];
        [self updateTitle];
        [self updateStatus];
    }
}

- (NSMenu *)debugMenu
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Debug"];
    [menu addItem:[self menuItem:@"Debug Project" action:@selector(gmbDebugProject:) key:@"F5"]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self menuItem:@"Step Into" action:@selector(debugStepInto:) key:@"F8"]];
    [menu addItem:[self menuItem:@"Step Over" action:@selector(debugStepOver:) key:@"F10"]];
    [menu addItem:[self menuItem:@"Step Out" action:@selector(debugStepOut:) key:@"F11"]];
    [menu addItem:[self disabledItem:@"Run To Cursor" key:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self disabledItem:@"Add Watch..." key:@""]];
    [menu addItem:[self disabledItem:@"Edit Watch..." key:@""]];
    [menu addItem:[self disabledItem:@"Quick Watch..." key:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self menuItem:@"Toggle Breakpoint" action:@selector(toggleBreakpointAtCurrentLine:) key:@"F9"]];
    [menu addItem:[self menuItem:@"Clear All Breakpoints" action:@selector(clearAllBreakpoints:) key:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self disabledItem:@"Set Next Statement" key:@""]];
    [menu addItem:[self disabledItem:@"Show Next Statement" key:@""]];
    return menu;
}

- (NSMenu *)runMenu
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Run"];
    [menu addItem:[self menuItem:@"Start" action:@selector(gmbRunProject:) key:@"F5"]];
    [menu addItem:[self menuItem:@"Start With Full Compile" action:@selector(gmbRunProject:) key:@""]];
    [menu addItem:[self disabledItem:@"Break" key:@""]];
    [menu addItem:[self menuItem:@"End" action:@selector(debugStop:) key:@""]];
    [menu addItem:[self menuItem:@"Restart" action:@selector(debugRestart:) key:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self menuItem:@"Run Form..." action:@selector(gmbRunForm:) key:@""]];
    return menu;
}

- (NSMenu *)toolsMenu
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Tools"];
    [menu addItem:[self menuItem:@"Add Procedure..." action:@selector(addProcedure:) key:@""]];
    [menu addItem:[self disabledItem:@"Procedure Attributes..." key:@""]];
    [menu addItem:[self disabledItem:@"Menu Editor..." key:@""]];
    [menu addItem:[self disabledItem:@"Options..." key:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self menuItem:@"Terminal" action:@selector(gmbTerminal:) key:@""]];
    NSMenuItem *themeRoot = [[NSMenuItem alloc] initWithTitle:@"Theme" action:nil keyEquivalent:@""];
    themeRoot.submenu = [self themeMenu];
    [menu addItem:themeRoot];
    return menu;
}

- (NSMenu *)queryMenu
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Query"];
    [menu addItem:[self disabledItem:@"Run Query" key:@""]];
    [menu addItem:[self disabledItem:@"Clear Results" key:@""]];
    return menu;
}

- (NSMenu *)diagramMenu
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Diagram"];
    [menu addItem:[self disabledItem:@"New Diagram..." key:@""]];
    return menu;
}

- (NSMenu *)addInsMenu
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Add-Ins"];
    [menu addItem:[self disabledItem:@"Visual Data Manager..." key:@""]];
    [menu addItem:[self disabledItem:@"Add-In Manager..." key:@""]];
    return menu;
}

- (NSMenu *)themeMenu
{
    NSArray<NSString *> *themes = @[@"vb6", @"turbo", @"qbasic", @"light", @"midnight", @"nord", @"dracula", @"monokai", @"solarized", @"retrogreen", @"amber", @"soft", @"neon", @"glass"];
    NSMutableArray<NSString *> *ordered = [themes mutableCopy];
    [ordered removeObject:@"vb6"];
    [ordered insertObject:@"vb6" atIndex:0];
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Theme"];
    for (NSString *name in ordered) {
        NSString *title = [name isEqualToString:@"vb6"] ? @"vb6 (default)" : name;
        NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:@selector(gmbSelectTheme:) keyEquivalent:@""];
        item.target = self;
        item.representedObject = name;
        item.state = [name isEqualToString:[VBGmbRunner themeName]] ? NSControlStateValueOn : NSControlStateValueOff;
        [menu addItem:item];
    }
    return menu;
}

- (NSMenu *)buildMenu
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Build"];
    [menu addItem:[self menuItem:@"Check Project" action:@selector(gmbCheckProject:) key:@""]];
    [menu addItem:[self menuItem:@"Build Project" action:@selector(gmbBuildProject:) key:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self menuItem:@"Set Toolchain..." action:@selector(gmbSetToolchain:) key:@""]];
    return menu;
}

- (NSMenu *)windowMenu
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Window"];
    [menu addItem:[self disabledItem:@"Split" key:@""]];
    [menu addItem:[self disabledItem:@"Tile Horizontally" key:@""]];
    [menu addItem:[self disabledItem:@"Tile Vertically" key:@""]];
    [menu addItem:[self disabledItem:@"Cascade" key:@""]];
    [menu addItem:[self disabledItem:@"Arrange Icons" key:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self menuItem:@"Show Code" action:@selector(showCode) key:@""]];
    [menu addItem:[self menuItem:@"Show Designer" action:@selector(showDesigner) key:@""]];
    return menu;
}

- (NSMenu *)helpMenu
{
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Help"];
    [menu addItem:[self disabledItem:@"Contents..." key:@""]];
    [menu addItem:[self disabledItem:@"Index..." key:@""]];
    [menu addItem:[self disabledItem:@"Search..." key:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self disabledItem:@"Technical Support" key:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self menuItem:@"About GMBasicIDE" action:@selector(showAbout:) key:@""]];
    return menu;
}

- (void)buildMenuBar
{
    NSArray<NSString *> *titles = @[@"File", @"Edit", @"View", @"Project", @"Format", @"Debug", @"Run",
                                    @"Query", @"Diagram", @"Build", @"Tools", @"Add-Ins", @"Window", @"Help"];
    NSArray<NSMenu *> *menus = @[[self fileMenu], [self editMenu], [self viewMenu], [self projectMenu],
                                 [self formatMenu], [self debugMenu], [self runMenu],
                                 [self queryMenu], [self diagramMenu],
                                 [self buildMenu], [self toolsMenu], [self addInsMenu],
                                 [self windowMenu], [self helpMenu]];
    [_menuBar setTitles:titles menus:menus];
}

#pragma mark - Interface

- (void)buildInterface
{
    NSView *content = self.window.contentView;
    CGFloat width = NSWidth(content.bounds);
    CGFloat height = NSHeight(content.bounds);
    content.wantsLayer = YES;
    content.layer.backgroundColor = [VBColorFace() CGColor];

    _menuBar = [[VBMenuBarView alloc] initWithFrame:NSMakeRect(0, height - VB_MENU_HEIGHT, width, VB_MENU_HEIGHT)];
    _menuBar.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    [content addSubview:_menuBar];
    [self buildMenuBar];

    _toolbar = [[VBToolbarView alloc] initWithFrame:NSMakeRect(0, height - VB_MENU_HEIGHT - 26, width, 26)];
    _toolbar.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    _toolbar.delegate = self;
    [content addSubview:_toolbar];
    [_toolbar setButtons:@[
        @{@"identifier": @"new", @"icon": @"new", @"tooltip": @"New Form"},
        @{@"identifier": @"open", @"icon": @"open", @"tooltip": @"Open Form"},
        @{@"identifier": @"save", @"icon": @"save", @"tooltip": @"Save Form"},
        @{@"separator": @YES},
        @{@"identifier": @"cut", @"icon": @"cut", @"tooltip": @"Cut"},
        @{@"identifier": @"copy", @"icon": @"copy", @"tooltip": @"Copy"},
        @{@"identifier": @"paste", @"icon": @"paste", @"tooltip": @"Paste"},
        @{@"identifier": @"explorer", @"icon": @"explorer", @"tooltip": @"Project Explorer"},
        @{@"identifier": @"properties", @"icon": @"properties", @"tooltip": @"Properties Window"},
        @{@"identifier": @"toolbox", @"icon": @"toolbox", @"tooltip": @"Toolbox"},
    ]];

    _formatBar = [[VBToolbarView alloc] initWithFrame:NSMakeRect(0, height - VB_MENU_HEIGHT - 52, width, 26)];
    _formatBar.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    _formatBar.delegate = self;
    [content addSubview:_formatBar];
    [_formatBar setButtons:@[
        @{@"identifier": @"align-left", @"tooltip": @"Align Lefts"},
        @{@"identifier": @"align-center", @"tooltip": @"Align Centers"},
        @{@"identifier": @"align-right", @"tooltip": @"Align Rights"},
        @{@"identifier": @"align-top", @"tooltip": @"Align Tops"},
        @{@"identifier": @"align-middle", @"tooltip": @"Align Middles"},
        @{@"identifier": @"align-bottom", @"tooltip": @"Align Bottoms"},
        @{@"separator": @YES},
        @{@"identifier": @"same-width", @"tooltip": @"Make Same Width"},
        @{@"identifier": @"same-height", @"tooltip": @"Make Same Height"},
        @{@"identifier": @"same-size", @"tooltip": @"Make Same Size"},
        @{@"separator": @YES},
        @{@"identifier": @"space-h-equal", @"tooltip": @"Horizontal Spacing: Make Equal"},
        @{@"identifier": @"space-v-equal", @"tooltip": @"Vertical Spacing: Make Equal"},
    ]];

    _statusBar = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, width, 22)];
    _statusBar.autoresizingMask = NSViewWidthSizable | NSViewMaxYMargin;
    _statusBar.wantsLayer = YES;
    _statusBar.layer.backgroundColor = [VBColorFace() CGColor];
    [content addSubview:_statusBar];

    CGFloat leftWidth = MAX(160, width - 332);
    VBStatusPanelView *leftPanel = [[VBStatusPanelView alloc] initWithFrame:NSMakeRect(2, 2, leftWidth, 18)];
    leftPanel.autoresizingMask = NSViewWidthSizable;
    [_statusBar addSubview:leftPanel];

    VBStatusPanelView *modePanel = [[VBStatusPanelView alloc] initWithFrame:NSMakeRect(width - 326, 2, 122, 18)];
    modePanel.autoresizingMask = NSViewMinXMargin;
    [_statusBar addSubview:modePanel];

    VBStatusPanelView *posPanel = [[VBStatusPanelView alloc] initWithFrame:NSMakeRect(width - 200, 2, 196, 18)];
    posPanel.autoresizingMask = NSViewMinXMargin;
    [_statusBar addSubview:posPanel];

    _status = [NSTextField labelWithString:@"Ready"];
    _status.frame = NSMakeRect(8, 3, leftWidth - 12, 16);
    _status.font = VBFontUI();
    _status.textColor = VBColorText();
    _status.autoresizingMask = NSViewWidthSizable;
    [_statusBar addSubview:_status];

    _statusMode = [NSTextField labelWithString:@"DESIGN"];
    _statusMode.frame = NSMakeRect(width - 320, 3, 112, 16);
    _statusMode.font = VBFontUI();
    _statusMode.textColor = VBColorText();
    _statusMode.alignment = NSTextAlignmentLeft;
    _statusMode.autoresizingMask = NSViewMinXMargin;
    [_statusBar addSubview:_statusMode];

    _statusPos = [NSTextField labelWithString:@""];
    _statusPos.frame = NSMakeRect(width - 194, 3, 186, 16);
    _statusPos.font = VBFontUI();
    _statusPos.textColor = VBColorText();
    _statusPos.alignment = NSTextAlignmentLeft;
    _statusPos.autoresizingMask = NSViewMinXMargin;
    [_statusBar addSubview:_statusPos];

    _split = [[NSSplitView alloc] initWithFrame:NSMakeRect(0, 22, width, height - 22 - VB_MENU_HEIGHT - 52)];
    _split.vertical = YES;
    _split.dividerStyle = NSSplitViewDividerStyleThin;
    _split.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    [content addSubview:_split];

    _toolScroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, 84, 520)];
    _toolScroll.hasVerticalScroller = NO;
    _toolScroll.hasHorizontalScroller = NO;
    _toolScroll.borderType = NSNoBorder;
    _toolScroll.drawsBackground = YES;
    _toolScroll.backgroundColor = VBColorFace();
    _toolbox = [[VBToolboxView alloc] initWithFrame:NSMakeRect(0, 0, 82, 500)];
    _toolbox.delegate = self;
    _toolbox.frame = NSMakeRect(0, 0, 82, [_toolbox preferredHeight]);
    _toolScroll.documentView = _toolbox;
    [_split addSubview:_toolScroll];

    _workspace = [[VBMdiChildView alloc] initWithFrame:NSMakeRect(0, 0, width - 440, height - 70)];
    _workspace.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _workspace.wantsLayer = NO;
    [_split addSubview:_workspace];

    _designerWindow = [[VBMdiChildView alloc] initWithFrame:NSMakeRect(6, 6, 600, 480)];
    _designerWindow.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    _designerScroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, 500, 400)];
    _designerScroll.hasVerticalScroller = YES;
    _designerScroll.hasHorizontalScroller = YES;
    _designerScroll.borderType = NSNoBorder;
    _designerScroll.drawsBackground = YES;
    _designerScroll.backgroundColor = VBColorWorkspace();
    _designer = [[VBDesignerView alloc] initWithFrame:NSMakeRect(0, 0, 900, 700)];
    _designer.delegate = self;
    _designerScroll.documentView = _designer;
    [_designerWindow setTitleText:@"No form" content:_designerScroll];
    [_workspace addSubview:_designerWindow];

    _codeWindow = [[VBMdiChildView alloc] initWithFrame:NSMakeRect(6, 6, 600, 480)];
    _codeWindow.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _codeEditor = [[VBCodeEditor alloc] initWithFrame:NSMakeRect(0, 0, 500, 400)];
    _codeEditor.delegate = self;
    [_codeWindow setTitleText:@"Code" content:_codeEditor];
    _codeWindow.hidden = YES;
    [_workspace addSubview:_codeWindow];

    _rightSplit = [[NSSplitView alloc] initWithFrame:NSMakeRect(0, 0, 286, height - 70)];
    _rightSplit.vertical = NO;
    _rightSplit.dividerStyle = NSSplitViewDividerStyleThin;
    _rightSplit.autoresizingMask = NSViewHeightSizable;
    [_split addSubview:_rightSplit];

    _outline = [[VBProjectOutlineView alloc] initWithFrame:NSMakeRect(0, 0, 286, 260)];
    _outline.delegate = self;
    [_rightSplit addSubview:_outline];

    _properties = [[VBPropertyPanel alloc] initWithFrame:NSMakeRect(0, 0, 286, 400)];
    _properties.delegate = self;
    [_rightSplit addSubview:_properties];

    _formLayoutDock = [[VBFormLayoutView alloc] initWithFrame:NSMakeRect(0, 0, 286, 120)];
    _formLayoutDock.showsHeader = YES;
    [_rightSplit addSubview:_formLayoutDock];

    [self updateTitle];
    [self updateStatus];
    [self updateToolbarState];
}

- (void)showWindow:(id)sender
{
    [super showWindow:sender];
    [self.window layoutIfNeeded];
    [_split adjustSubviews];
    [_split setPosition:84 ofDividerAtIndex:0];
    [_split setPosition:NSWidth(_split.bounds) - 288 ofDividerAtIndex:1];
    [_rightSplit setPosition:NSHeight(_rightSplit.bounds) * 0.40 ofDividerAtIndex:0];
    [_toolbox setFrameSize:NSMakeSize(82, [_toolbox preferredHeight])];
    [self layoutWorkspace];
}

- (void)layoutWorkspace
{
    CGFloat w = NSWidth(_workspace.bounds);
    CGFloat h = NSHeight(_workspace.bounds);
    _designerWindow.frame = NSMakeRect(4, 4, MAX(120, w - 8), MAX(120, h - 8));
    _codeWindow.frame = NSMakeRect(4, 4, MAX(120, w - 8), MAX(120, h - 8));
}

- (void)updateToolbarState
{

    BOOL canAlign = [_designer canAlignSelection];
    BOOL canSpace = [_designer canSpaceSelection];
    for (NSString *identifier in @[@"align-left", @"align-center", @"align-right",
                                  @"align-top", @"align-middle", @"align-bottom",
                                  @"same-width", @"same-height", @"same-size"]) {
        [_formatBar setEnabled:canAlign forIdentifier:identifier];
    }
    for (NSString *identifier in @[@"space-h-equal", @"space-v-equal"]) {
        [_formatBar setEnabled:canSpace forIdentifier:identifier];
    }
}

- (void)toolbarView:(VBToolbarView *)toolbar didSelectIdentifier:(NSString *)identifier
{
    (void)toolbar;
    if ([identifier isEqualToString:@"new"]) [self newForm];
    else if ([identifier isEqualToString:@"open"]) [self openFormDocument];
    else if ([identifier isEqualToString:@"save"]) [self saveCurrentForm];
    else if ([identifier isEqualToString:@"cut"]) [NSApp sendAction:@selector(cut:) to:nil from:self];
    else if ([identifier isEqualToString:@"copy"]) [NSApp sendAction:@selector(copy:) to:nil from:self];
    else if ([identifier isEqualToString:@"paste"]) [NSApp sendAction:@selector(paste:) to:nil from:self];
    else if ([identifier isEqualToString:@"explorer"]) [self.window makeFirstResponder:_outline];
    else if ([identifier isEqualToString:@"properties"]) [self.window makeFirstResponder:_properties];
    else if ([identifier isEqualToString:@"toolbox"]) [self.window makeFirstResponder:_toolbox];
    else if ([identifier isEqualToString:@"align-left"]) [self alignLefts:nil];
    else if ([identifier isEqualToString:@"align-center"]) [self alignCenters:nil];
    else if ([identifier isEqualToString:@"align-right"]) [self alignRights:nil];
    else if ([identifier isEqualToString:@"align-top"]) [self alignTops:nil];
    else if ([identifier isEqualToString:@"align-middle"]) [self alignMiddles:nil];
    else if ([identifier isEqualToString:@"align-bottom"]) [self alignBottoms:nil];
    else if ([identifier isEqualToString:@"same-width"]) [self sameWidth:nil];
    else if ([identifier isEqualToString:@"same-height"]) [self sameHeight:nil];
    else if ([identifier isEqualToString:@"same-size"]) [self sameSize:nil];
    else if ([identifier isEqualToString:@"space-h-equal"]) [self spaceHorizontalEqual:nil];
    else if ([identifier isEqualToString:@"space-v-equal"]) [self spaceVerticalEqual:nil];
}

- (void)openProjectDocument
{
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.title = @"Open Project";
    panel.allowedContentTypes = @[[UTType typeWithFilenameExtension:@"vbp"]];
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    if ([panel runModal] == NSModalResponseOK && panel.URL) {
        [self loadProjectAtPath:panel.URL.path];
    }
}

/* Debug > Break / Run > Break (and the toolbar Break button): suspend the program on
   the next statement, like Ctrl+Break in VB6. F8 then steps, F5 continues. */
- (void)runDebugHandlerMenu
{
    if (!_runtime) {
        NSBeep();
        return;
    }
    [_runtime requestBreak];
    [_runtime.window makeKeyAndOrderFront:nil];
}

- (void)completeWord:(id)sender
{
    (void)sender;
    [_codeEditor completeWord];
}

- (void)toggleBreakpointAtCursor:(id)sender
{
    (void)sender;
    NSInteger line = [_codeEditor currentLineNumber];
    if (line > 0) [_codeEditor toggleBreakpointAtLine:line];
}

- (void)clearAllBreakpoints:(id)sender
{
    (void)sender;
    VBGmbSourceWindowController *source = [self activeGmbSourceWindow];
    if (source) { [source clearAllBreakpoints]; return; }
    [_codeEditor setBreakpointLines:nil];
    VbForm *form = (_currentItem && _currentItem->form) ? _currentItem->form : NULL;
    if (form) {
        vb_form_clear_breakpoints(form);
        form->dirty = 1;
        _projectDirty = YES;
        [self updateTitle];
    }
}

- (VBGmbSourceWindowController *)openGmbSourceAtPath:(NSString *)path
{
    if (!path.length) return nil;
    if (!gGmbSourceWindows) gGmbSourceWindows = [NSMutableArray array];
    for (VBGmbSourceWindowController *existing in gGmbSourceWindows) {
        if ([existing.path isEqualToString:path]) {
            [existing.window makeKeyAndOrderFront:nil];
            return existing;
        }
    }
    VBGmbSourceWindowController *controller = [[VBGmbSourceWindowController alloc] initWithPath:path];
    [gGmbSourceWindows addObject:controller];
    [controller showWindow:nil];
    [controller.window makeKeyAndOrderFront:nil];
    return controller;
}

- (void)gmbOpenProjectFile:(id)sender
{
    (void)sender;
    NSString *project = [VBGmbRunner projectPath];
    if (!project.length) {
        NSBeep();
        return;
    }
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = NO;
    panel.directoryURL = [NSURL fileURLWithPath:project isDirectory:YES];
    panel.message = @"Select a source file in the GMBasic project";
    [panel beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse response) {
        if (response == NSModalResponseOK) [self openGmbSourceAtPath:panel.URL.path];
    }];
}

- (NSUInteger)exportAllFormsToProject:(NSString *)project error:(NSString **)error
{
    if (!project.length || !_project) {
        if (error) *error = @"Open a GMBasic project and a VB6 project first.";
        return 0;
    }
    NSUInteger exported = 0, failed = 0;
    for (int i = 0; i < _project->nitems; i++) {
        VbItem *item = &_project->items[i];
        if (item->kind != VB_ITEM_FORM || !item->form) continue;
        NSString *name = [NSString stringWithUTF8String:vb_form_name(item->form)];
        NSString *path = [project stringByAppendingPathComponent:[name stringByAppendingString:@".ui.bas"]];
        if (![self exportForm:item->form toPath:path]) { failed++; continue; }
        NSString *manifestError = nil;
        if (![VBGmbRunner addSource:path form:name toProject:project error:&manifestError]) failed++;
        else {
            [VBGmbRunner addForm:path name:name toProject:project error:nil];
            exported++;
        }
    }
    [self refreshGmbProjectOutline];
    if (failed && error) *error = [NSString stringWithFormat:@"%lu forms failed", (unsigned long)failed];
    return exported;
}

- (void)gmbExportAllForms:(id)sender
{
    (void)sender;
    NSString *project = [VBGmbRunner projectPath];
    NSString *error = nil;
    NSUInteger exported = [self exportAllFormsToProject:project error:&error];
    [VBGmbRunner showOutput:[NSString stringWithFormat:@"Exported: %lu\n%@",
                             (unsigned long)exported, error ?: @""]
                      title:@"GMBasic forms"];
}

- (void)gmbRunForm:(id)sender
{
    (void)sender;
    VbForm *form = (_currentItem && _currentItem->form) ? _currentItem->form : NULL;
    if (!form) {
        [VBGmbRunner showOutput:@"Select a form before running.\n" title:@"GMBasic form"];
        return;
    }
    NSString *tmp = [NSTemporaryDirectory() stringByAppendingPathComponent:
                     [NSString stringWithFormat:@"gmbide-run-form-%d-%u", getpid(), arc4random()]];
    [[NSFileManager defaultManager] createDirectoryAtPath:tmp withIntermediateDirectories:YES attributes:nil error:nil];
    NSString *name = [NSString stringWithUTF8String:vb_form_name(form)];
    NSString *path = [tmp stringByAppendingPathComponent:[name stringByAppendingString:@".ui.bas"]];
    if (![self exportCurrentFormToPath:path]) {
        [[NSFileManager defaultManager] removeItemAtPath:tmp error:nil];
        [VBGmbRunner showOutput:@"Form export failed.\n" title:@"GMBasic form"];
        return;
    }
    NSString *wrapper = [NSString stringWithFormat:@"\nSUB Main()\n  %s_Show\nEND SUB\n", vb_form_name(form)];
    NSFileHandle *handle = [NSFileHandle fileHandleForWritingAtPath:path];
    [handle seekToEndOfFile];
    [handle writeData:[wrapper dataUsingEncoding:NSUTF8StringEncoding]];
    [handle closeFile];
    [VBGmbRunner runCommand:@"run" project:tmp entry:path completion:^(int status, NSString *output) {
        [VBGmbRunner showOutput:[NSString stringWithFormat:@"RUN FORM (exit %d)\n\n%@", status, output]
                          title:@"GMBasic form"];
        [[NSFileManager defaultManager] removeItemAtPath:tmp error:nil];
    }];
}

- (void)gmbExportForm:(id)sender
{
    (void)sender;
    VbForm *form = (_currentItem && _currentItem->form) ? _currentItem->form : NULL;
    if (!form) {
        [VBGmbRunner showOutput:@"Select a form before exporting.\n" title:@"GMBasic form"];
        return;
    }
    NSString *name = [NSString stringWithUTF8String:vb_form_name(form)];
    NSSavePanel *panel = [NSSavePanel savePanel];
    panel.nameFieldStringValue = [name stringByAppendingString:@".ui.bas"];
    NSString *project = [VBGmbRunner projectPath];
    if (project.length) panel.directoryURL = [NSURL fileURLWithPath:project isDirectory:YES];
    panel.allowedContentTypes = @[UTTypePlainText];
    panel.message = @"Export a GMBasic .ui.bas form module";
    [panel beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse response) {
        if (response != NSModalResponseOK) return;
        BOOL ok = [self exportCurrentFormToPath:panel.URL.path];
        NSString *project = [VBGmbRunner projectPath];
        if (ok && project.length && [panel.URL.path hasPrefix:[project stringByAppendingString:@"/"]]) {
            NSString *manifestError = nil;
            [VBGmbRunner addSource:panel.URL.path form:name toProject:project error:&manifestError];
            if (!manifestError) [VBGmbRunner addForm:panel.URL.path name:name toProject:project error:nil];
            if (manifestError) ok = NO;
            [self refreshGmbProjectOutline];
        }
        [VBGmbRunner showOutput:ok ? [NSString stringWithFormat:@"Exported: %@\n", panel.URL.path]
                                 : @"Export failed.\n"
                          title:@"GMBasic form"];
    }];
}

- (void)gmbOpenSource:(id)sender
{
    (void)sender;
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = NO;
    panel.message = @"Select a GMBasic .bas source";
    [panel beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse response) {
        if (response != NSModalResponseOK) return;
        [self openGmbSourceAtPath:panel.URL.path];
    }];
}


static void VBSetNeedsDisplayTree(NSView *view)
{
    if (!view) return;
    [view setNeedsDisplay:YES];
    for (NSView *child in view.subviews) VBSetNeedsDisplayTree(child);
}

- (BOOL)exportForm:(VbForm *)form toPath:(NSString *)path
{
    if (!form || !path.length) return NO;
    char *code = vb_gmb_form_export(form);
    if (!code) return NO;
    BOOL ok = vb_write_file(path.UTF8String, code, strlen(code)) != 0;
    free(code);
    return ok;
}

- (BOOL)exportCurrentFormToPath:(NSString *)path
{
    VbForm *form = (_currentItem && _currentItem->form) ? _currentItem->form : NULL;
    return [self exportForm:form toPath:path];
}

- (void)refreshGmbProjectOutline
{
    NSString *project = [VBGmbRunner projectPath];
    if (!project.length) return;
    NSDictionary *manifest = [VBGmbRunner manifestForProject:project];
    NSMutableArray<NSString *> *files = [NSMutableArray array];
    NSArray *sources = manifest[@"sources"];
    if ([sources isKindOfClass:NSArray.class]) {
        for (id item in sources) {
            if (![item isKindOfClass:NSString.class]) continue;
            NSString *path = [item isAbsolutePath] ? item : [project stringByAppendingPathComponent:item];
            if ([[NSFileManager defaultManager] fileExistsAtPath:path] && ![files containsObject:path])
                [files addObject:path];
        }
    }
    NSArray *forms = manifest[@"forms"];
    if ([forms isKindOfClass:NSArray.class]) {
        for (NSDictionary *form in forms) {
            NSString *relative = [form isKindOfClass:NSDictionary.class] ? form[@"file"] : nil;
            if (![relative isKindOfClass:NSString.class]) continue;
            NSString *path = [relative isAbsolutePath] ? relative : [project stringByAppendingPathComponent:relative];
            if ([[NSFileManager defaultManager] fileExistsAtPath:path] && ![files containsObject:path])
                [files addObject:path];
        }
    }
    if (!files.count) {
        for (NSString *name in [[NSFileManager defaultManager] contentsOfDirectoryAtPath:project error:nil]) {
            NSString *ext = name.pathExtension.lowercaseString;
            if (![ext isEqualToString:@"bas"] && ![ext isEqualToString:@"inc"]) continue;
            NSString *path = [project stringByAppendingPathComponent:name];
            if (![files containsObject:path]) [files addObject:path];
        }
        [files sortUsingSelector:@selector(localizedCaseInsensitiveCompare:)];
    }
    NSString *name = [manifest[@"name"] isKindOfClass:NSString.class] ? manifest[@"name"] : project.lastPathComponent;
    [_outline setGmbFiles:files title:name];
}

- (void)projectOutline:(VBProjectOutlineView *)outline didActivateGmbFile:(NSString *)path
{
    (void)outline;
    if ([path.pathExtension.lowercaseString isEqualToString:@"frm"]) [self openFormAtPath:path];
    else [self openGmbSourceAtPath:path];
}

- (void)refreshProjectTitle
{
    NSString *project = [VBGmbRunner projectPath];
    if (!project.length) {
        self.window.title = @"GMBasicIDE";
        return;
    }
    NSDictionary *manifest = [VBGmbRunner manifestForProject:project];
    NSString *name = [manifest[@"name"] isKindOfClass:NSString.class] ? manifest[@"name"] : project.lastPathComponent;
    self.window.title = [NSString stringWithFormat:@"GMBasicIDE - %@", name];
}

- (void)refreshTheme
{
    self.window.backgroundColor = VBColorFace();
    self.window.contentView.layer.backgroundColor = [VBColorFace() CGColor];
    _statusBar.layer.backgroundColor = [VBColorFace() CGColor];
    _status.textColor = VBColorText();
    _statusMode.textColor = VBColorText();
    _statusPos.textColor = VBColorText();
    _toolScroll.backgroundColor = VBColorFace();
    _designerScroll.backgroundColor = VBColorWorkspace();
    [_properties refreshTheme];
    [_codeEditor refreshTheme];
    VBSetNeedsDisplayTree(self.window.contentView);
    for (NSWindow *window in NSApp.windows) VBSetNeedsDisplayTree(window.contentView);
    for (VBGmbSourceWindowController *source in gGmbSourceWindows) [source refreshTheme];
}

- (void)gmbSelectTheme:(NSMenuItem *)item
{
    NSString *name = [item.representedObject isKindOfClass:NSString.class] ? item.representedObject : @"vb6";
    [VBGmbRunner setThemeName:name];
    for (NSMenuItem *entry in item.menu.itemArray)
        if (entry.action == @selector(gmbSelectTheme:)) entry.state = NSControlStateValueOff;
    item.state = NSControlStateValueOn;
    [VBGmbRunner loadThemeNamed:name completion:^(NSDictionary *json) {
        if (!json) {
            [VBGmbRunner showOutput:[NSString stringWithFormat:@"Theme %@ non disponibile (gmb_theme assente).\n", name]
                              title:@"GMBasic theme"];
            return;
        }
        VBThemeApplyJSON(json);
        [self refreshTheme];
    }];
}

- (void)gmbCheckProject:(id)sender
{

    (void)sender;
    [self gmbChooseProjectForCommand:@"check"];
}

- (void)gmbBuildProject:(id)sender
{
    (void)sender;
    [self gmbChooseProjectForCommand:@"build"];
}

- (void)gmbRunProject:(id)sender
{
    (void)sender;
    [self gmbChooseProjectForCommand:@"run"];
}

- (void)gmbDebugProject:(id)sender
{
    (void)sender;
    NSString *project = [VBGmbRunner projectPath];
    NSString *entry = project.length ? [VBGmbRunner entryForProject:project] : nil;
    if (!entry.length) {
        [VBGmbRunner showOutput:@"Open a GMBasic project with an entry file first.\n"
                          title:@"GMBasic debug"];
        return;
    }
    VBGmbSourceWindowController *source = [self openGmbSourceAtPath:entry];
    [source startDebug];
}

- (void)gmbTerminal:(id)sender
{
    (void)sender;
    if (!_terminals) _terminals = [NSMutableArray array];
    NSString *directory = [VBGmbRunner projectPath] ?: [[NSFileManager defaultManager] currentDirectoryPath];
    VBTerminalWindow *terminal = [[VBTerminalWindow alloc] initWithWorkingDirectory:directory];
    [_terminals addObject:terminal];
    [terminal showWindow:nil];
    [terminal.window makeKeyAndOrderFront:nil];
}

- (void)gmbSetToolchain:(id)sender
{
    (void)sender;
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = NO;
    panel.message = @"Select the GMBasic gmb executable";
    [panel beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse response) {
        if (response == NSModalResponseOK) {
            [VBGmbRunner setToolPath:panel.URL.path];
            [VBGmbRunner showOutput:[NSString stringWithFormat:@"Toolchain: %@\n", panel.URL.path]
                              title:@"GMBasic toolchain"];
        }
    }];
}

- (void)gmbNewProject:(id)sender
{
    (void)sender;
    NSSavePanel *panel = [NSSavePanel savePanel];
    panel.nameFieldStringValue = @"NewProject";
    panel.canCreateDirectories = YES;
    panel.message = @"Create a new GMBasic project";
    [panel beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse response) {
        if (response != NSModalResponseOK) return;
        NSString *path = panel.URL.path;
        NSString *error = nil;
        if (![VBGmbRunner createProjectAtPath:path name:path.lastPathComponent error:&error]) {
            [VBGmbRunner showOutput:[NSString stringWithFormat:@"%@\n", error ?: @"Cannot create project."]
                              title:@"GMBasic project"];
            return;
        }
        [VBGmbRunner setProjectPath:path];
        [self refreshProjectTitle];
        [self openGmbSourceAtPath:[VBGmbRunner entryForProject:path]];
    }];
}

- (void)gmbOpenProject:(id)sender
{
    (void)sender;
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = NO;
    panel.canChooseDirectories = YES;
    panel.allowsMultipleSelection = NO;
    panel.message = @"Select a GMBasic project folder";
    [panel beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse response) {
        if (response != NSModalResponseOK) return;
        NSString *project = panel.URL.path;
        if (![VBGmbRunner entryForProject:project].length) {
            [VBGmbRunner showOutput:@"No entry .bas found in the selected folder.\n" title:@"GMBasic"];
            return;
        }
        NSDictionary *manifest = [VBGmbRunner manifestForProject:project];
        NSString *name = [manifest[@"name"] isKindOfClass:NSString.class] ? manifest[@"name"] : project.lastPathComponent;
        NSString *backend = [manifest[@"backend"] isKindOfClass:NSString.class] ? manifest[@"backend"] : @"default";
        [VBGmbRunner setProjectPath:project];
        [self refreshProjectTitle];
        NSString *entry = [VBGmbRunner entryForProject:project];
        [self openGmbSourceAtPath:entry];
        [VBGmbRunner showOutput:[NSString stringWithFormat:@"Project: %@\nName: %@\nBackend: %@\nEntry: %@\n",
                                 project, name, backend, entry.lastPathComponent]
                          title:@"GMBasic project"];
    }];
}

- (void)gmbRunProjectCommand:(NSString *)command project:(NSString *)project
{
    NSString *entry = [VBGmbRunner entryForProject:project];
    if (!entry.length) {
        [VBGmbRunner showOutput:@"No entry .bas found in the selected folder.\n" title:@"GMBasic"];
        return;
    }
    [VBGmbRunner setProjectPath:project];
    [VBGmbRunner showOutput:[NSString stringWithFormat:@"%@ %@\n", command, project] title:@"GMBasic"];
    [VBGmbRunner runCommand:command project:project entry:entry completion:^(int status, NSString *output) {
        [VBGmbRunner showOutput:[NSString stringWithFormat:@"%@ (exit %d)\n\n%@",
                                 command.uppercaseString, status, output]
                          title:@"GMBasic"];
    }];
}

- (void)gmbChooseProjectForCommand:(NSString *)command
{
    NSString *stored = [VBGmbRunner projectPath];
    if (stored.length && [VBGmbRunner entryForProject:stored].length) {
        [self gmbRunProjectCommand:command project:stored];
        return;
    }
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = NO;
    panel.canChooseDirectories = YES;
    panel.allowsMultipleSelection = NO;
    panel.message = @"Select a GMBasic project folder";
    [panel beginSheetModalForWindow:self.window completionHandler:^(NSModalResponse response) {
        if (response != NSModalResponseOK) return;
        [self gmbRunProjectCommand:command project:panel.URL.path];
    }];
}

/* Debug > Step Into / F8 while paused; Run > Continue resumes (F5 while paused). */
- (void)stepInto
{
    if (!_runtime || !_runtime.isInBreakMode) {
        NSBeep();
        return;
    }
    [_runtime breakStep];
}

- (void)stepOver
{
    if (!_runtime || !_runtime.isInBreakMode) {
        NSBeep();
        return;
    }
    [_runtime breakStep];
}

- (void)continueRun
{
    if (!_runtime || !_runtime.isInBreakMode) {
        NSBeep();
        return;
    }
    [_runtime breakContinue];
}

#pragma mark - Modes

- (void)showDesigner
{
    if (!_currentItem || _currentItem->kind != VB_ITEM_FORM || !_currentItem->form) {
        [self showCode];
        return;
    }
    _showCode = NO;
    _designerWindow.hidden = NO;
    _codeWindow.hidden = YES;
    _designerWindow.active = YES;
    _codeWindow.active = NO;
    [_designer refresh];
    [self.window makeFirstResponder:_designer];
    [self updateTitle];
}

- (void)showCode
{
    _showCode = YES;
    _designerWindow.hidden = YES;
    _codeWindow.hidden = NO;
    _designerWindow.active = NO;
    _codeWindow.active = YES;
    [_codeEditor focusEditor];
    [self updateTitle];
}

#pragma mark - Project lifecycle

- (BOOL)loadProjectAtPath:(NSString *)path
{
    if (!path.length) return NO;
    VbProject *project = vb_project_load(path.UTF8String);
    if (!project) {
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = @"Could not open project";
        alert.informativeText = [NSString stringWithFormat:@"GMBasicIDE could not read %@. Check that the .vbp file and its forms are present.", path];
        [alert addButtonWithTitle:@"OK"];
        [alert runModal];
        return NO;
    }
    [self adoptProject:project];
    return YES;
}

- (void)adoptProject:(VbProject *)project
{
    if (_runtime) [self stopProject];
    if (_project) vb_project_free(_project);
    _project = project;
    _currentItem = NULL;
    _projectDirty = NO;
    _showCode = NO;
    [_moduleCodes removeAllObjects];
    [_dirtyModules removeAllObjects];

    [_outline setProject:_project];
    [_properties setControl:NULL];
    [_designer setForm:NULL];
    [_codeEditor setCode:@""];
    [_toolbox clearSelection];
    [self updateTitle];
    [self updateStatus];
    [self updateToolbarState];

    if (!_project) return;
    VbItem *startup = _project->startup ? vb_project_find_item(_project, _project->startup) : NULL;
    if (!startup || startup->kind != VB_ITEM_FORM) {
        startup = NULL;
        for (int i = 0; i < _project->nitems; i++) {
            if (_project->items[i].kind == VB_ITEM_FORM) { startup = &_project->items[i]; break; }
        }
    }
    if (startup) {
        [self selectItem:startup];
        [_outline selectItem:startup];
    } else {
        [self showCode];
    }
}

- (void)newProject
{
    VbProject *project = vb_project_new();
    if (!project) return;
    project->name = vb_strdup("Project1");
    project->startup = vb_strdup("Form1");
    VbItem *item = vb_project_add_item(project, VB_ITEM_FORM, "Form1", NULL);
    if (item) {
        snprintf(item->kind_key, sizeof(item->kind_key), "%s", "Form");
        item->form = vb_form_new("Form1");
        if (item->form) {
            vb_control_set(item->form->root, "Caption", "\"Form1\"");
        }
    }
    [self adoptProject:project];
    _projectDirty = YES;
    [self updateTitle];
    [self updateStatus];
    if (item) {
        [self selectItem:item];
        [_outline selectItem:item];
    }
}

- (void)showProjectExplorer:(id)sender
{
    (void)sender;
    [self.window makeFirstResponder:_outline];
}

- (void)toggleStandardToolbar:(NSMenuItem *)sender
{
    _toolbar.hidden = !_toolbar.hidden;
    sender.state = _toolbar.hidden ? NSControlStateValueOff : NSControlStateValueOn;
}

- (void)toggleFormatToolbar:(NSMenuItem *)sender
{
    _formatBar.hidden = !_formatBar.hidden;
    sender.state = _formatBar.hidden ? NSControlStateValueOff : NSControlStateValueOn;
}

- (void)showFormLayout:(id)sender
{
    (void)sender;
    if (!_formLayoutWindow) {
        _formLayoutView = [[VBFormLayoutView alloc] initWithFrame:NSMakeRect(0, 0, 260, 200)];
        _formLayoutWindow = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 260, 200)
                                                        styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                                          backing:NSBackingStoreBuffered
                                                            defer:NO];
        _formLayoutWindow.title = @"Form Layout";
        _formLayoutWindow.releasedWhenClosed = NO;
        _formLayoutWindow.contentView = _formLayoutView;
        [_formLayoutWindow center];
    }
    [_formLayoutView setForm:(_currentItem && _currentItem->form) ? _currentItem->form : NULL];
    [_formLayoutWindow makeKeyAndOrderFront:nil];
}

- (void)indentSelection:(id)sender
{
    (void)sender;
    [_codeEditor indentSelection];
}

- (void)outdentSelection:(id)sender
{
    (void)sender;
    [_codeEditor outdentSelection];
}

- (void)toggleBreakpointAtCurrentLine:(id)sender
{
    (void)sender;
    VBGmbSourceWindowController *source = [self activeGmbSourceWindow];
    if (source) { [source toggleCurrentBreakpoint]; return; }
    if (!_currentItem || _showCode == NO) [self showCode];
    NSInteger line = [_codeEditor currentLineNumber];
    if (line > 0) [_codeEditor toggleBreakpointAtLine:line];
}

- (void)addProcedure:(id)sender
{
    (void)sender;
    if (!_currentItem) {
        NSBeep();
        return;
    }
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"Add Procedure";
    alert.informativeText = @"Aggiunge una procedura alla fine del modulo corrente.";
    [alert addButtonWithTitle:@"OK"];
    [alert addButtonWithTitle:@"Cancel"];

    NSView *accessory = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 340, 96)];
    NSTextField *nameField = [[NSTextField alloc] initWithFrame:NSMakeRect(100, 66, 230, 24)];
    NSPopUpButton *typePopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(100, 38, 150, 24) pullsDown:NO];
    [typePopup addItemsWithTitles:@[@"Sub", @"Function", @"Property", @"Event"]];
    NSPopUpButton *scopePopup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(100, 10, 150, 24) pullsDown:NO];
    [scopePopup addItemsWithTitles:@[@"Public", @"Private"]];
    NSTextField *nameLabel = [NSTextField labelWithString:@"Name:"];
    nameLabel.frame = NSMakeRect(8, 70, 88, 18);
    NSTextField *typeLabel = [NSTextField labelWithString:@"Type:"];
    typeLabel.frame = NSMakeRect(8, 42, 88, 18);
    NSTextField *scopeLabel = [NSTextField labelWithString:@"Scope:"];
    scopeLabel.frame = NSMakeRect(8, 14, 88, 18);
    [accessory addSubview:nameLabel];
    [accessory addSubview:nameField];
    [accessory addSubview:typeLabel];
    [accessory addSubview:typePopup];
    [accessory addSubview:scopeLabel];
    [accessory addSubview:scopePopup];
    alert.accessoryView = accessory;
    if ([alert runModal] != NSAlertFirstButtonReturn) return;

    NSString *name = [nameField.stringValue stringByTrimmingCharactersInSet:
                      [NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (!name.length) {
        NSBeep();
        return;
    }
    NSString *scope = scopePopup.titleOfSelectedItem ?: @"Public";
    NSInteger type = typePopup.indexOfSelectedItem;
    NSString *code = nil;
    if (type == 3) {
        code = [NSString stringWithFormat:@"%@ Event %@()\n", scope, name];
    } else if (type == 2) {
        code = [NSString stringWithFormat:@"%@ Property Get %@() As Variant\n\nEnd Property\n\n%@ Property Let %@(ByVal vNewValue As Variant)\n\nEnd Property\n", scope, name, scope, name];
    } else {
        NSString *kind = type == 1 ? @"Function" : @"Sub";
        code = [NSString stringWithFormat:@"%@ %@ %@()\n\nEnd %@\n", scope, kind, name, kind];
    }
    [self showCode];
    [_codeEditor appendCode:code];
}

- (void)removeCurrentForm:(id)sender
{
    (void)sender;
    if (!_currentItem || _currentItem->kind != VB_ITEM_FORM || !_project) {
        NSBeep();
        return;
    }
    NSString *name = _currentItem->name ? [NSString stringWithUTF8String:_currentItem->name] : @"";
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"Remove Form";
    alert.informativeText = [NSString stringWithFormat:@"Rimuovere %@ dal progetto? Il file .frm non verrà cancellato.", name];
    [alert addButtonWithTitle:@"Remove"];
    [alert addButtonWithTitle:@"Cancel"];
    if ([alert runModal] != NSAlertFirstButtonReturn) return;

    NSString *project = [VBGmbRunner projectPath];
    if (project.length) {
        NSString *error = nil;
        if (![VBGmbRunner removeFormNamed:name fromProject:project error:&error]) {
            [VBGmbRunner showOutput:error ?: @"Cannot remove form.\n" title:@"Project"];
            return;
        }
    }

    int index = (int)(_currentItem - _project->items);
    VbItem *item = _currentItem;
    if (item->form) vb_form_free(item->form);
    free(item->name);
    free(item->file);
    if (index >= 0 && index < _project->nitems - 1)
        memmove(&_project->items[index], &_project->items[index + 1],
                (size_t)(_project->nitems - index - 1) * sizeof(VbItem));
    _project->nitems--;
    _projectDirty = YES;
    [_outline setProject:_project];
    [_properties setControl:NULL];
    [_designer setForm:NULL];
    [_codeEditor setCode:@""];
    _currentItem = NULL;
    if (_project->nitems > 0) {
        [self selectItem:&_project->items[0]];
        [_outline selectItem:&_project->items[0]];
    } else {
        [self showCode];
    }
    [self updateTitle];
    [self updateStatus];
}

- (void)showReferences:(id)sender
{
    (void)sender;
    NSMutableString *text = [NSMutableString stringWithString:@"GMBasicIDE\n  Automatic reference\n\n"];
    if (_project) {
        for (int i = 0; i < _project->nitems; i++) {
            VbItem *item = &_project->items[i];
            [text appendFormat:@"%@  %s\n", item->kind == VB_ITEM_FORM ? @"Form" :
                                               item->kind == VB_ITEM_MODULE ? @"Module" :
                                               item->kind == VB_ITEM_CLASS ? @"Class" : @"Item",
                                   item->name ? item->name : "?"];
        }
    }
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"References";
    alert.informativeText = text;
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
}

- (void)showComponents:(id)sender
{
    (void)sender;
    NSMutableString *text = [NSMutableString stringWithString:@"Available controls:\n\n"];
    int count = 0;
    const VbToolDef *tools = vb_tool_defs(&count);
    for (int i = 0; i < count; i++) {
        NSString *type = [NSString stringWithUTF8String:tools[i].type];
        [text appendFormat:@"%@\n", [type stringByReplacingOccurrencesOfString:@"VB." withString:@""]];
    }
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"Components";
    alert.informativeText = text;
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
}

- (void)toggleBookmark:(id)sender
{
    (void)sender;
    [self showCode];
    [_codeEditor toggleBookmarkAtLine:[_codeEditor currentLineNumber]];
}

- (void)nextBookmark:(id)sender
{
    (void)sender;
    [self showCode];
    [_codeEditor nextBookmark];
}

- (void)previousBookmark:(id)sender
{
    (void)sender;
    [self showCode];
    [_codeEditor previousBookmark];
}

- (void)clearBookmarks:(id)sender
{
    (void)sender;
    [_codeEditor clearBookmarks];
}

- (void)jumpToDefinition:(id)sender
{
    (void)sender;
    [self showCode];
    [_codeEditor jumpToDefinition];
}

- (void)jumpToLastPosition:(id)sender
{
    (void)sender;
    [self showCode];
    [_codeEditor jumpToLastPosition];
}

- (VBGmbSourceWindowController *)activeGmbSourceWindow
{
    for (VBGmbSourceWindowController *source in gGmbSourceWindows) {
        if (source.window.isKeyWindow) return source;
    }
    return gGmbSourceWindows.lastObject;
}

- (void)debugStepInto:(id)sender
{
    (void)sender;
    [[self activeGmbSourceWindow] stepDebug];
}

- (void)debugStepOver:(id)sender
{
    (void)sender;
    [[self activeGmbSourceWindow] stepDebug];
}

- (void)debugStepOut:(id)sender
{
    (void)sender;
    [[self activeGmbSourceWindow] stepDebug];
}

- (void)debugStop:(id)sender
{
    (void)sender;
    [[self activeGmbSourceWindow] stopDebug];
}

- (void)debugRestart:(id)sender
{
    (void)sender;
    VBGmbSourceWindowController *source = [self activeGmbSourceWindow];
    [source stopDebug];
    [source startDebug];
}

- (void)showObjectBrowser:(id)sender
{
    (void)sender;
    if (!_objectBrowserWindow) {
        _objectBrowserWindow = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 480, 360)
                                                           styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable
                                                             backing:NSBackingStoreBuffered
                                                               defer:NO];
        _objectBrowserWindow.title = @"Object Browser";
        _objectBrowserWindow.releasedWhenClosed = NO;
        NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, 480, 360)];
        scroll.hasVerticalScroller = YES;
        scroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        _objectBrowserText = [[NSTextView alloc] initWithFrame:scroll.bounds];
        _objectBrowserText.editable = NO;
        _objectBrowserText.selectable = YES;
        _objectBrowserText.font = [NSFont monospacedSystemFontOfSize:12 weight:NSFontWeightRegular];
        _objectBrowserText.autoresizingMask = NSViewWidthSizable;
        scroll.documentView = _objectBrowserText;
        _objectBrowserWindow.contentView = scroll;
        [_objectBrowserWindow center];
    }

    NSMutableString *text = [NSMutableString string];
    if (_project) {
        [text appendFormat:@"Project: %s\n\n", _project->name ? _project->name : "Project"];
        for (int i = 0; i < _project->nitems; i++) {
            VbItem *item = &_project->items[i];
            const char *kind = item->kind == VB_ITEM_FORM ? "Form" :
                               item->kind == VB_ITEM_MODULE ? "Module" :
                               item->kind == VB_ITEM_CLASS ? "Class" :
                               item->kind == VB_ITEM_USERCONTROL ? "UserControl" : "Item";
            [text appendFormat:@"%s  %s\n", kind, item->name ? item->name : "?"];
            const char *code = NULL;
            if (item->kind == VB_ITEM_FORM && item->form) code = item->form->code;
            else {
                NSString *module = _moduleCodes[[NSValue valueWithPointer:item]];
                code = module.UTF8String;
            }
            VbHandlerList handlers;
            vb_handler_list_init(&handlers);
            vb_code_handlers(code ? code : "", &handlers);
            for (int h = 0; h < handlers.count; h++) {
                [text appendFormat:@"    %s\n", handlers.items[h].name ? handlers.items[h].name : "?"];
            }
            vb_handler_list_free(&handlers);
            [text appendString:@"\n"];
        }
    } else {
        [text appendString:@"No project loaded.\n"];
    }
    _objectBrowserText.string = text;
    [_objectBrowserWindow makeKeyAndOrderFront:nil];
}

- (void)showFindPanel:(id)sender
{
    (void)sender;
    [self showCode];
    [_codeEditor showFindPanel];
}

- (void)findNext:(id)sender
{
    (void)sender;
    [self showCode];
    [_codeEditor findNext];
}

- (void)findPrevious:(id)sender
{
    (void)sender;
    [self showCode];
    [_codeEditor findPrevious];
}

- (void)insertSourceFile:(id)sender
{
    (void)sender;
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = NO;
    if ([panel runModal] != NSModalResponseOK || !panel.URL) return;
    NSString *text = [NSString stringWithContentsOfURL:panel.URL encoding:NSUTF8StringEncoding error:nil];
    if (!text) {
        [VBGmbRunner showOutput:@"Cannot read the selected file.\n" title:@"Insert File"];
        return;
    }
    [self showCode];
    [_codeEditor insertTextAtCaret:text];
}

- (void)showColorPaletteFore:(id)sender
{
    (void)sender;
    [self openColorPaletteBack:NO];
}

- (void)showColorPaletteBackColor:(id)sender
{
    (void)sender;
    [self openColorPaletteBack:YES];
}

- (void)openColorPaletteBack:(BOOL)back
{
    _paletteBack = back;
    if (!_colorPaletteWindow) {
        VBColorPaletteView *view = [[VBColorPaletteView alloc] initWithFrame:NSMakeRect(0, 0, 152, 152)];
        view.target = self;
        view.action = @selector(paletteColorPicked:);
        _colorPaletteWindow = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 152, 152)
                                                         styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                                                           backing:NSBackingStoreBuffered
                                                             defer:NO];
        _colorPaletteWindow.releasedWhenClosed = NO;
        _colorPaletteWindow.contentView = view;
        [_colorPaletteWindow center];
    }
    VBColorPaletteView *view = (VBColorPaletteView *)_colorPaletteWindow.contentView;
    view.pickingBackColor = back;
    _colorPaletteWindow.title = back ? @"Color Palette — BackColor" : @"Color Palette — ForeColor";
    [_colorPaletteWindow makeKeyAndOrderFront:nil];
}

- (void)paletteColorPicked:(NSColor *)color
{
    VbControl *control = _properties.control;
    if (!control && _currentItem && _currentItem->form) control = _currentItem->form->root;
    if (!control || !color) return;
    NSColor *rgb = [color colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
    if (!rgb) return;
    int r = (int)lround(rgb.redComponent * 255.0);
    int g = (int)lround(rgb.greenComponent * 255.0);
    int b = (int)lround(rgb.blueComponent * 255.0);
    NSString *value = [NSString stringWithFormat:@"&H%02X%02X%02X&", b, g, r];
    vb_control_set_display(control, _paletteBack ? "BackColor" : "ForeColor", value.UTF8String);
    if (_currentItem && _currentItem->form) _currentItem->form->dirty = 1;
    _projectDirty = YES;
    [_designer refresh];
    [_properties reload];
    [self updateTitle];
}

- (void)propertiesAlphabetical:(id)sender
{
    (void)sender;
    [_properties setCategorized:NO];
    [self showPropertiesWindow:nil];
}

- (void)propertiesCategorized:(id)sender
{
    (void)sender;
    [_properties setCategorized:YES];
    [self showPropertiesWindow:nil];
}

- (void)showPropertiesWindow:(id)sender
{
    (void)sender;
    [self.window makeFirstResponder:_properties];
}

- (void)showToolbox:(id)sender
{
    (void)sender;
    [self.window makeFirstResponder:_toolbox];
}

- (VbItem *)addCodeItemKind:(VbItemKind)kind prefix:(NSString *)prefix source:(NSString *)source
{
    if (!_project) [self newProject];
    int index = 1;
    char name[64];
    do {
        snprintf(name, sizeof(name), "%s%d", prefix.UTF8String, index++);
    } while (vb_project_find_item(_project, name) != NULL);
    VbItem *item = vb_project_add_item(_project, kind, name, NULL);
    if (!item) return NULL;
    snprintf(item->kind_key, sizeof(item->kind_key), "%s", kind == VB_ITEM_MODULE ? "Module" : "Class");
    _moduleCodes[[NSValue valueWithPointer:item]] = source ?: @"";
    _projectDirty = YES;
    [_outline setProject:_project];
    [self selectItem:item];
    [_outline selectItem:item];
    [self showCode];
    [self updateTitle];
    [self updateStatus];
    return item;
}

- (void)addModule
{
    NSString *source = [NSString stringWithFormat:@"Attribute VB_Name = \"Module1\"\n\n' Modulo standard\n"];
    VbItem *item = [self addCodeItemKind:VB_ITEM_MODULE prefix:@"Module" source:source];
    if (item) {
        NSString *named = [source stringByReplacingOccurrencesOfString:@"Module1"
                                                            withString:StringFromC(item->name)];
        _moduleCodes[[NSValue valueWithPointer:item]] = named;
        [_codeEditor setCode:named];
    }
}

- (void)addClassModule
{
    NSString *source = [NSString stringWithFormat:@"VERSION 1.0 CLASS\nBEGIN\n  MultiUse = -1  'True\nEND\nAttribute VB_Name = \"Class1\"\nAttribute VB_GlobalNameSpace = False\nAttribute VB_Creatable = False\nAttribute VB_PredeclaredId = False\nAttribute VB_Exposed = False\n"];
    VbItem *item = [self addCodeItemKind:VB_ITEM_CLASS prefix:@"Class" source:source];
    if (item) {
        NSString *named = [source stringByReplacingOccurrencesOfString:@"Class1"
                                                            withString:StringFromC(item->name)];
        _moduleCodes[[NSValue valueWithPointer:item]] = named;
        [_codeEditor setCode:named];
    }
}

- (void)printCurrentView:(id)sender
{
    (void)sender;
    NSView *view = _showCode ? (NSView *)_codeEditor : (NSView *)_designer;
    if (!view) return;
    NSPrintInfo *info = [NSPrintInfo sharedPrintInfo];
    info.horizontalPagination = NSPrintingPaginationModeFit;
    info.verticalPagination = NSPrintingPaginationModeAutomatic;
    NSPrintOperation *operation = [NSPrintOperation printOperationWithView:view printInfo:info];
    operation.showsPrintPanel = YES;
    operation.showsProgressPanel = YES;
    [operation runOperation];
}

- (void)printSetup:(id)sender
{
    (void)sender;
    [[NSPageLayout pageLayout] runModal];
}

- (void)projectProperties:(id)sender
{
    (void)sender;
    NSString *path = [VBGmbRunner projectPath];
    if (!path.length) {
        NSBeep();
        return;
    }
    NSDictionary *manifest = [VBGmbRunner manifestForProject:path];
    NSString *name = [manifest[@"name"] isKindOfClass:NSString.class] ? manifest[@"name"] : path.lastPathComponent;
    NSString *entry = [manifest[@"entry"] isKindOfClass:NSString.class] ? manifest[@"entry"] : @"Main.bas";
    NSString *backend = [manifest[@"backend"] isKindOfClass:NSString.class] ? manifest[@"backend"] : @"c99native";

    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"Project Properties";
    alert.informativeText = @"Nome, entry point e backend del progetto GMBasic.";
    [alert addButtonWithTitle:@"OK"];
    [alert addButtonWithTitle:@"Cancel"];

    NSView *accessory = [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 360, 94)];
    NSTextField *nameField = [[NSTextField alloc] initWithFrame:NSMakeRect(100, 64, 250, 24)];
    nameField.stringValue = name;
    NSTextField *entryField = [[NSTextField alloc] initWithFrame:NSMakeRect(100, 34, 250, 24)];
    entryField.stringValue = entry;
    NSTextField *backendField = [[NSTextField alloc] initWithFrame:NSMakeRect(100, 4, 250, 24)];
    backendField.stringValue = backend;
    NSTextField *nameLabel = [NSTextField labelWithString:@"Name:"];
    nameLabel.frame = NSMakeRect(8, 68, 88, 18);
    NSTextField *entryLabel = [NSTextField labelWithString:@"Entry:"];
    entryLabel.frame = NSMakeRect(8, 38, 88, 18);
    NSTextField *backendLabel = [NSTextField labelWithString:@"Backend:"];
    backendLabel.frame = NSMakeRect(8, 8, 88, 18);
    [accessory addSubview:nameLabel];
    [accessory addSubview:nameField];
    [accessory addSubview:entryLabel];
    [accessory addSubview:entryField];
    [accessory addSubview:backendLabel];
    [accessory addSubview:backendField];
    alert.accessoryView = accessory;
    if ([alert runModal] != NSAlertFirstButtonReturn) return;

    NSString *newName = [nameField.stringValue stringByTrimmingCharactersInSet:
                         [NSCharacterSet whitespaceAndNewlineCharacterSet]];
    NSString *newEntry = [entryField.stringValue stringByTrimmingCharactersInSet:
                          [NSCharacterSet whitespaceAndNewlineCharacterSet]];
    NSString *newBackend = [backendField.stringValue stringByTrimmingCharactersInSet:
                            [NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (!newName.length) newName = name;
    if (!newEntry.length) newEntry = entry;
    if (!newBackend.length) newBackend = backend;
    NSString *error = nil;
    if (![VBGmbRunner updateProjectManifest:@{@"name": newName, @"entry": newEntry, @"backend": newBackend}
                                  project:path
                                    error:&error]) {
        [VBGmbRunner showOutput:error ?: @"Cannot update project properties.\n" title:@"Project"];
        return;
    }
    _projectDirty = YES;
    [VBGmbRunner showOutput:[NSString stringWithFormat:@"Project updated: %@\nEntry: %@\nBackend: %@\n",
                             newName, newEntry, newBackend] title:@"Project"];
}

- (void)showAbout:(id)sender
{
    (void)sender;
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"GMBasicIDE";
    NSString *version = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"CFBundleShortVersionString"] ?: @"1.0";
    alert.informativeText = [NSString stringWithFormat:
        @"Visual Basic 6 / GMBasic IDE\nVersion %@\n\nDesigner, editor, debugger e terminale integrato.", version];
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
}

- (void)gmbAddSourceWithDefaultName:(NSString *)defaultName source:(NSString *)source
{
    NSString *project = [VBGmbRunner projectPath];
    if (!project.length) {
        [VBGmbRunner showOutput:@"Open or create a project first.\n" title:@"Project"];
        return;
    }
    NSSavePanel *panel = [NSSavePanel savePanel];
    panel.title = @"Add Module";
    panel.allowedContentTypes = @[[UTType typeWithFilenameExtension:@"bas"]];
    panel.nameFieldStringValue = defaultName;
    panel.directoryURL = [NSURL fileURLWithPath:project isDirectory:YES];
    if ([panel runModal] != NSModalResponseOK || !panel.URL) return;
    if (![source writeToFile:panel.URL.path atomically:YES encoding:NSUTF8StringEncoding error:nil]) {
        [VBGmbRunner showOutput:@"Cannot write module.\n" title:@"Project"];
        return;
    }
    NSString *error = nil;
    if (![VBGmbRunner addSource:panel.URL.path form:nil toProject:project error:&error]) {
        [VBGmbRunner showOutput:error ?: @"Cannot update project.\n" title:@"Project"];
        return;
    }
    [self refreshGmbProjectOutline];
    [self openGmbSourceAtPath:panel.URL.path];
}

- (void)gmbAddFile
{
    NSString *project = [VBGmbRunner projectPath];
    if (!project.length) {
        [VBGmbRunner showOutput:@"Open or create a project first.\n" title:@"Project"];
        return;
    }
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = NO;
    panel.allowedContentTypes = @[[UTType typeWithFilenameExtension:@"bas"], [UTType typeWithFilenameExtension:@"inc"]];
    panel.directoryURL = [NSURL fileURLWithPath:project isDirectory:YES];
    if ([panel runModal] != NSModalResponseOK || !panel.URL) return;
    NSString *error = nil;
    if (![VBGmbRunner addSource:panel.URL.path form:nil toProject:project error:&error]) {
        [VBGmbRunner showOutput:error ?: @"Cannot add file.\n" title:@"Project"];
        return;
    }
    [self refreshGmbProjectOutline];
    [self openGmbSourceAtPath:panel.URL.path];
}

- (void)gmbAddModule
{
    [self gmbAddSourceWithDefaultName:@"Module1.bas" source:@"' Module\n"];
}

- (void)gmbAddClass
{
    [self gmbAddSourceWithDefaultName:@"Class1.bas"
                               source:@"PUBLIC CLASS Class1\nEND CLASS\n"];
}
- (void)gmbAddUserControl
{
    [self gmbAddSourceWithDefaultName:@"UserControl1.bas"
                               source:@"PUBLIC CLASS UserControl1 INHERITS Control\n  SUB Paint(g AS ANY)\n  END SUB\nEND CLASS\n"];
}


- (void)openFormAtPath:(NSString *)path
{
    VbForm *form = vb_form_load(path.UTF8String);
    if (!form) {
        [VBGmbRunner showOutput:[NSString stringWithFormat:@"Cannot load form: %@\n", path]
                          title:@"Form"];
        return;
    }
    if (_runtime) [self stopProject];
    VbProject *project = vb_project_new();
    if (!project) { vb_form_free(form); return; }
    const char *formName = vb_form_name(form);
    project->name = vb_strdup(formName ? formName : "Form");
    VbItem *item = vb_project_add_item(project, VB_ITEM_FORM,
                                       formName ? formName : "Form", path.UTF8String);
    if (item) {
        snprintf(item->kind_key, sizeof(item->kind_key), "%s", "Form");
        item->form = form;
        project->startup = vb_strdup(item->name);
    } else {
        vb_form_free(form);
    }
    [self adoptProject:project];
    if (item) {
        [self selectItem:item];
        [_outline selectItem:item];
        [self showDesigner];
    }
}

- (void)openFormDocument
{
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.title = @"Open Form";
    panel.allowedContentTypes = @[[UTType typeWithFilenameExtension:@"frm"]];
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    if ([panel runModal] == NSModalResponseOK && panel.URL) [self openFormAtPath:panel.URL.path];
}

- (void)newForm
{
    NSSavePanel *panel = [NSSavePanel savePanel];
    panel.title = @"New Form";
    panel.allowedContentTypes = @[[UTType typeWithFilenameExtension:@"frm"]];
    panel.nameFieldStringValue = @"Form1.frm";
    NSString *projectPath = [VBGmbRunner projectPath];
    if (projectPath.length) panel.directoryURL = [NSURL fileURLWithPath:projectPath isDirectory:YES];
    if ([panel runModal] != NSModalResponseOK || !panel.URL) return;
    VbForm *form = vb_form_new("Form1");
    if (!form) return;
    vb_control_set(form->root, "Caption", "\"Form1\"");
    if (!vb_form_save(form, panel.URL.path.UTF8String)) {
        vb_form_free(form);
        [VBGmbRunner showOutput:@"Cannot create form.\n" title:@"Form"];
        return;
    }
    free(form->path);
    form->path = vb_strdup(panel.URL.path.UTF8String);
    vb_form_free(form);
    NSString *project = [VBGmbRunner projectPath];
    if (project.length && [panel.URL.path hasPrefix:[project stringByAppendingString:@"/"]]) {
        NSString *error = nil;
        [VBGmbRunner addForm:panel.URL.path name:@"Form1" toProject:project error:&error];
    }
    [self openFormAtPath:panel.URL.path];
}

- (void)saveCurrentFormAs
{
    VbForm *form = (_currentItem && _currentItem->form) ? _currentItem->form : NULL;
    if (!form) { NSBeep(); return; }
    NSSavePanel *panel = [NSSavePanel savePanel];
    panel.allowedContentTypes = @[[UTType typeWithFilenameExtension:@"frm"]];
    panel.nameFieldStringValue = [NSString stringWithFormat:@"%s.frm", vb_form_name(form)];
    if ([panel runModal] != NSModalResponseOK || !panel.URL) return;
    if (!vb_form_save(form, panel.URL.path.UTF8String)) {
        [VBGmbRunner showOutput:@"Cannot save form.\n" title:@"Form"];
        return;
    }
    free(form->path);
    form->path = vb_strdup(panel.URL.path.UTF8String);
    form->dirty = 0;
    _projectDirty = NO;
    [self updateTitle];
    [self updateStatus];
}

- (void)saveCurrentForm
{
    VbForm *form = (_currentItem && _currentItem->form) ? _currentItem->form : NULL;
    if (!form) { NSBeep(); return; }
    NSString *path = form->path ? StringFromC(form->path) : nil;
    if (!path.length) {
        NSSavePanel *panel = [NSSavePanel savePanel];
        panel.allowedContentTypes = @[[UTType typeWithFilenameExtension:@"frm"]];
        panel.nameFieldStringValue = [NSString stringWithFormat:@"%s.frm", vb_form_name(form)];
        if ([panel runModal] != NSModalResponseOK || !panel.URL) return;
        path = panel.URL.path;
        free(form->path);
        form->path = vb_strdup(path.UTF8String);
    }
    if (!vb_form_save(form, path.UTF8String)) {
        [VBGmbRunner showOutput:[NSString stringWithFormat:@"Cannot save %@\n", path] title:@"Form"];
        return;
    }
    form->dirty = 0;
    _projectDirty = NO;
    [self updateTitle];
    [self updateStatus];
}

- (void)addForm
{
    if (!_project) {
        [self newProject];
        return;
    }
    int index = 1;
    char name[64];
    do {
        snprintf(name, sizeof(name), "Form%d", index++);
    } while (vb_project_find_item(_project, name) != NULL);

    VbItem *item = vb_project_add_item(_project, VB_ITEM_FORM, name, NULL);
    if (!item) return;
    snprintf(item->kind_key, sizeof(item->kind_key), "%s", "Form");
    item->form = vb_form_new(name);
    if (item->form) {
        char quoted[96];
        snprintf(quoted, sizeof(quoted), "\"%s\"", name);
        vb_control_set(item->form->root, "Caption", quoted);
    }
    _projectDirty = YES;
    [_outline setProject:_project];
    [self selectItem:item];
    [_outline selectItem:item];
    [self updateTitle];
    [self updateStatus];
    [self showDesigner];
}

- (void)selectItem:(VbItem *)item
{
    _currentItem = item;
    if (!item) {
        [_designer setForm:NULL];
        [_properties setControl:NULL];
        [_codeEditor setCode:@""];
        [self updateTitle];
        [self updateStatus];
        return;
    }

    if (item->kind == VB_ITEM_FORM) {
        if (!item->form && item->file) item->form = vb_form_load(item->file);
        if (!item->form) {
            [_designer setForm:NULL];
            [_properties setControl:NULL];
            [_codeEditor setCode:@""];
            [self showCode];
            [self updateStatus];
            return;
        }
        [_designer setForm:item->form];
        [_designer refresh];
        [_properties setObjects:[self propertyObjects]];
        [_properties setControl:item->form->root];
        [_codeEditor setCode:StringFromC(item->form->code)];
        [_codeEditor setBreakpointLines:[self breakpointSetForForm:item->form]];
        _designerWindow.titleText = [NSString stringWithFormat:@"%@ - %@ (Form)",
                                     _project && _project->name ? StringFromC(_project->name) : @"Project1",
                                     StringFromC(item->name)];
        _codeWindow.titleText = [NSString stringWithFormat:@"%@ - %@ (Code)",
                                 _project && _project->name ? StringFromC(_project->name) : @"Project1",
                                 StringFromC(item->name)];
        if (_showCode) [self showCode];
        else [self showDesigner];
    } else {
        [_designer setForm:NULL];
        [_properties setControl:NULL];
        [_properties reload];
        [_codeEditor setCode:[self codeForItem:item]];
        _codeWindow.titleText = [NSString stringWithFormat:@"%@ - %@ (Code)",
                                 _project && _project->name ? StringFromC(_project->name) : @"Project1",
                                 StringFromC(item->name)];
        [self showCode];
    }
    [self updateTitle];
    [self updateStatus];
    [self updateToolbarState];
}

- (NSString *)codeForItem:(VbItem *)item
{
    if (!item) return @"";
    if (item->kind == VB_ITEM_FORM && item->form) return StringFromC(item->form->code);
    NSValue *key = [NSValue valueWithPointer:item];
    NSString *cached = _moduleCodes[key];
    if (cached) return cached;
    if (!item->file) return @"";
    size_t len = 0;
    char *data = vb_read_file(item->file, &len);
    NSString *text = data ? [[NSString alloc] initWithBytes:data length:len encoding:NSUTF8StringEncoding] : nil;
    free(data);
    if (!text) text = @"";
    _moduleCodes[key] = text;
    return text;
}

- (void)updateTitle
{
    if (!_project) {
        self.window.title = @"GMBasicIDE";
        return;
    }
    NSString *name = StringFromC(_project->name);
    NSString *mode = _runtime ? @"run" : (_showCode ? @"code" : @"design");
    NSMutableString *title = [NSMutableString stringWithFormat:@"%@%@ - GMBasicIDE [%@]",
                              name, _projectDirty ? @" *" : @"", mode];
    if (_currentItem) {
        NSString *kind = _currentItem->kind == VB_ITEM_FORM ? @"Form" : @"Code";
        [title appendFormat:@" - [%@ (%@)]", StringFromC(_currentItem->name), kind];
    }
    self.window.title = title;
    if (_currentItem) {
        _designerWindow.titleText = [NSString stringWithFormat:@"%@ - %@ (Form)", name, StringFromC(_currentItem->name)];
        _codeWindow.titleText = [NSString stringWithFormat:@"%@ - %@ (Code)", name, StringFromC(_currentItem->name)];
    }
    VbForm *activeForm = (_currentItem && _currentItem->form) ? _currentItem->form : NULL;
    if (_formLayoutDock) [_formLayoutDock setForm:activeForm];
    if (_formLayoutWindow.isVisible) [_formLayoutView setForm:activeForm];
}

- (void)updateStatus
{
    [self updateToolbarState];

    if (!_project) {
        _status.stringValue = @"Ready";
        _statusMode.stringValue = @"DESIGN";
        _statusPos.stringValue = @"";
        return;
    }
    NSMutableString *text = [NSMutableString string];
    if (_currentItem && _currentItem->form) {
        VbForm *form = _currentItem->form;
        [text appendFormat:@"%@  ·  %d × %d twips  ·  %d controls",
         StringFromC(vb_form_name(form)), vb_form_client_width(form), vb_form_client_height(form),
         CountControls(form->root)];
    } else if (_currentItem) {
        [text appendFormat:@"%@", StringFromC(_currentItem->name)];
    } else {
        [text appendString:StringFromC(_project->name)];
    }
    VbControl *root = (_currentItem && _currentItem->form) ? _currentItem->form->root : NULL;
    if (_designer.selectionCount > 1) {
        [text appendFormat:@"  ·  %lu controlli selezionati", (unsigned long)_designer.selectionCount];
    } else if (_designer.selected && _designer.selected != root) {
        [text appendFormat:@"  ·  %@ (%@)", StringFromC(_designer.selected->name),
         [StringFromC(_designer.selected->type) stringByReplacingOccurrencesOfString:@"VB." withString:@""]];
    }
    _status.stringValue = text;

    /* The break self-test window has no Run > Start behind it, but while it is paused it
       is the run the IDE is showing: mirror it so the status bar really says BREAK. */
    VBRuntimeWindow *running = _runtime;
    if (!running && _runtimeBreakTest.isInBreakMode) running = _runtimeBreakTest;
    if (running && running.isInBreakMode) {
        _statusMode.stringValue = @"BREAK";
    } else if (running) {
        _statusMode.stringValue = @"RUNNING";
    } else {
        _statusMode.stringValue = @"DESIGN";
    }

    if (_designer.selectionCount > 1) {
        _statusPos.stringValue = [NSString stringWithFormat:@"%lu selezionati",
                                  (unsigned long)_designer.selectionCount];
    } else if (_designer.selected && _designer.selected != root) {
        _statusPos.stringValue = [NSString stringWithFormat:@"Left %d  Top %d  W %d  H %d",
                                  vb_control_get_int(_designer.selected, "Left", 0),
                                  vb_control_get_int(_designer.selected, "Top", 0),
                                  vb_control_get_int(_designer.selected, "Width", 0),
                                  vb_control_get_int(_designer.selected, "Height", 0)];
    } else {
        _statusPos.stringValue = _currentItem && _currentItem->form
            ? [NSString stringWithFormat:@"%d × %d", vb_form_client_width(_currentItem->form),
               vb_form_client_height(_currentItem->form)]
            : @"";
    }
}

#pragma mark - Saving

- (void)refreshOutline
{
    if (!_project) return;
    VbItem *current = _currentItem;
    [_outline setProject:_project];
    if (current) [_outline selectItem:current];
}

- (BOOL)ensureFilePaths
{
    if (!_project || !_project->dir) return NO;
    for (int i = 0; i < _project->nitems; i++) {
        VbItem *item = &_project->items[i];
        if (item->file && *item->file) continue;
        char filename[512];
        snprintf(filename, sizeof(filename), "%s.%s", item->name ? item->name : "Item",
                 ExtensionForKind(item->kind));
        item->file = vb_path_join(_project->dir, filename);
    }
    return YES;
}

- (BOOL)saveModules
{
    for (NSValue *key in [_dirtyModules copy]) {
        VbItem *item = (VbItem *)key.pointerValue;
        NSString *code = _moduleCodes[key];
        if (!item || !item->file || !code) continue;
        if (vb_write_file(item->file, code.UTF8String, strlen(code.UTF8String))) {
            [_dirtyModules removeObject:key];
        }
    }
    return YES;
}

- (BOOL)saveProject
{
    if (!_project) return NO;
    if (!_project->path) return [self saveProjectAs];
    if (![self ensureFilePaths]) return [self saveProjectAs];

    for (int i = 0; i < _project->nitems; i++) {
        VbItem *item = &_project->items[i];
        if (item->kind == VB_ITEM_FORM && item->form && item->file) {
            if (!vb_form_save(item->form, item->file)) {
                NSAlert *alert = [[NSAlert alloc] init];
                alert.messageText = @"Could not save form";
                alert.informativeText = [NSString stringWithFormat:@"Failed to write %@.", StringFromC(item->file)];
                [alert addButtonWithTitle:@"OK"];
                [alert runModal];
                return NO;
            }
        }
    }
    [self saveModules];
    if (!vb_project_save(_project, _project->path)) {
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = @"Could not save project";
        alert.informativeText = [NSString stringWithFormat:@"Failed to write %@.", StringFromC(_project->path)];
        [alert addButtonWithTitle:@"OK"];
        [alert runModal];
        return NO;
    }

    _projectDirty = NO;
    [self refreshOutline];
    [self updateTitle];
    [self updateStatus];
    return YES;
}

- (BOOL)saveProjectAs
{
    if (!_project) return NO;
    NSSavePanel *panel = [NSSavePanel savePanel];
    panel.title = @"Save Project As";
    panel.allowedContentTypes = @[[UTType typeWithFilenameExtension:@"vbp"]];
    panel.canCreateDirectories = YES;
    panel.nameFieldStringValue = [NSString stringWithFormat:@"%@.vbp", StringFromC(_project->name)];
    if ([panel runModal] != NSModalResponseOK || !panel.URL) return NO;

    NSString *path = panel.URL.path;
    NSString *dir = path.stringByDeletingLastPathComponent;
    free(_project->path);
    free(_project->dir);
    _project->path = vb_strdup(path.UTF8String);
    _project->dir = vb_strdup(dir.UTF8String);

    for (int i = 0; i < _project->nitems; i++) {
        VbItem *item = &_project->items[i];
        if (item->kind != VB_ITEM_FORM && item->file) {
            NSValue *key = [NSValue valueWithPointer:item];
            if (!_moduleCodes[key]) {
                size_t len = 0;
                char *data = vb_read_file(item->file, &len);
                if (data) {
                    NSString *code = [[NSString alloc] initWithBytes:data length:len encoding:NSUTF8StringEncoding];
                    if (code) _moduleCodes[key] = code;
                    free(data);
                }
            }
        }
        free(item->file);
        item->file = NULL;
    }
    [self ensureFilePaths];
    _projectDirty = YES;
    return [self saveProject];
}

#pragma mark - Run mode

- (NSIndexSet *)breakpointSetForForm:(VbForm *)form
{
    NSMutableIndexSet *set = [NSMutableIndexSet indexSet];
    int i;
    if (!form) return set;
    for (i = 0; i < vb_form_breakpoint_count(form); i++) {
        int line = vb_form_breakpoint_at(form, i);
        if (line > 0) [set addIndex:(NSUInteger)line];
    }
    return set;
}

- (void)runProject
{
    if (!_project) {
        NSBeep();
        return;
    }
    if (![self saveModules]) return;
    VbItem *startup = _project->startup ? vb_project_find_item(_project, _project->startup) : NULL;
    if (!startup || startup->kind != VB_ITEM_FORM || !startup->form) {
        startup = NULL;
        for (int i = 0; i < _project->nitems; i++) {
            if (_project->items[i].kind == VB_ITEM_FORM && _project->items[i].form) {
                startup = &_project->items[i];
                break;
            }
        }
    }
    if (!startup || !startup->form) {
        NSBeep();
        return;
    }
    if (_runtime) [self stopProject];
    _runtime = [[VBRuntimeWindow alloc] initWithForm:startup->form];
    if (!_runtime) {
        NSBeep();
        return;
    }
    _runtime.delegate = self;
    _runtime.breakpointLines = [self breakpointSetForForm:startup->form];
    [_runtime startRuntime];
    [self updateStatus];
    [self updateTitle];
}

- (void)stopProject
{
    if (!_runtime) return;
    VBRuntimeWindow *runtime = _runtime;
    _runtime = nil;
    [runtime.window close];
    [self updateStatus];
    [self updateTitle];
}

- (void)runtimeWindowDidClose:(VBRuntimeWindow *)window
{
    /* Test/diagnostic runtime windows must not clear a project run. */
    if (window != _runtime) return;
    _runtime = nil;
    [self updateStatus];
    [self updateTitle];
}

/* Shows the form the runtime window belongs to and highlights a statement: the code
   window becomes the active MDI child, like Debug in VB6. */
- (void)revealRuntimeHandler:(NSString *)handler line:(NSInteger)line inWindow:(VBRuntimeWindow *)window
{
    VbForm *form = window.form;
    VbItem *item = NULL;
    if (!_project || !form) return;
    for (int i = 0; i < _project->nitems; i++) {
        if (_project->items[i].form == form) {
            item = &_project->items[i];
            break;
        }
    }
    if (!item) return;
    if (item != _currentItem) [self selectItem:item];
    [self showCode];
    if (line > 0) [_codeEditor showDebugLine:line];
    fprintf(stderr, "debug: %s.%s line %ld\n", StringFromC(_project->name).UTF8String,
            handler.UTF8String, (long)line);
}

/* Debug with break mode unavailable (automation): stop on the offending statement. */
- (void)runtimeWindow:(VBRuntimeWindow *)window didRequestDebugForHandler:(NSString *)handler
                 line:(NSInteger)line
{
    [self revealRuntimeHandler:handler line:line inWindow:window];
}

/* Break mode: every pause (Debug, Run > Break, each F8 step) moves the highlight. */
- (void)runtimeWindow:(VBRuntimeWindow *)window didBreakAtHandler:(NSString *)handler
                 line:(NSInteger)line
{
    [self revealRuntimeHandler:handler line:line inWindow:window];
    if (window == _runtime || window == _runtimeBreakTest) [self updateStatus];
}

- (void)runtimeWindowDidLeaveBreakMode:(VBRuntimeWindow *)window
{
    [_codeEditor clearDebugLine];
    if (window == _runtime || window == _runtimeBreakTest) [self updateStatus];
}

#pragma mark - Toolbox

- (void)toolboxView:(VBToolboxView *)view didSelectType:(NSString *)type
{
    (void)view;
    if (!_currentItem || _currentItem->kind != VB_ITEM_FORM || !_currentItem->form) {
        NSBeep();
        [_toolbox clearSelection];
        return;
    }
    [self showDesigner];
    _designer.pendingTool = type;
    [self.window makeFirstResponder:_designer];
}

#pragma mark - Project outline

- (void)projectOutline:(VBProjectOutlineView *)outline didSelectItem:(VbItem *)item
{
    (void)outline;
    if (item == _currentItem) return;
    [self selectItem:item];
}

- (void)projectOutline:(VBProjectOutlineView *)outline didActivateItem:(VbItem *)item
{
    (void)outline;
    [self selectItem:item];
    if (item->kind == VB_ITEM_FORM) [self showDesigner];
    else [self showCode];
}

#pragma mark - Designer

- (BOOL)runDesignerSelectionSelfTest
{
    VbForm *form = (_currentItem && _currentItem->form) ? _currentItem->form : NULL;
    if (!_designer || !form) return NO;
    [self showDesigner];
    if (_designer.form != form) [_designer setForm:form];
    return VBRunDesignerSelectionSelfTest(_designer, form);
}

- (BOOL)runFormatSelfTest
{
    VbForm *form = (_currentItem && _currentItem->form) ? _currentItem->form : NULL;
    BOOL ok;
    if (!_designer || !_menuBar || !form || !form->root) return NO;
    ok = VBRunFormatToolbarSelfTest(_formatBar, _designer, form);
    ok = VBRunFormatMenuSelfTest(_menuBar, _designer, form) && ok;
    [self updateFormatMenuState];
    ok = VBCheckFormatMenuState(_menuBar, YES, YES, YES) && ok;
    ok = VBCheckToolbarState(_formatBar, YES, YES) && ok;
    if (form->root->nchildren > 0) [_designer selectControl:form->root->children[0]];
    [self updateFormatMenuState];
    ok = VBCheckFormatMenuState(_menuBar, NO, NO, YES) && ok;
    ok = VBCheckToolbarState(_formatBar, NO, NO) && ok;
    [_designer selectControl:form->root];
    [self updateFormatMenuState];
    ok = VBCheckFormatMenuState(_menuBar, NO, NO, NO) && ok;
    ok = VBCheckToolbarState(_formatBar, NO, NO) && ok;
    ok = VBCheckTabOrderMenu(_menuBar) && ok;
    ok = VBRunZOrderSelfTest(_menuBar, _designer, form) && ok;
    [self updateFormatMenuState];
    return ok;
}

- (NSWindow *)runtimeTabTestWindow
{
    return _runtimeTabTest.window;
}

- (NSWindow *)runtimeAccessKeyTestWindow
{
    return _runtimeAccessKeyTest.window;
}

- (NSWindow *)runtimeFocusTestWindow
{
    return _runtimeFocusTest.window;
}

- (NSWindow *)runtimeErrorTestWindow
{
    return _runtimeErrorTest.window;
}

#define RT_EXPECT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "runtime-tab: FAIL at line %d\n", __LINE__); \
        ok = NO; \
    } \
} while (0)

- (BOOL)runRuntimeListSelfTest
{
    VbForm *form = (_currentItem && _currentItem->form) ? _currentItem->form : NULL;
    VbControl *list;
    VBRuntimeWindow *runtime;
    NSInteger baseCount;
    BOOL ok = YES;
    if (!form || !form->root) return NO;
    list = vb_form_find_control(form, "List1");
    if (!list) list = vb_form_add_control(form, form->root, "VB.ListBox", 240, 240, 2000, 1000);
    if (!list) return NO;
    vb_control_set_display(list, "List", "Alpha\nBeta");
    vb_control_set_int(list, "ListIndex", 1);
    {
        NSString *code = [NSString stringWithFormat:@"%s\n"
                          "Private Sub ListRun()\n"
                          "    List1.AddItem \"Gamma\"\n"
                          "    List1.ListIndex = 0\n"
                          "End Sub\n\n"
                          "Private Sub ListClear()\n"
                          "    List1.Clear\n"
                          "End Sub\n",
                          form->code ? form->code : ""];
        free(form->code);
        form->code = strdup(code.UTF8String);
    }
    [self selectItem:_currentItem];
    runtime = [[VBRuntimeWindow alloc] initWithForm:form];
    runtime.suppressDialogs = YES;
    _runtimeListTest = runtime;
    [runtime startRuntime];
    baseCount = [runtime listItemCountForControl:@"List1"];
    if (baseCount != 2) ok = NO;
    if (![[runtime propertyValueForControl:@"List1" property:@"ListIndex"] isEqualToString:@"1"]) ok = NO;
    [runtime runEventHandler:@"ListRun"];
    if ([runtime listItemCountForControl:@"List1"] != 3) ok = NO;
    if (![[runtime propertyValueForControl:@"List1" property:@"ListIndex"] isEqualToString:@"0"]) ok = NO;
    [runtime runEventHandler:@"ListClear"];
    if ([runtime listItemCountForControl:@"List1"] != 0) ok = NO;
    fprintf(stderr, "runtime-list: items=%ld index=%s %s\n",
            (long)[runtime listItemCountForControl:@"List1"],
            [runtime propertyValueForControl:@"List1" property:@"ListIndex"].UTF8String,
            ok ? "PASS" : "FAIL");
    return ok;
}

- (BOOL)runRuntimeTabSelfTest
{
    VbForm *form = (_currentItem && _currentItem->form) ? _currentItem->form : NULL;
    VbControl *text;
    VbControl *command;
    VbControl *label;
    VbControl *frame;
    VbControl *innerButton;
    VbControl *innerText;
    NSString *frameName;
    NSString *innerButtonName;
    NSString *innerTextName;
    NSArray<NSString *> *names;
    BOOL ok = YES;
    if (!form || !form->root) return NO;
    text = vb_form_find_control(form, "Text1");
    command = vb_form_find_control(form, "Command1");
    label = vb_form_find_control(form, "Label1");
    if (!text || !command || !label) {
        fprintf(stderr, "runtime-tab: fixture needs Text1, Command1 and Label1\n");
        return NO;
    }
    /* Non-contiguous TabIndex proves the runtime sorts instead of using z-order. */
    vb_control_set_int(command, "TabIndex", 0);
    vb_control_set_int(label, "TabIndex", 3);
    vb_control_set_int(text, "TabIndex", 4);

    /* Nested container: its children take the Frame's slot in the tab order. */
    frame = vb_form_add_control(form, form->root, "VB.Frame", 3000, 2400, 1500, 600);
    vb_control_set_int(frame, "TabIndex", 1);
    innerButton = vb_form_add_control(form, frame, "VB.CommandButton", 120, 120, 1200, 240);
    innerText = vb_form_add_control(form, frame, "VB.TextBox", 120, 360, 1200, 180);
    vb_control_set_int(innerButton, "TabIndex", 0);
    vb_control_set_int(innerText, "TabIndex", 1);
    frameName = StringFromC(frame->name);
    innerButtonName = StringFromC(innerButton->name);
    innerTextName = StringFromC(innerText->name);

    _runtimeTabTest = [[VBRuntimeWindow alloc] initWithForm:form];
    [_runtimeTabTest startRuntime];

    names = [_runtimeTabTest tabOrderControlNames];
    RT_EXPECT(names.count == 4 && [names[0] isEqualToString:@"Command1"] &&
          [names[1] isEqualToString:innerButtonName] &&
          [names[2] isEqualToString:innerTextName] && [names[3] isEqualToString:@"Text1"]);
    fprintf(stderr, "runtime-tab: order=%s\n", names.description.UTF8String);
    RT_EXPECT([[_runtimeTabTest focusedControlName] isEqualToString:@"Command1"]);
    fprintf(stderr, "runtime-tab: initial focus=%s\n",
            [_runtimeTabTest focusedControlName].UTF8String);

    RT_EXPECT([_runtimeTabTest sendTabKeyBackwards:NO]);
    RT_EXPECT([[_runtimeTabTest focusedControlName] isEqualToString:innerButtonName]);
    fprintf(stderr, "runtime-tab: tab into frame -> %s\n",
            [_runtimeTabTest focusedControlName].UTF8String);
    RT_EXPECT([_runtimeTabTest sendTabKeyBackwards:NO]);
    RT_EXPECT([[_runtimeTabTest focusedControlName] isEqualToString:innerTextName]);
    fprintf(stderr, "runtime-tab: tab inside frame -> %s\n",
            [_runtimeTabTest focusedControlName].UTF8String);
    RT_EXPECT([_runtimeTabTest sendTabKeyBackwards:NO]);
    RT_EXPECT([[_runtimeTabTest focusedControlName] isEqualToString:@"Text1"]);
    RT_EXPECT([_runtimeTabTest sendTabKeyBackwards:NO]);
    RT_EXPECT([[_runtimeTabTest focusedControlName] isEqualToString:@"Command1"]);
    fprintf(stderr, "runtime-tab: wrap -> %s\n", [_runtimeTabTest focusedControlName].UTF8String);

    /* Hiding a nested Frame removes its children from the tab cycle; when the focus was
       inside, it leaves the Frame and lands on the next control of the rebuilt order. */
    RT_EXPECT([_runtimeTabTest sendTabKeyBackwards:NO]);
    RT_EXPECT([[_runtimeTabTest focusedControlName] isEqualToString:innerButtonName]);
    RT_EXPECT([_runtimeTabTest sendTabKeyBackwards:NO]);
    RT_EXPECT([[_runtimeTabTest focusedControlName] isEqualToString:innerTextName]);
    [_runtimeTabTest setProperty:@"Visible" forControl:frameName value:@"False"];
    names = [_runtimeTabTest tabOrderControlNames];
    RT_EXPECT(names.count == 2 && [names[0] isEqualToString:@"Command1"] &&
          [names[1] isEqualToString:@"Text1"]);
    fprintf(stderr, "runtime-tab: frame hidden -> %s focus=%s\n", names.description.UTF8String, [_runtimeTabTest focusedControlName].UTF8String);
    RT_EXPECT([[_runtimeTabTest focusedControlName] isEqualToString:@"Text1"]);
    RT_EXPECT([_runtimeTabTest focusNextControlBackwards:NO]);
    RT_EXPECT([[_runtimeTabTest focusedControlName] isEqualToString:@"Command1"]);
    [_runtimeTabTest setProperty:@"Visible" forControl:frameName value:@"True"];
    names = [_runtimeTabTest tabOrderControlNames];
    RT_EXPECT(names.count == 4);
    RT_EXPECT([[_runtimeTabTest focusedControlName] isEqualToString:@"Command1"]);
    RT_EXPECT([_runtimeTabTest focusNextControlBackwards:NO]);
    RT_EXPECT([[_runtimeTabTest focusedControlName] isEqualToString:innerButtonName]);

    /* Visible = False removes a control while the form is running. */
    [_runtimeTabTest setProperty:@"Visible" forControl:innerButtonName value:@"False"];
    names = [_runtimeTabTest tabOrderControlNames];
    RT_EXPECT(names.count == 3 && ![names containsObject:innerButtonName]);
    fprintf(stderr, "runtime-tab: visible=false -> %s focus=%s\n", names.description.UTF8String, [_runtimeTabTest focusedControlName].UTF8String);
    RT_EXPECT([[_runtimeTabTest focusedControlName] isEqualToString:innerTextName]);
    RT_EXPECT([_runtimeTabTest focusNextControlBackwards:NO]);
    RT_EXPECT([[_runtimeTabTest focusedControlName] isEqualToString:@"Text1"]);

    /* Enabled = False skips a control too. */
    [_runtimeTabTest setProperty:@"Enabled" forControl:innerTextName value:@"False"];
    names = [_runtimeTabTest tabOrderControlNames];
    RT_EXPECT(names.count == 2 && [names[0] isEqualToString:@"Command1"] &&
          [names[1] isEqualToString:@"Text1"]);
    fprintf(stderr, "runtime-tab: enabled=false -> %s focus=%s\n", names.description.UTF8String, [_runtimeTabTest focusedControlName].UTF8String);
    RT_EXPECT([[_runtimeTabTest focusedControlName] isEqualToString:@"Text1"]);
    RT_EXPECT([_runtimeTabTest sendTabKeyBackwards:NO]);
    RT_EXPECT([[_runtimeTabTest focusedControlName] isEqualToString:@"Command1"]);

    /* TabStop = False removes the control from the tab cycle. */
    [_runtimeTabTest setProperty:@"TabStop" forControl:@"Text1" value:@"False"];
    names = [_runtimeTabTest tabOrderControlNames];
    RT_EXPECT(names.count == 1 && [names[0] isEqualToString:@"Command1"]);
    fprintf(stderr, "runtime-tab: tabstop=false -> %s\n", names.description.UTF8String);
    RT_EXPECT([_runtimeTabTest focusNextControlBackwards:NO]);
    RT_EXPECT([[_runtimeTabTest focusedControlName] isEqualToString:@"Command1"]);
    /* With a single focusable control Tab is still consumed but the focus stays put. */
    (void)[_runtimeTabTest sendTabKeyBackwards:NO];
    RT_EXPECT([[_runtimeTabTest focusedControlName] isEqualToString:@"Command1"]);

    /* Restore the fixture and leave the focus on the Frame child for the snapshot. */
    [_runtimeTabTest setProperty:@"TabStop" forControl:@"Text1" value:@"True"];
    [_runtimeTabTest setProperty:@"Enabled" forControl:innerTextName value:@"True"];
    [_runtimeTabTest setProperty:@"Visible" forControl:innerButtonName value:@"True"];
    names = [_runtimeTabTest tabOrderControlNames];
    RT_EXPECT(names.count == 4);
    fprintf(stderr, "runtime-tab: restored -> %s focus=%s\n", names.description.UTF8String, [_runtimeTabTest focusedControlName].UTF8String);
    RT_EXPECT([_runtimeTabTest sendTabKeyBackwards:NO]);
    RT_EXPECT([[_runtimeTabTest focusedControlName] isEqualToString:innerButtonName]);
    fprintf(stderr, "runtime-tab: focus inside frame -> %s\n",
            [_runtimeTabTest focusedControlName].UTF8String);

    /* Enabled = False on a Frame disables every control inside it, without touching the
       children's own Enabled flag (Text2 stays disabled after the Frame comes back). */
    [_runtimeTabTest setProperty:@"Enabled" forControl:innerTextName value:@"False"];
    names = [_runtimeTabTest tabOrderControlNames];
    RT_EXPECT(names.count == 3 && [names[0] isEqualToString:@"Command1"] &&
              [names[1] isEqualToString:innerButtonName] && [names[2] isEqualToString:@"Text1"]);
    [_runtimeTabTest setProperty:@"Enabled" forControl:frameName value:@"False"];
    names = [_runtimeTabTest tabOrderControlNames];
    fprintf(stderr, "runtime-tab: frame disabled -> %s focus=%s\n",
            names.description.UTF8String, [_runtimeTabTest focusedControlName].UTF8String);
    RT_EXPECT(names.count == 2 && [names[0] isEqualToString:@"Command1"] &&
              [names[1] isEqualToString:@"Text1"]);
    /* The focus was inside the Frame and must leave it. */
    RT_EXPECT([[_runtimeTabTest focusedControlName] isEqualToString:@"Command1"]);
    [_runtimeTabTest setProperty:@"Enabled" forControl:frameName value:@"True"];
    names = [_runtimeTabTest tabOrderControlNames];
    RT_EXPECT(names.count == 3 && ![names containsObject:innerTextName]);
    [_runtimeTabTest setProperty:@"Enabled" forControl:innerTextName value:@"True"];
    names = [_runtimeTabTest tabOrderControlNames];
    RT_EXPECT(names.count == 4);
    RT_EXPECT([_runtimeTabTest sendTabKeyBackwards:NO]);
    RT_EXPECT([[_runtimeTabTest focusedControlName] isEqualToString:innerButtonName]);

    /* PictureBox containers share the same canvas path: cover them with a second window.
       The PictureBox already has Enabled = False in the model, so the design-time path
       is exercised too (children start outside the tab order and get no focus). */
    {
        VbControl *picture = vb_form_add_control(form, form->root, "VB.PictureBox", 4800, 2400, 1800, 900);
        VbControl *pictureButton;
        VBRuntimeWindow *pictureRuntime;
        NSString *pictureName;
        NSString *pictureButtonName;
        vb_control_set_int(picture, "Enabled", 0);
        pictureButton = vb_form_add_control(form, picture, "VB.CommandButton", 120, 120, 1200, 240);
        pictureName = StringFromC(picture->name);
        pictureButtonName = StringFromC(pictureButton->name);
        pictureRuntime = [[VBRuntimeWindow alloc] initWithForm:form];
        [pictureRuntime startRuntime];
        names = [pictureRuntime tabOrderControlNames];
        fprintf(stderr, "runtime-tab: picturebox disabled at load -> %s\n", names.description.UTF8String);
        RT_EXPECT(![names containsObject:pictureButtonName]);
        RT_EXPECT([[pictureRuntime focusedControlName] isEqualToString:@"Command1"]);
        [pictureRuntime setProperty:@"Enabled" forControl:pictureName value:@"True"];
        RT_EXPECT([[pictureRuntime tabOrderControlNames] containsObject:pictureButtonName]);
        [pictureRuntime setProperty:@"Enabled" forControl:pictureName value:@"False"];
        names = [pictureRuntime tabOrderControlNames];
        fprintf(stderr, "runtime-tab: picturebox disabled at runtime -> %s\n", names.description.UTF8String);
        RT_EXPECT(![names containsObject:pictureButtonName]);
        [pictureRuntime setProperty:@"Enabled" forControl:pictureName value:@"True"];
        RT_EXPECT([[pictureRuntime tabOrderControlNames] containsObject:pictureButtonName]);
        [pictureRuntime close];
        [self.runtimeTabTestWindow makeKeyAndOrderFront:nil];
    }

    /* Leave the Frame disabled for the snapshot: caption and children are greyed out. */
    [_runtimeTabTest setProperty:@"Enabled" forControl:frameName value:@"False"];
    RT_EXPECT([_runtimeTabTest tabOrderControlNames].count == 2);
    fprintf(stderr, "runtime-tab: final focus=%s\n",
            [_runtimeTabTest focusedControlName].UTF8String);
    RT_EXPECT([[_runtimeTabTest focusedControlName] isEqualToString:@"Command1"]);

    fprintf(stderr, "runtime-tab: %s\n", (ok ? @"PASS" : @"FAIL").UTF8String);
    return ok;
}

#undef RT_EXPECT

#define AK_EXPECT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "runtime-access-key: FAIL at line %d\n", __LINE__); \
        ok = NO; \
    } \
} while (0)

/* VB6 access keys: Alt+letter on a caption "&Letter" activates the control. Covers
   CommandButton (Click), CheckBox (toggle + Click), Label (focus moves on), "&&" as a
   literal ampersand, disabled/hidden controls, disabled containers and duplicate keys. */
- (BOOL)runRuntimeAccessKeySelfTest
{
    VbForm *form = vb_form_new("AccessKeys");
    VbControl *text;
    VbControl *save;
    VbControl *check;
    VbControl *nameLabel;
    VbControl *literalLabel;
    VbControl *frame;
    VbControl *inner;
    VbControl *disabled;
    VbControl *duplicate;
    VbControl *status;
    VbControl *quietLabel;
    VBRuntimeWindow *runtime;
    NSString *saveName;
    NSString *checkName;
    NSString *frameName;
    NSString *innerName;
    NSString *duplicateName;
    NSString *statusName;
    NSInteger index = -2;
    BOOL ok = YES;

    if (!form || !form->root) return NO;

    /* Caption parsing: the first single "&" marks the access key, "&&" is one literal
       ampersand, a trailing "&" is dropped and plain captions have no access key. */
    AK_EXPECT([[VBRuntimeWindow displayCaption:@"&Save && Exit" mnemonicIndex:&index] isEqualToString:@"Save & Exit"]);
    AK_EXPECT(index == 0);
    index = -2;
    AK_EXPECT([[VBRuntimeWindow displayCaption:@"A && B" mnemonicIndex:&index] isEqualToString:@"A & B"]);
    AK_EXPECT(index == -1);
    index = -2;
    AK_EXPECT([[VBRuntimeWindow displayCaption:@"Save &" mnemonicIndex:&index] isEqualToString:@"Save "]);
    AK_EXPECT(index == -1);
    index = -2;
    AK_EXPECT([[VBRuntimeWindow displayCaption:@"No key" mnemonicIndex:&index] isEqualToString:@"No key"]);
    AK_EXPECT(index == -1);

    text = vb_form_add_control(form, form->root, "VB.TextBox", 240, 240, 2400, 300);
    save = vb_form_add_control(form, form->root, "VB.CommandButton", 240, 720, 1600, 400);
    check = vb_form_add_control(form, form->root, "VB.CheckBox", 240, 1320, 1600, 300);
    nameLabel = vb_form_add_control(form, form->root, "VB.Label", 240, 1800, 1600, 300);
    literalLabel = vb_form_add_control(form, form->root, "VB.Label", 240, 2160, 1600, 300);
    frame = vb_form_add_control(form, form->root, "VB.Frame", 2400, 720, 2000, 1200);
    inner = vb_form_add_control(form, frame, "VB.CommandButton", 120, 120, 1200, 300);
    disabled = vb_form_add_control(form, form->root, "VB.CommandButton", 2400, 2160, 1600, 400);
    duplicate = vb_form_add_control(form, form->root, "VB.CommandButton", 2400, 2640, 1600, 400);
    status = vb_form_add_control(form, form->root, "VB.Label", 240, 2640, 1600, 300);
    quietLabel = vb_form_add_control(form, form->root, "VB.Label", 3480, 180, 1000, 300);

    vb_control_set_int(text, "TabIndex", 0);
    vb_control_set(save, "Caption", "&Save");
    vb_control_set_int(save, "TabIndex", 1);
    vb_control_set(check, "Caption", "&Bold");
    vb_control_set_int(check, "TabIndex", 2);
    vb_control_set(nameLabel, "Caption", "&Name");
    vb_control_set_int(nameLabel, "TabIndex", 3);
    vb_control_set(literalLabel, "Caption", "A && B");
    vb_control_set_int(literalLabel, "TabIndex", 4);
    vb_control_set(frame, "Caption", "Group");
    vb_control_set_int(frame, "TabIndex", 5);
    vb_control_set(inner, "Caption", "&Inner");
    vb_control_set_int(inner, "TabIndex", 0);
    vb_control_set(disabled, "Caption", "&Off");
    vb_control_set_int(disabled, "Enabled", 0);
    vb_control_set_int(disabled, "TabIndex", 6);
    vb_control_set(duplicate, "Caption", "&Save");
    vb_control_set_int(duplicate, "TabIndex", 7);
    vb_control_set_int(status, "TabIndex", 8);
    vb_control_set(quietLabel, "Caption", "&Quit");
    vb_control_set_int(quietLabel, "UseMnemonic", 0);
    vb_control_set_int(quietLabel, "TabIndex", 9);

    saveName = StringFromC(save->name);
    checkName = StringFromC(check->name);
    frameName = StringFromC(frame->name);
    innerName = StringFromC(inner->name);
    duplicateName = StringFromC(duplicate->name);
    statusName = StringFromC(status->name);
    {
        NSString *code = [NSString stringWithFormat:@"%s\n"
                          "Private Sub %s_Click()\n"
                          "    %s.Caption = \"saved\"\n"
                          "End Sub\n\n"
                          "Private Sub %s_Click()\n"
                          "    %s.Caption = \"inner\"\n"
                          "End Sub\n\n"
                          "Private Sub %s_Click()\n"
                          "    %s.Caption = \"duplicate\"\n"
                          "End Sub\n",
                          form->code ? form->code : "",
                          save->name, status->name, inner->name, status->name,
                          duplicate->name, status->name];
        free(form->code);
        form->code = strdup(code.UTF8String);
    }

    runtime = [[VBRuntimeWindow alloc] initWithForm:form];
    runtime.suppressDialogs = YES;
    [runtime startRuntime];
    _runtimeAccessKeyTest = runtime;

    AK_EXPECT([[runtime focusedControlName] isEqualToString:StringFromC(text->name)]);

    /* A plain letter still types into the focused TextBox; Alt+letter does not. */
    [runtime sendTestKeyEvent:@"s" optionDown:NO];
    AK_EXPECT([[runtime propertyValueForControl:StringFromC(text->name) property:@"Text"] isEqualToString:@"s"]);

    /* The real AppKit dispatch (NSApplication -> key window -> responder) must also
       route Alt+letter to the access key instead of the TextBox. */
    [runtime dispatchTestKeyEvent:@"s" optionDown:YES];
    AK_EXPECT([[runtime focusedControlName] isEqualToString:saveName]);
    AK_EXPECT([[runtime propertyValueForControl:StringFromC(text->name) property:@"Text"] isEqualToString:@"s"]);
    fprintf(stderr, "runtime-access-key: NSApp dispatch Alt+S -> %s (no text inserted)\n",
            [runtime focusedControlName].UTF8String);

    [runtime sendTestKeyEvent:@"s" optionDown:YES];
    AK_EXPECT([[runtime focusedControlName] isEqualToString:saveName]);
    AK_EXPECT([[runtime propertyValueForControl:statusName property:@"Caption"] isEqualToString:@"saved"]);
    AK_EXPECT([[runtime propertyValueForControl:StringFromC(text->name) property:@"Text"] isEqualToString:@"s"]);
    fprintf(stderr, "runtime-access-key: Alt+S -> %s (%s)\n",
            [runtime focusedControlName].UTF8String,
            [runtime propertyValueForControl:statusName property:@"Caption"].UTF8String);

    /* CheckBox: the access key toggles Value and fires Click, like a real click. */
    [runtime sendTestKeyEvent:@"b" optionDown:YES];
    AK_EXPECT([[runtime focusedControlName] isEqualToString:checkName]);
    AK_EXPECT([[runtime propertyValueForControl:checkName property:@"Value"] intValue] != 0);
    [runtime sendTestKeyEvent:@"b" optionDown:YES];
    AK_EXPECT([[runtime propertyValueForControl:checkName property:@"Value"] intValue] == 0);
    fprintf(stderr, "runtime-access-key: Alt+B focus=%s value=%s\n",
            [runtime focusedControlName].UTF8String,
            [runtime propertyValueForControl:checkName property:@"Value"].UTF8String);

    /* Labels cannot take the focus: the key moves it to the next control in tab order,
       skipping the caption-less Label2 and the container itself. */
    [runtime sendTestKeyEvent:@"n" optionDown:YES];
    AK_EXPECT([[runtime focusedControlName] isEqualToString:innerName]);
    fprintf(stderr, "runtime-access-key: Alt+N (Label) -> %s\n",
            [runtime focusedControlName].UTF8String);

    /* A control inside a Frame answers its access key from outside the Frame. */
    [runtime sendTestKeyEvent:@"s" optionDown:YES];
    AK_EXPECT([[runtime focusedControlName] isEqualToString:saveName]);
    [runtime sendTestKeyEvent:@"i" optionDown:YES];
    AK_EXPECT([[runtime focusedControlName] isEqualToString:innerName]);
    AK_EXPECT([[runtime propertyValueForControl:statusName property:@"Caption"] isEqualToString:@"inner"]);

    /* Disabled controls and the literal "&&" caption never claim the key. */
    [runtime sendTestKeyEvent:@"o" optionDown:YES];
    AK_EXPECT([[runtime focusedControlName] isEqualToString:innerName]);
    [runtime sendTestKeyEvent:@"&" optionDown:YES];
    AK_EXPECT([[runtime focusedControlName] isEqualToString:innerName]);

    /* A disabled container mutes the access keys of its children as well. */
    [runtime setProperty:@"Enabled" forControl:frameName value:@"False"];
    AK_EXPECT(![[runtime focusedControlName] isEqualToString:innerName]);
    {
        NSString *before = [runtime focusedControlName];
        [runtime sendTestKeyEvent:@"i" optionDown:YES];
        AK_EXPECT([[runtime focusedControlName] isEqualToString:before]);
        fprintf(stderr, "runtime-access-key: frame disabled, Alt+I keeps focus=%s\n",
                [runtime focusedControlName].UTF8String);
    }
    [runtime setProperty:@"Enabled" forControl:frameName value:@"True"];
    [runtime sendTestKeyEvent:@"i" optionDown:YES];
    AK_EXPECT([[runtime focusedControlName] isEqualToString:innerName]);

    /* Duplicate access keys: the first control in tab order wins, and a hidden one
       hands the key to the next candidate with the same caption. */
    [runtime sendTestKeyEvent:@"s" optionDown:YES];
    AK_EXPECT([[runtime focusedControlName] isEqualToString:saveName]);
    AK_EXPECT([[runtime propertyValueForControl:statusName property:@"Caption"] isEqualToString:@"saved"]);
    [runtime setProperty:@"Visible" forControl:saveName value:@"False"];
    [runtime sendTestKeyEvent:@"s" optionDown:YES];
    AK_EXPECT([[runtime focusedControlName] isEqualToString:duplicateName]);
    AK_EXPECT([[runtime propertyValueForControl:statusName property:@"Caption"] isEqualToString:@"duplicate"]);
    fprintf(stderr, "runtime-access-key: duplicate key -> %s\n",
            [runtime focusedControlName].UTF8String);
    [runtime setProperty:@"Visible" forControl:saveName value:@"True"];
    [runtime setProperty:@"Text" forControl:StringFromC(text->name) value:@""];
    [runtime sendTestKeyEvent:@"s" optionDown:YES];
    AK_EXPECT([[runtime focusedControlName] isEqualToString:saveName]);

    /* No view has the focus (the window is the first responder): the window itself
       still answers the access key instead of beeping. */
    [runtime.window makeFirstResponder:nil];
    AK_EXPECT([[runtime focusedControlName] isEqualToString:@""]);
    [runtime sendTestKeyEvent:@"s" optionDown:YES];
    AK_EXPECT([[runtime focusedControlName] isEqualToString:saveName]);
    fprintf(stderr, "runtime-access-key: window responder, Alt+S -> %s\n",
            [runtime focusedControlName].UTF8String);

    /* Label.UseMnemonic = False keeps the caption's ampersand out of the access keys. */
    [runtime sendTestKeyEvent:@"q" optionDown:YES];
    AK_EXPECT([[runtime focusedControlName] isEqualToString:saveName]);

    fprintf(stderr, "runtime-access-key: %s\n", (ok ? @"PASS" : @"FAIL").UTF8String);
    return ok;
}

#undef AK_EXPECT

#define FE_EXPECT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "runtime-focus: FAIL at line %d\n", __LINE__); \
        ok = NO; \
    } \
} while (0)

/* VB6 focus: SetFocus from code, GotFocus/LostFocus on every focus change (mouse, Tab,
   access keys, SetFocus), Form_Load before the first GotFocus, and no runaway recursion
   when two handlers keep moving the focus. */
- (BOOL)runRuntimeFocusSelfTest
{
    VbForm *form = vb_form_new("FocusEvents");
    VbControl *text1;
    VbControl *text2;
    VbControl *text3;
    VbControl *command1;
    VbControl *command2;
    VbControl *command3;
    VbControl *command4;
    VbControl *command5;
    VbControl *command6;
    VbControl *command7;
    VbControl *command8;
    VbControl *logLabel;
    VBRuntimeWindow *runtime;
    NSString *logName;
    NSString *log;
    BOOL ok = YES;

    if (!form || !form->root) return NO;

    text1 = vb_form_add_control(form, form->root, "VB.TextBox", 240, 120, 2000, 300);
    text2 = vb_form_add_control(form, form->root, "VB.TextBox", 240, 480, 2000, 300);
    text3 = vb_form_add_control(form, form->root, "VB.TextBox", 240, 840, 2000, 300);
    command1 = vb_form_add_control(form, form->root, "VB.CommandButton", 240, 1260, 1600, 360);
    command2 = vb_form_add_control(form, form->root, "VB.CommandButton", 240, 1740, 1600, 360);
    command3 = vb_form_add_control(form, form->root, "VB.CommandButton", 2400, 120, 1800, 360);
    command4 = vb_form_add_control(form, form->root, "VB.CommandButton", 2400, 540, 1800, 360);
    command5 = vb_form_add_control(form, form->root, "VB.CommandButton", 2400, 960, 1800, 360);
    command6 = vb_form_add_control(form, form->root, "VB.CommandButton", 2400, 1380, 1800, 360);
    command7 = vb_form_add_control(form, form->root, "VB.CommandButton", 2400, 1800, 1800, 360);
    command8 = vb_form_add_control(form, form->root, "VB.CommandButton", 2400, 2220, 1800, 360);
    logLabel = vb_form_add_control(form, form->root, "VB.Label", 240, 2280, 1900, 600);

    vb_control_set_int(text1, "TabIndex", 0);
    vb_control_set_int(text2, "TabIndex", 1);
    vb_control_set_int(text3, "TabIndex", 2);
    vb_control_set_int(text3, "TabStop", 0);
    vb_control_set(command1, "Caption", "&Go");
    vb_control_set_int(command1, "TabIndex", 3);
    vb_control_set(command2, "Caption", "&Off");
    vb_control_set_int(command2, "Enabled", 0);
    vb_control_set_int(command2, "TabIndex", 4);
    vb_control_set(command3, "Caption", "Call Text1.SetFocus");
    vb_control_set_int(command3, "TabIndex", 5);
    vb_control_set(command4, "Caption", "SetFocus on disabled");
    vb_control_set_int(command4, "TabIndex", 6);
    vb_control_set(command5, "Caption", "SetFocus on Label");
    vb_control_set_int(command5, "TabIndex", 7);
    vb_control_set(command6, "Caption", "SetFocus TabStop = False");
    vb_control_set_int(command6, "TabIndex", 8);
    vb_control_set(command7, "Caption", "Ping");
    vb_control_set_int(command7, "TabIndex", 9);
    vb_control_set(command8, "Caption", "Pong");
    vb_control_set_int(command8, "TabIndex", 10);
    vb_control_set(logLabel, "Caption", "");
    vb_control_set_int(logLabel, "TabIndex", 11);

    /* The fixture code refers to the auto-generated names: keep them stable and loud. */
    FE_EXPECT([StringFromC(text1->name) isEqualToString:@"Text1"]);
    FE_EXPECT([StringFromC(text2->name) isEqualToString:@"Text2"]);
    FE_EXPECT([StringFromC(text3->name) isEqualToString:@"Text3"]);
    FE_EXPECT([StringFromC(command1->name) isEqualToString:@"Command1"]);
    FE_EXPECT([StringFromC(command2->name) isEqualToString:@"Command2"]);
    FE_EXPECT([StringFromC(command3->name) isEqualToString:@"Command3"]);
    FE_EXPECT([StringFromC(command8->name) isEqualToString:@"Command8"]);
    FE_EXPECT([StringFromC(logLabel->name) isEqualToString:@"Label1"]);
    logName = StringFromC(logLabel->name);
    {
        static const char *code =
            "Private Sub Text1_GotFocus()\n"
            "    Label1.Caption = Label1.Caption & \"got:Text1;\"\n"
            "End Sub\n\n"
            "Private Sub Text1_LostFocus()\n"
            "    Label1.Caption = Label1.Caption & \"lost:Text1;\"\n"
            "End Sub\n\n"
            "Private Sub Text2_GotFocus()\n"
            "    Label1.Caption = Label1.Caption & \"got:Text2;\"\n"
            "End Sub\n\n"
            "Private Sub Text2_LostFocus()\n"
            "    Label1.Caption = Label1.Caption & \"lost:Text2;\"\n"
            "End Sub\n\n"
            "Private Sub Text3_GotFocus()\n"
            "    Label1.Caption = Label1.Caption & \"got:Text3;\"\n"
            "End Sub\n\n"
            "Private Sub Command1_GotFocus()\n"
            "    Label1.Caption = Label1.Caption & \"got:Command1;\"\n"
            "End Sub\n\n"
            "Private Sub Command1_Click()\n"
            "    Text2.SetFocus\n"
            "End Sub\n\n"
            "Private Sub Command3_Click()\n"
            "    Call Text1.SetFocus\n"
            "End Sub\n\n"
            "Private Sub Command4_Click()\n"
            "    Command2.SetFocus\n"
            "End Sub\n\n"
            "Private Sub Command5_Click()\n"
            "    Label1.SetFocus\n"
            "End Sub\n\n"
            "Private Sub Command6_Click()\n"
            "    Text3.SetFocus\n"
            "End Sub\n\n"
            "Private Sub Command7_Click()\n"
            "    Command8.SetFocus\n"
            "End Sub\n\n"
            "Private Sub Command7_GotFocus()\n"
            "    Command8.SetFocus\n"
            "End Sub\n\n"
            "Private Sub Command8_GotFocus()\n"
            "    Label1.Caption = Label1.Caption & \"got:Command8;\"\n"
            "    Command7.SetFocus\n"
            "End Sub\n";
        NSString *text = [NSString stringWithFormat:@"%s\n%s", form->code ? form->code : "", code];
        free(form->code);
        form->code = strdup(text.UTF8String);
    }

    runtime = [[VBRuntimeWindow alloc] initWithForm:form];
    runtime.suppressDialogs = YES;
    [runtime startRuntime];
    _runtimeFocusTest = runtime;

    /* Form_Load runs first, then the first control gets the focus (GotFocus). */
    FE_EXPECT([[runtime focusedControlName] isEqualToString:@"Text1"]);
    log = [runtime propertyValueForControl:logName property:@"Caption"];
    FE_EXPECT([log isEqualToString:@"got:Text1;"]);
    fprintf(stderr, "runtime-focus: start focus=%s log=%s\n",
            [runtime focusedControlName].UTF8String, log.UTF8String);

    /* SetFocus from a Click handler: LostFocus of the old control, then GotFocus. */
    [runtime runEventHandler:@"Command1_Click"];
    log = [runtime propertyValueForControl:logName property:@"Caption"];
    FE_EXPECT([[runtime focusedControlName] isEqualToString:@"Text2"]);
    FE_EXPECT([log isEqualToString:@"got:Text1;lost:Text1;got:Text2;"]);
    fprintf(stderr, "runtime-focus: SetFocus -> %s log=%s\n",
            [runtime focusedControlName].UTF8String, log.UTF8String);

    /* "Call Text1.SetFocus" is the same method statement with the Call keyword. */
    [runtime runEventHandler:@"Command3_Click"];
    log = [runtime propertyValueForControl:logName property:@"Caption"];
    FE_EXPECT([[runtime focusedControlName] isEqualToString:@"Text1"]);
    FE_EXPECT([log isEqualToString:@"got:Text1;lost:Text1;got:Text2;lost:Text2;got:Text1;"]);
    fprintf(stderr, "runtime-focus: Call SetFocus -> %s\n",
            [runtime focusedControlName].UTF8String);

    /* SetFocus on a disabled control or on a Label changes nothing and fires no events. */
    log = [runtime propertyValueForControl:logName property:@"Caption"];
    [runtime runEventHandler:@"Command4_Click"];
    FE_EXPECT([[runtime focusedControlName] isEqualToString:@"Text1"]);
    FE_EXPECT([[runtime propertyValueForControl:logName property:@"Caption"] isEqualToString:log]);
    [runtime runEventHandler:@"Command5_Click"];
    FE_EXPECT([[runtime focusedControlName] isEqualToString:@"Text1"]);
    FE_EXPECT([[runtime propertyValueForControl:logName property:@"Caption"] isEqualToString:log]);
    fprintf(stderr, "runtime-focus: disabled/Label ignored, focus=%s\n",
            [runtime focusedControlName].UTF8String);

    /* TabStop = False keeps the control out of the Tab cycle but SetFocus still works. */
    FE_EXPECT(![[runtime tabOrderControlNames] containsObject:@"Text3"]);
    [runtime runEventHandler:@"Command6_Click"];
    FE_EXPECT([[runtime focusedControlName] isEqualToString:@"Text3"]);
    FE_EXPECT(![[runtime tabOrderControlNames] containsObject:@"Text3"]);
    log = [runtime propertyValueForControl:logName property:@"Caption"];
    FE_EXPECT([log hasSuffix:@"lost:Text1;got:Text3;"]);
    fprintf(stderr, "runtime-focus: TabStop=False focus=%s\n",
            [runtime focusedControlName].UTF8String);

    /* SetFocus on an unknown control is a no-op. */
    FE_EXPECT(![runtime setFocusToControl:@"NoSuchControl"]);
    FE_EXPECT([[runtime focusedControlName] isEqualToString:@"Text3"]);

    /* Access key -> Click -> SetFocus: the focus events fire along the whole chain. */
    [runtime sendTestKeyEvent:@"g" optionDown:YES];
    FE_EXPECT([[runtime focusedControlName] isEqualToString:@"Text2"]);
    log = [runtime propertyValueForControl:logName property:@"Caption"];
    FE_EXPECT([log rangeOfString:@"got:Command1;"].location != NSNotFound);
    FE_EXPECT([log hasSuffix:@"got:Command1;got:Text2;"]);
    fprintf(stderr, "runtime-focus: access key chain -> %s\n",
            [runtime focusedControlName].UTF8String);

    /* Two handlers that keep moving the focus: the nesting is capped, no hang. */
    [runtime runEventHandler:@"Command7_Click"];
    log = [runtime propertyValueForControl:logName property:@"Caption"];
    FE_EXPECT([log rangeOfString:@"got:Command8;"].location != NSNotFound);
    FE_EXPECT([[runtime focusedControlName] isEqualToString:@"Command7"] ||
              [[runtime focusedControlName] isEqualToString:@"Command8"]);
    fprintf(stderr, "runtime-focus: ping-pong stopped at %s\n",
            [runtime focusedControlName].UTF8String);

    /* Leave the fixture tidy for the snapshot. */
    [runtime setFocusToControl:@"Text1"];
    FE_EXPECT([[runtime focusedControlName] isEqualToString:@"Text1"]);

    fprintf(stderr, "runtime-focus: %s\n", (ok ? @"PASS" : @"FAIL").UTF8String);
    return ok;
}

#undef FE_EXPECT

#define ER_EXPECT(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "runtime-error: FAIL at line %d\n", __LINE__); \
        ok = NO; \
    } \
} while (0)

/* VB6 run-time error 5 ("Invalid procedure call or argument") when SetFocus cannot
   take the focus: End/Debug dialog, execution stops at the statement, Debug opens the
   code window on the offending line. */
- (BOOL)runRuntimeErrorSelfTest
{
    VbForm *form = (_currentItem && _currentItem->form) ? _currentItem->form : NULL;
    VBRuntimeWindow *runtime;
    NSString *originalCode;
    NSInteger errorsBefore;
    BOOL showCodeBefore;
    BOOL ok = YES;
    if (!form || !form->root) return NO;
    if (!vb_form_find_control(form, "Text1") || !vb_form_find_control(form, "Label1")) {
        fprintf(stderr, "runtime-error: fixture needs Text1 and Label1\n");
        return NO;
    }
    originalCode = StringFromC(form->code);

    /* Append the fixture and reload the code window: Debug must land on the same text. */
    {
        NSString *code = [NSString stringWithFormat:@"%s\n"
                          "Private Sub ErrorInvalid()\n"
                          "    Label1.SetFocus\n"
                          "    Label1.Caption = \"after\"\n"
                          "End Sub\n\n"
                          "Private Sub ErrorValid()\n"
                          "    Text1.SetFocus\n"
                          "    Label1.Caption = \"valid\"\n"
                          "End Sub\n",
                          form->code ? form->code : ""];
        free(form->code);
        form->code = strdup(code.UTF8String);
    }
    [self selectItem:_currentItem];

    runtime = [[VBRuntimeWindow alloc] initWithForm:form];
    runtime.suppressDialogs = YES;
    runtime.suppressedErrorResult = 1; /* End */
    runtime.delegate = self;
    [runtime startRuntime];
    _runtimeErrorTest = runtime;

    errorsBefore = runtime.runtimeErrorCount;
    showCodeBefore = _showCode;
    [runtime runEventHandler:@"ErrorInvalid"];
    ER_EXPECT(runtime.runtimeErrorCount == errorsBefore + 1);
    ER_EXPECT(runtime.lastRuntimeErrorNumber == 5);
    ER_EXPECT([runtime.lastRuntimeErrorMessage isEqualToString:
               @"Run-time error '5':\n\nInvalid procedure call or argument"]);
    ER_EXPECT([runtime.lastRuntimeErrorHandler isEqualToString:@"ErrorInvalid"]);
    ER_EXPECT(runtime.lastRuntimeErrorLine > 0);
    /* The statement after the failing SetFocus must not run. */
    ER_EXPECT([[runtime propertyValueForControl:@"Label1" property:@"Caption"]
               isEqualToString:@"Type your name"]);
    ER_EXPECT([[runtime focusedControlName] isEqualToString:@"Text1"]);
    /* End does not open the code window. */
    ER_EXPECT(_showCode == showCodeBefore);
    fprintf(stderr, "runtime-error: error %ld at %s line %ld -> End\n",
            (long)runtime.lastRuntimeErrorNumber,
            runtime.lastRuntimeErrorHandler.UTF8String,
            (long)runtime.lastRuntimeErrorLine);

    /* A valid SetFocus raises nothing. */
    [runtime runEventHandler:@"ErrorValid"];
    ER_EXPECT(runtime.runtimeErrorCount == errorsBefore + 1);
    ER_EXPECT([[runtime propertyValueForControl:@"Label1" property:@"Caption"] isEqualToString:@"valid"]);

    /* Debug stops at the same statement and opens the code window on its line. */
    runtime.suppressedErrorResult = 2; /* Debug */
    [runtime runEventHandler:@"ErrorInvalid"];
    ER_EXPECT(runtime.runtimeErrorCount == errorsBefore + 2);
    ER_EXPECT([[runtime propertyValueForControl:@"Label1" property:@"Caption"] isEqualToString:@"valid"]);
    ER_EXPECT(_showCode);
    ER_EXPECT([_codeEditor currentLineNumber] == runtime.lastRuntimeErrorLine);
    fprintf(stderr, "runtime-error: Debug opened code at line %ld\n",
            (long)[_codeEditor currentLineNumber]);

    /* Restore the project form and the code window. */
    free(form->code);
    form->code = strdup(originalCode.UTF8String);
    if (_currentItem) [self selectItem:_currentItem];

    fprintf(stderr, "runtime-error: %s\n", (ok ? @"PASS" : @"FAIL").UTF8String);
    return ok;
}

/* Posts a real key event; while paused the break loop's local monitor swallows it. */
static void PostKeyDown(NSInteger windowNumber, unsigned short keyCode)
{
    NSEvent *event = [NSEvent keyEventWithType:NSEventTypeKeyDown
                                     location:NSZeroPoint
                                modifierFlags:0
                                   timestamp:[NSDate timeIntervalSinceReferenceDate]
                                windowNumber:windowNumber
                                     context:nil
                                  characters:@""
                 charactersIgnoringModifiers:@""
                                   isARepeat:NO
                                     keyCode:keyCode];
    if (event) [NSApp postEvent:event atStart:NO];
}

static BOOL WriteWindowContentPNG(NSWindow *window, NSString *path)
{
    NSView *view = window.contentView;
    NSBitmapImageRep *rep;
    NSData *data;
    if (!view || !path.length) return NO;
    [window displayIfNeeded];
    [view layoutSubtreeIfNeeded];
    [view displayIfNeeded];
    rep = [view bitmapImageRepForCachingDisplayInRect:view.bounds];
    if (!rep) return NO;
    [view cacheDisplayInRect:view.bounds toBitmapImageRep:rep];
    data = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    if (![data writeToFile:path atomically:YES]) return NO;
    fprintf(stderr, "snapshot: %s (%lu bytes)\n", path.UTF8String,
            (unsigned long)data.length);
    return YES;
}

#define BR_EXPECT(...) do { \
    if (!(__VA_ARGS__)) { \
        fprintf(stderr, "runtime-break: FAIL at line %d\n", __LINE__); \
        ok = NO; \
    } \
} while (0)

/* Break mode: Run > Break pauses before the next statement, F8 executes one statement
   and pauses again, F5 runs on, and the code window highlights the paused line. Debug
   in the End/Debug dialog starts break mode on the failing statement, which keeps the
   procedure alive so the rest can be stepped through. */
- (BOOL)runRuntimeBreakSelfTest:(NSString *)snapshotPath
{
    VbForm *form = (_currentItem && _currentItem->form) ? _currentItem->form : NULL;
    VBRuntimeWindow *runtime;
    NSString *originalCode;
    __block BOOL ok = YES;
    __block NSInteger pauses = 0;
    NSMutableArray<NSNumber *> *lines = [NSMutableArray array];
    NSMutableArray<NSString *> *captions = [NSMutableArray array];
    if (!form || !form->root) return NO;
    if (!vb_form_find_control(form, "Label1")) {
        fprintf(stderr, "runtime-break: fixture needs Label1\n");
        return NO;
    }
    originalCode = StringFromC(form->code);

    /* Key map: F8/F10 step, F5 continues, Shift+F5 stops, everything else is passed
       through to the code window. */
    BR_EXPECT([VBRuntimeWindow breakActionForKeyCode:100 shift:NO] == VBRuntimeBreakStep);
    BR_EXPECT([VBRuntimeWindow breakActionForKeyCode:109 shift:NO] == VBRuntimeBreakStep);
    BR_EXPECT([VBRuntimeWindow breakActionForKeyCode:96 shift:NO] == VBRuntimeBreakContinue);
    BR_EXPECT([VBRuntimeWindow breakActionForKeyCode:96 shift:YES] == VBRuntimeBreakStop);
    BR_EXPECT([VBRuntimeWindow breakActionForKeyCode:36 shift:NO] < 0);

    {
        NSString *code = [NSString stringWithFormat:@"%s\n"
                          "Private Sub StepThrough()\n"
                          "    Label1.Caption = \"one\"\n"
                          "    Label1.Caption = Label1.Caption & \":two\"\n"
                          "    Label1.Caption = Label1.Caption & \":three\"\n"
                          "End Sub\n\n"
                          "Private Sub DebugStep()\n"
                          "    Label1.Caption = \"before\"\n"
                          "    Label1.SetFocus\n"
                          "    Label1.Caption = \"after\"\n"
                          "End Sub\n",
                          form->code ? form->code : ""];
        free(form->code);
        form->code = strdup(code.UTF8String);
    }
    [self selectItem:_currentItem];

    runtime = [[VBRuntimeWindow alloc] initWithForm:form];
    runtime.suppressDialogs = YES;
    runtime.suppressedErrorResult = VB_RUNTIME_ERROR_DEBUG;
    runtime.delegate = self;
    _runtimeBreakTest = runtime;
    [runtime startRuntime];
    BR_EXPECT(runtime.runtimeErrorCount == 0);

    /* The first pause happens before the first statement, so it still shows whatever
       Form_Load left in the label. */
    NSString *initialCaption = [runtime propertyValueForControl:@"Label1" property:@"Caption"] ?: @"";
    __weak VBEditorWindowController *weakSelf = self;
    runtime.breakAction = ^VBRuntimeBreakAction(VBRuntimeWindow *window, NSString *handler,
                                                NSInteger line) {
        VBEditorWindowController *editor = weakSelf;
        pauses++;
        [lines addObject:@(line)];
        [captions addObject:[window propertyValueForControl:@"Label1" property:@"Caption"] ?: @""];
        BR_EXPECT(window.isInBreakMode);
        BR_EXPECT(window.breakLine == line);
        BR_EXPECT([window.breakHandler isEqualToString:handler]);
        /* The IDE reacted before the pause: code window visible, line highlighted. */
        BR_EXPECT(editor != nil);
        BR_EXPECT(editor->_showCode);
        BR_EXPECT([editor->_codeEditor debugLine] == line);
        if (pauses == 1 && snapshotPath.length) {
            BR_EXPECT(WriteWindowContentPNG(editor.window, snapshotPath));
        }
        return pauses < 3 ? VBRuntimeBreakStep : VBRuntimeBreakContinue;
    };

    /* Run > Break, then F8 twice and F5: three pauses, one more statement each time. */
    [runtime requestBreak];
    [runtime runEventHandler:@"StepThrough"];
    BR_EXPECT(pauses == 3);
    BR_EXPECT([lines count] == 3);
    BR_EXPECT(lines.count == 3 && lines[1].integerValue == lines[0].integerValue + 1 &&
              lines[2].integerValue == lines[1].integerValue + 1);
    BR_EXPECT([captions isEqualToArray:@[initialCaption, @"one", @"one:two"]]);
    BR_EXPECT([[runtime propertyValueForControl:@"Label1" property:@"Caption"]
               isEqualToString:@"one:two:three"]);
    BR_EXPECT(runtime.runtimeErrorCount == 0);
    BR_EXPECT(!runtime.isInBreakMode);
    BR_EXPECT(runtime.breakCount == 3);
    BR_EXPECT(runtime.breakLine == lines.lastObject.integerValue);
    BR_EXPECT([runtime.breakHandler isEqualToString:@"StepThrough"]);
    /* Leaving break mode clears the bar in the editor. */
    BR_EXPECT([_codeEditor debugLine] == 0);
    fprintf(stderr, "runtime-break: step pauses=%ld lines=(%ld,%ld,%ld) caption=%s\n",
            (long)pauses, (long)lines[0].integerValue, (long)lines[1].integerValue,
            (long)lines[2].integerValue,
            [runtime propertyValueForControl:@"Label1" property:@"Caption"].UTF8String);

    /* Stop (Shift+F5) must not execute the statement it is paused on. */
    pauses = 0;
    [lines removeAllObjects];
    [captions removeAllObjects];
    runtime.breakAction = ^VBRuntimeBreakAction(VBRuntimeWindow *window, NSString *handler,
                                                NSInteger line) {
        (void)window;
        (void)handler;
        (void)line;
        pauses++;
        return VBRuntimeBreakStop;
    };
    [runtime requestBreak];
    [runtime runEventHandler:@"StepThrough"];
    BR_EXPECT(pauses == 1);
    BR_EXPECT([[runtime propertyValueForControl:@"Label1" property:@"Caption"]
               isEqualToString:@"one:two:three"]);
    BR_EXPECT(!runtime.isInBreakMode);
    fprintf(stderr, "runtime-break: stop pauses=%ld caption=%s\n", (long)pauses,
            [runtime propertyValueForControl:@"Label1" property:@"Caption"].UTF8String);

    /* Debug on an unhandled error: paused on the failing line, then one step runs the
       rest of the handler instead of stopping the procedure at the error. */
    pauses = 0;
    [lines removeAllObjects];
    [captions removeAllObjects];
    runtime.breakAction = ^VBRuntimeBreakAction(VBRuntimeWindow *window, NSString *handler,
                                                NSInteger line) {
        (void)handler;
        pauses++;
        [lines addObject:@(line)];
        [captions addObject:[window propertyValueForControl:@"Label1" property:@"Caption"] ?: @""];
        return VBRuntimeBreakStep;
    };
    NSInteger errorsBefore = runtime.runtimeErrorCount;
    [runtime runEventHandler:@"DebugStep"];
    BR_EXPECT(pauses == 2);
    BR_EXPECT(runtime.runtimeErrorCount == errorsBefore + 1);
    BR_EXPECT(runtime.lastRuntimeErrorNumber == 5);
    BR_EXPECT(lines.count == 2 && lines[0].integerValue == runtime.lastRuntimeErrorLine);
    BR_EXPECT(lines.count == 2 && lines[1].integerValue == lines[0].integerValue + 1);
    BR_EXPECT([captions isEqualToArray:@[@"before", @"before"]]);
    BR_EXPECT([[runtime propertyValueForControl:@"Label1" property:@"Caption"]
               isEqualToString:@"after"]);
    fprintf(stderr, "runtime-break: debug paused at %ld (error %ld), next %ld, caption=%s\n",
            (long)lines[0].integerValue, (long)runtime.lastRuntimeErrorLine,
            (long)lines[1].integerValue,
            [runtime propertyValueForControl:@"Label1" property:@"Caption"].UTF8String);

    /* Interactive path: with no breakAction the runtime enters its own event loop. Two
       posted F8 keys must step twice and F5 must run to the end; the timers below only
       fire because that nested loop drains the main run loop. */
    {
        NSInteger breaksBefore = runtime.breakCount;
        runtime.breakAction = nil;
        [NSTimer scheduledTimerWithTimeInterval:0.15 repeats:NO block:^(NSTimer *timer) {
            (void)timer;
            PostKeyDown(runtime.window.windowNumber, 100); /* F8 */
        }];
        [NSTimer scheduledTimerWithTimeInterval:0.35 repeats:NO block:^(NSTimer *timer) {
            (void)timer;
            PostKeyDown(runtime.window.windowNumber, 100); /* F8 */
        }];
        [NSTimer scheduledTimerWithTimeInterval:0.55 repeats:NO block:^(NSTimer *timer) {
            (void)timer;
            PostKeyDown(runtime.window.windowNumber, 96); /* F5: continue */
        }];
        /* Watchdog: never let a missed key hang the self test (or CI). */
        [NSTimer scheduledTimerWithTimeInterval:5.0 repeats:NO block:^(NSTimer *timer) {
            (void)timer;
            fprintf(stderr, "runtime-break: interactive keys timed out\n");
            [runtime breakContinue];
        }];
        [runtime requestBreak];
        [runtime runEventHandler:@"StepThrough"];
        BR_EXPECT(runtime.breakCount == breaksBefore + 3);
        BR_EXPECT([[runtime propertyValueForControl:@"Label1" property:@"Caption"]
                   isEqualToString:@"one:two:three"]);
        BR_EXPECT(!runtime.isInBreakMode);
        BR_EXPECT([_codeEditor debugLine] == 0);
        fprintf(stderr, "runtime-break: interactive F8/F8/F5 pauses=%ld caption=%s\n",
                (long)(runtime.breakCount - breaksBefore),
                [runtime propertyValueForControl:@"Label1" property:@"Caption"].UTF8String);
    }

    runtime.breakAction = nil;
    free(form->code);
    form->code = strdup(originalCode.UTF8String);
    if (_currentItem) [self selectItem:_currentItem];

    fprintf(stderr, "runtime-break: %s\n", (ok ? @"PASS" : @"FAIL").UTF8String);
    return ok;
}

#undef BR_EXPECT

#define BP_EXPECT(...) do { \
    if (!(__VA_ARGS__)) { \
        fprintf(stderr, "breakpoints: FAIL at line %d\n", __LINE__); \
        ok = NO; \
    } \
} while (0)

- (BOOL)runCompletionSelfTest
{
    VBCodeEditor *editor = [[VBCodeEditor alloc] initWithFrame:NSMakeRect(0, 0, 600, 400)];
    [editor setCode:@"Private Sub Command1_Click()\nDim Counter AS Integer\nEnd Sub\nPrivate Sub SaveFile()\nEnd Sub\n"];
    BOOL ok = YES;
#define COMP_EXPECT(prefix, want) do { \
    NSArray<NSString *> *m = [editor completionMatchesForPrefix:prefix]; \
    BOOL found = NO; \
    for (NSString *s in m) if ([s caseInsensitiveCompare:want] == NSOrderedSame) { found = YES; break; } \
    if (!found) { fprintf(stderr, "completion: FAIL %s -> %s (got %lu)\n", prefix.UTF8String, want.UTF8String, (unsigned long)m.count); ok = NO; } \
} while (0)
    COMP_EXPECT(@"Cou", @"Counter");
    COMP_EXPECT(@"com", @"Command1");
    COMP_EXPECT(@"Msg", @"MsgBox");
    COMP_EXPECT(@"tabi", @"TabIndex");
    COMP_EXPECT(@"save", @"SaveFile");
    editor.gmbLanguage = YES;
    COMP_EXPECT(@"NAM", @"NAMESPACE");
    COMP_EXPECT(@"SCRE", @"SCREEN");
#undef COMP_EXPECT
    fprintf(stderr, "completion: %s\n", ok ? "PASS" : "FAIL");
    return ok;
}

/* Breakpoints: a gutter click sets a dark red dot only on executable statements, the
   line is stored in the form and survives a save/load through the .frm comment, and the
   interpreter stops on it. */
- (BOOL)runBreakpointSelfTest:(NSString *)snapshotPath
{
    VbForm *form = (_currentItem && _currentItem->form) ? _currentItem->form : NULL;
    VBRuntimeWindow *runtime = nil;
    NSString *originalCode;
    NSMutableArray<NSNumber *> *originalLines = [NSMutableArray array];
    NSInteger originalCount;
    __block BOOL ok = YES;
    __block NSInteger pauses = 0;
    __block NSInteger pausedLine = 0;
    __block NSString *pausedCaption = nil;
    NSInteger bodyLine = 0;
    NSInteger targetLine;
    NSInteger refusedLine;
    const char *firstPath = "/tmp/gmbaside-breakpoints-1.frm";
    const char *secondPath = "/tmp/gmbaside-breakpoints-2.frm";
    if (!form || !form->root) return NO;
    if (!vb_form_find_control(form, "Label1")) {
        fprintf(stderr, "breakpoints: fixture needs Label1\n");
        return NO;
    }
    originalCode = StringFromC(form->code);
    originalCount = vb_form_breakpoint_count(form);
    for (NSInteger i = 0; i < originalCount; i++) {
        [originalLines addObject:@(vb_form_breakpoint_at(form, (int)i))];
    }

    {
        NSString *code = [NSString stringWithFormat:@"%s\n"
                          "Private Sub BreakpointHit()\n"
                          "    Label1.Caption = \"first\"\n"
                          "    Label1.Caption = Label1.Caption & \":second\"\n"
                          "    Label1.Caption = Label1.Caption & \":third\"\n"
                          "End Sub\n",
                          form->code ? form->code : ""];
        free(form->code);
        form->code = strdup(code.UTF8String);
    }
    vb_form_clear_breakpoints(form);
    [self selectItem:_currentItem];
    [_codeEditor setBreakpointLines:nil];

    {
        VbHandlerList handlers;
        vb_handler_list_init(&handlers);
        vb_code_handlers(form->code, &handlers);
        for (int i = 0; i < handlers.count; i++) {
            if (vb_streq_ci(handlers.items[i].name, "BreakpointHit")) {
                bodyLine = handlers.items[i].body_line;
            }
        }
        vb_handler_list_free(&handlers);
    }
    if (bodyLine <= 0) {
        fprintf(stderr, "breakpoints: handler line not found\n");
        free(form->code);
        form->code = strdup(originalCode.UTF8String);
        vb_form_clear_breakpoints(form);
        for (NSNumber *line in originalLines) vb_form_set_breakpoint(form, line.intValue, 1);
        if (_currentItem) [self selectItem:_currentItem];
        fprintf(stderr, "breakpoints: FAIL\n");
        return NO;
    }
    targetLine = bodyLine + 1;   /* second statement */
    refusedLine = bodyLine + 3;  /* "End Sub" is not executable */

    /* 1. A gutter click sets the dot on the right line and reaches the form model. */
    [self showCode];
    [_codeEditor layoutSubtreeIfNeeded];
    BP_EXPECT([_codeEditor toggleBreakpointAtGutterPoint:
               [_codeEditor gutterPointForLine:targetLine]]);
    BP_EXPECT([_codeEditor.breakpointLines containsIndex:(NSUInteger)targetLine]);
    BP_EXPECT(vb_form_has_breakpoint(form, (int)targetLine));
    BP_EXPECT(vb_form_breakpoint_count(form) == 1);
    if (snapshotPath.length) BP_EXPECT(WriteWindowContentPNG(self.window, snapshotPath));
    fprintf(stderr, "breakpoints: set %ld (body %ld)\n", (long)targetLine, (long)bodyLine);

    /* 2. Non-executable lines are refused (VB6 only marks statements). */
    BP_EXPECT([_codeEditor toggleBreakpointAtGutterPoint:
               [_codeEditor gutterPointForLine:refusedLine]]);
    BP_EXPECT(![_codeEditor.breakpointLines containsIndex:(NSUInteger)refusedLine]);
    BP_EXPECT(vb_form_breakpoint_count(form) == 1);
    fprintf(stderr, "breakpoints: refused line %ld\n", (long)refusedLine);

    /* 3. The .frm keeps it as a trailing comment, and save/load/save is byte-stable. */
    {
        size_t len = 0;
        char *data = vb_form_serialize(form, &len);
        NSString *text;
        VbForm *reloaded;
        BP_EXPECT(data != NULL);
        BP_EXPECT(data && vb_write_file(firstPath, data, len));
        text = data ? [NSString stringWithUTF8String:data] : @"";
        free(data);
        BP_EXPECT([text containsString:@(VB_BREAKPOINT_MARKER)]);
        BP_EXPECT([text containsString:[NSString stringWithFormat:@"%@: %ld",
                                        @(VB_BREAKPOINT_MARKER), (long)targetLine]]);
        reloaded = vb_form_load(firstPath);
        BP_EXPECT(reloaded != NULL);
        BP_EXPECT(vb_form_breakpoint_count(reloaded) == 1);
        BP_EXPECT(vb_form_has_breakpoint(reloaded, (int)targetLine));
        /* The marker is metadata, not code: it never reaches the editor text. */
        BP_EXPECT(reloaded && strstr(reloaded->code, VB_BREAKPOINT_MARKER) == NULL);
        BP_EXPECT(reloaded && vb_streq(reloaded->code, form->code));
        BP_EXPECT(reloaded && vb_form_save(reloaded, secondPath));
        {
            NSData *first = [NSData dataWithContentsOfFile:@(firstPath)];
            NSData *second = [NSData dataWithContentsOfFile:@(secondPath)];
            BP_EXPECT(first.length > 0 && [first isEqualToData:second]);
        }
        vb_form_free(reloaded);
    }
    fprintf(stderr, "breakpoints: saved to %s\n", firstPath);

    /* 4. The interpreter stops on the breakpoint and the IDE highlights the line. */
    runtime = [[VBRuntimeWindow alloc] initWithForm:form];
    runtime.delegate = self;
    _runtimeBreakpointTest = runtime;
    [runtime startRuntime];
    runtime.breakpointLines = [_codeEditor breakpointLines];
    runtime.breakAction = ^VBRuntimeBreakAction(VBRuntimeWindow *window, NSString *handler,
                                                NSInteger line) {
        (void)handler;
        pauses++;
        pausedLine = line;
        pausedCaption = [[window propertyValueForControl:@"Label1" property:@"Caption"] copy] ?: @"";
        BP_EXPECT(window.isInBreakMode);
        BP_EXPECT([window.breakHandler isEqualToString:@"BreakpointHit"]);
        BP_EXPECT([_codeEditor debugLine] == line);
        fprintf(stderr, "breakpoints: paused at %ld caption=%s\n", (long)line,
                pausedCaption.UTF8String);
        return VBRuntimeBreakContinue;
    };
    [runtime runEventHandler:@"BreakpointHit"];
    BP_EXPECT(pauses == 1);
    BP_EXPECT(pausedLine == targetLine);
    /* The breakpoint statement has not run yet: only "first" is on the label. */
    BP_EXPECT([pausedCaption isEqualToString:@"first"]);
    BP_EXPECT([[runtime propertyValueForControl:@"Label1" property:@"Caption"]
               isEqualToString:@"first:second:third"]);
    BP_EXPECT(runtime.breakCount == 1);
    BP_EXPECT(!runtime.isInBreakMode);
    BP_EXPECT([_codeEditor debugLine] == 0);
    runtime.breakAction = nil;
    fprintf(stderr, "breakpoints: run pauses=%ld line=%ld caption=%s\n",
            (long)pauses, (long)pausedLine,
            [runtime propertyValueForControl:@"Label1" property:@"Caption"].UTF8String);

    /* 5. A second click clears it again, in the editor, the form and the saved file. */
    BP_EXPECT([_codeEditor toggleBreakpointAtGutterPoint:
               [_codeEditor gutterPointForLine:targetLine]]);
    BP_EXPECT(_codeEditor.breakpointLines.count == 0);
    BP_EXPECT(vb_form_breakpoint_count(form) == 0);
    {
        size_t len = 0;
        char *data = vb_form_serialize(form, &len);
        BP_EXPECT(data != NULL);
        BP_EXPECT(data && strstr(data, VB_BREAKPOINT_MARKER) == NULL);
        free(data);
    }
    fprintf(stderr, "breakpoints: cleared line %ld\n", (long)targetLine);

    /* 6. Editing moves the dots with their statements, as VB6 does, instead of leaving
       them glued to a line number. */
    {
        NSInteger shifted = targetLine;
        BP_EXPECT([_codeEditor toggleBreakpointAtGutterPoint:
                   [_codeEditor gutterPointForLine:shifted]]);
        BP_EXPECT(vb_form_has_breakpoint(form, (int)shifted));

        /* A line inserted above everything pushes the statement and its dot down. */
        BP_EXPECT([_codeEditor replaceCharactersInRange:NSMakeRange(0, 0) withString:@"\n"]);
        BP_EXPECT([_codeEditor.breakpointLines containsIndex:(NSUInteger)shifted + 1]);
        BP_EXPECT(vb_form_has_breakpoint(form, (int)(shifted + 1)));
        BP_EXPECT(vb_form_breakpoint_count(form) == 1);
        /* The marker saved in the .frm follows the statement too. */
        {
            size_t len = 0;
            char *data = vb_form_serialize(form, &len);
            NSString *text = data ? [NSString stringWithUTF8String:data] : @"";
            (void)len;
            BP_EXPECT([text containsString:[NSString stringWithFormat:@"%@: %ld",
                                            @(VB_BREAKPOINT_MARKER), (long)(shifted + 1)]]);
            free(data);
        }
        fprintf(stderr, "breakpoints: insert above -> line %ld\n", (long)(shifted + 1));

        /* Enter at column 0 of the statement: the statement moves down, the dot follows. */
        {
            NSRange line = [_codeEditor rangeForLine:shifted + 1];
            BP_EXPECT(line.location != NSNotFound);
            BP_EXPECT([_codeEditor replaceCharactersInRange:NSMakeRange(line.location, 0)
                                                withString:@"\n"]);
        }
        BP_EXPECT([_codeEditor.breakpointLines containsIndex:(NSUInteger)shifted + 2]);
        fprintf(stderr, "breakpoints: enter at column 0 -> line %ld\n", (long)(shifted + 2));

        /* Deleting the blank line above brings the dot back with its statement. */
        {
            NSRange blank = [_codeEditor rangeForLine:1];
            BP_EXPECT(blank.location != NSNotFound);
            BP_EXPECT([_codeEditor replaceCharactersInRange:blank withString:@""]);
        }
        BP_EXPECT([_codeEditor.breakpointLines containsIndex:(NSUInteger)shifted + 1]);
        {
            NSRange blank = [_codeEditor rangeForLine:shifted];
            BP_EXPECT(blank.location != NSNotFound);
            BP_EXPECT([_codeEditor replaceCharactersInRange:blank withString:@""]);
        }
        BP_EXPECT([_codeEditor.breakpointLines containsIndex:(NSUInteger)shifted]);
        BP_EXPECT(vb_form_has_breakpoint(form, (int)shifted));
        fprintf(stderr, "breakpoints: delete above -> line %ld\n", (long)shifted);

        /* Adding code below the statement leaves the dot where it is. */
        {
            NSUInteger end = _codeEditor.code.length;
            BP_EXPECT([_codeEditor replaceCharactersInRange:NSMakeRange(end, 0) withString:@"\n' note\n"]);
        }
        BP_EXPECT([_codeEditor.breakpointLines containsIndex:(NSUInteger)shifted]);
        fprintf(stderr, "breakpoints: edit below keeps line %ld\n", (long)shifted);

        /* Typing on the statement itself keeps the dot on that line. */
        {
            NSRange line = [_codeEditor rangeForLine:shifted];
            BP_EXPECT(line.location != NSNotFound);
            BP_EXPECT([_codeEditor replaceCharactersInRange:NSMakeRange(line.location, 0)
                                                withString:@" "]);
        }
        BP_EXPECT([_codeEditor.breakpointLines containsIndex:(NSUInteger)shifted]);
        {
            NSRange line = [_codeEditor rangeForLine:shifted];
            BP_EXPECT([_codeEditor replaceCharactersInRange:NSMakeRange(line.location, 1) withString:@""]);
        }
        BP_EXPECT([_codeEditor.breakpointLines containsIndex:(NSUInteger)shifted]);

        /* Deleting the statement takes the dot away, as in VB6. */
        {
            NSRange line = [_codeEditor rangeForLine:shifted];
            BP_EXPECT(line.location != NSNotFound);
            BP_EXPECT([_codeEditor replaceCharactersInRange:line withString:@""]);
        }
        BP_EXPECT(_codeEditor.breakpointLines.count == 0);
        BP_EXPECT(vb_form_breakpoint_count(form) == 0);
        fprintf(stderr, "breakpoints: deleting the statement drops the dot\n");
    }

    runtime.breakAction = nil;
    free(form->code);
    form->code = strdup(originalCode.UTF8String);
    vb_form_clear_breakpoints(form);
    for (NSNumber *line in originalLines) vb_form_set_breakpoint(form, line.intValue, 1);
    if (_currentItem) [self selectItem:_currentItem];

    fprintf(stderr, "breakpoints: %s\n", (ok ? @"PASS" : @"FAIL").UTF8String);
    return ok;
}

#undef BP_EXPECT

- (void)showRuntimeErrorDialogForSnapshot
{
    VbForm *form = vb_form_new("RuntimeError");
    VBRuntimeWindow *runtime;
    if (!form || !form->root) return;
    vb_form_add_control(form, form->root, "VB.Label", 240, 240, 3000, 300);
    free(form->code);
    form->code = strdup("Attribute VB_Name = \"RuntimeError\"\n\n"
                        "Private Sub Invalid()\n"
                        "    Label1.SetFocus\n"
                        "End Sub\n");
    runtime = [[VBRuntimeWindow alloc] initWithForm:form];
    _runtimeErrorSnapshot = runtime;
    [runtime startRuntime];
    /* Blocks in the modal dialog: the snapshot caller aborts the modal. */
    [runtime runEventHandler:@"Invalid"];
}

#undef ER_EXPECT

- (void)designerSelectionChanged:(VBDesignerView *)designer
{
    [_properties setControl:designer.selected];
    [_properties setObjects:[self propertyObjects]];
    [self updateStatus];
    [self updateToolbarState];
    [self updateFormatMenuState];
}

- (NSArray<NSDictionary *> *)propertyObjects
{
    NSMutableArray *objects = [NSMutableArray array];
    VbForm *form = (_currentItem && _currentItem->form) ? _currentItem->form : NULL;
    if (!form || !form->root) return objects;
    NSMutableArray *controls = [NSMutableArray array];
    CollectControls(form->root, controls);
    for (NSValue *value in controls) {
        VbControl *control = (VbControl *)value.pointerValue;
        NSString *name = control->name ? [NSString stringWithUTF8String:control->name] : @"";
        NSString *type = [StringFromC(control->type) stringByReplacingOccurrencesOfString:@"VB." withString:@""];
        NSString *title = control == form->root ? name : [NSString stringWithFormat:@"%@  %@", name, type];
        [objects addObject:@{@"title": title ?: @"", @"value": value}];
    }
    return objects;
}

- (void)propertyPanel:(VBPropertyPanel *)panel didSelectControl:(VbControl *)control
{
    (void)panel;
    if (!control) return;
    [_designer selectControl:control];
    [_properties setControl:control];
    [self updateStatus];
}

- (void)designerDidModifyForm:(VBDesignerView *)designer
{
    (void)designer;
    if (_currentItem && _currentItem->form) _currentItem->form->dirty = 1;
    _projectDirty = YES;
    [_properties reload];
    [self updateTitle];
    [self updateStatus];
}

- (void)designerDidFinishCreate:(VBDesignerView *)designer
{
    (void)designer;
    [_toolbox clearSelection];
    [_properties setControl:designer.selected];
    [self updateStatus];
}

- (void)designer:(VBDesignerView *)designer requestCodeForControl:(VbControl *)control
{
    (void)designer;
    if (!control || !_currentItem || !_currentItem->form) return;
    [self showCode];
    if (control == _currentItem->form->root) {
        [_codeEditor insertHandlerForControl:@"Form" event:@"Load"];
        return;
    }
    NSString *type = StringFromC(control->type);
    NSString *event = @"Click";
    if ([type isEqualToString:@"VB.TextBox"]) event = @"Change";
    else if ([type isEqualToString:@"VB.Timer"]) event = @"Timer";
    else if ([type isEqualToString:@"VB.Form"]) event = @"Load";
    [_codeEditor insertHandlerForControl:StringFromC(control->name) event:event];
}

#pragma mark - Code editor

- (void)codeEditorDidChange:(VBCodeEditor *)editor
{
    if (!_currentItem) return;
    NSString *code = editor.code;
    if (_currentItem->kind == VB_ITEM_FORM && _currentItem->form) {
        free(_currentItem->form->code);
        _currentItem->form->code = vb_strdup(code.UTF8String);
        _currentItem->form->dirty = 1;
    } else {
        NSValue *key = [NSValue valueWithPointer:_currentItem];
        _moduleCodes[key] = [code copy];
        [_dirtyModules addObject:key];
    }
    _projectDirty = YES;
    [self updateTitle];
}

/* Breakpoints live in the form model so they are written to the .frm; the gutter click
   in the editor mirrors its set into the form the editor is showing. */
- (void)codeEditorDidChangeBreakpoints:(VBCodeEditor *)editor
{
    VbForm *form = (_currentItem && _currentItem->form) ? _currentItem->form : NULL;
    if (!form) return;
    vb_form_clear_breakpoints(form);
    [editor.breakpointLines enumerateIndexesUsingBlock:^(NSUInteger index, BOOL *stop) {
        (void)stop;
        vb_form_set_breakpoint(form, (int)index, 1);
    }];
    form->dirty = 1;
    _projectDirty = YES;
    [self updateTitle];
    fprintf(stderr, "breakpoints: %d in %s\n", vb_form_breakpoint_count(form),
            StringFromC(vb_form_name(form)).UTF8String);
}

#pragma mark - Properties

- (BOOL)propertyPanel:(VBPropertyPanel *)panel setValue:(NSString *)value forKey:(NSString *)key
{
    VbControl *control = panel.control;
    VbForm *form = (_currentItem && _currentItem->form) ? _currentItem->form : NULL;
    if (!control || !form) return NO;

    if ([key isEqualToString:@"Name"]) {
        NSString *trimmed = [value stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]];
        if (!trimmed.length || [trimmed rangeOfCharacterFromSet:[NSCharacterSet whitespaceCharacterSet]].location != NSNotFound) {
            NSBeep();
            return NO;
        }
        VbControl *existing = vb_form_find_control(form, trimmed.UTF8String);
        if (existing && existing != control) {
            NSBeep();
            return NO;
        }
        free(control->name);
        control->name = vb_strdup(trimmed.UTF8String);
        if (control == form->root) {
            [self rewriteFormName:trimmed form:form];
        }
    } else if ([key isEqualToString:@"TabIndex"]) {
        if (!vb_value_is_numeric(value.UTF8String)) {
            NSBeep();
            return NO;
        }
        vb_control_set_tab_index(control, atoi(value.UTF8String));
        [_designer refresh];
    } else if (IsBooleanKey(key)) {
        BOOL on = !([value isEqualToString:@"0"] || [value caseInsensitiveCompare:@"False"] == NSOrderedSame ||
                    value.length == 0);
        vb_control_set_int(control, key.UTF8String, on ? -1 : 0);
    } else {
        if (IsIntegerKey(key) && !vb_value_is_numeric(value.UTF8String)) {
            NSBeep();
            return NO;
        }
        vb_control_set_display(control, key.UTF8String, value.UTF8String);
    }

    form->dirty = 1;
    _projectDirty = YES;
    [_designer refresh];
    [_properties reload];
    [self updateTitle];
    [self updateStatus];
    return YES;
}

- (void)rewriteFormName:(NSString *)name form:(VbForm *)form
{
    if (!form->code) return;
    NSString *code = StringFromC(form->code);
    NSRegularExpression *regex = [NSRegularExpression regularExpressionWithPattern:@"Attribute VB_Name\\s*=\\s*\"[^\"]*\""
                                                                           options:0
                                                                             error:nil];
    NSString *replacement = [NSString stringWithFormat:@"Attribute VB_Name = \"%@\"", name];
    NSString *updated = [regex stringByReplacingMatchesInString:code
                                                        options:0
                                                          range:NSMakeRange(0, code.length)
                                                   withTemplate:replacement];
    if (![updated isEqualToString:code]) {
        free(form->code);
        form->code = vb_strdup(updated.UTF8String);
        form->dirty = 1;
        [_codeEditor setCode:updated];
    }
}

- (void)runDebugHandler:(NSString *)handler
{
    if (!_runtime) return;
    _runtime.suppressDialogs = YES;
    [_runtime runEventHandler:handler];
}

- (BOOL)writeCodeEditorSnapshot:(NSString *)path
{
    [_codeEditor writeTextViewSnapshot:[path stringByAppendingString:@".text.png"]];
    [_codeEditor writeScrollViewSnapshot:[path stringByAppendingString:@".scroll.png"]];
    [_codeEditor layoutSubtreeIfNeeded];
    [_codeEditor displayIfNeeded];
    NSBitmapImageRep *rep = [_codeEditor bitmapImageRepForCachingDisplayInRect:_codeEditor.bounds];
    if (!rep) return NO;
    [_codeEditor cacheDisplayInRect:_codeEditor.bounds toBitmapImageRep:rep];
    NSData *data = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    return [data writeToFile:path atomically:YES];
}

- (NSString *)diagnostics
{
    NSMutableString *text = [NSMutableString string];
    [text appendFormat:@"window=%.0fx%.0f", NSWidth(self.window.frame), NSHeight(self.window.frame)];
    if (_split.subviews.count >= 3) {
        [text appendFormat:@" split=%.0f/%.0f", NSWidth(_split.subviews[0].frame),
         NSWidth(_split.subviews[1].frame)];
    }
    [text appendFormat:@" project=%@", _project ? StringFromC(_project->name) : @"(none)"];
    [text appendFormat:@" form=%@", (_currentItem && _currentItem->form) ? StringFromC(vb_form_name(_currentItem->form)) : @"(none)"];
    [text appendFormat:@" controls=%d", (_currentItem && _currentItem->form) ? CountControls(_currentItem->form->root) : 0];
    [text appendFormat:@" code=%lu", (unsigned long)_codeEditor.code.length];
    [text appendFormat:@" mode=%@", _showCode ? @"code" : @"design"];
    [text appendFormat:@" child=%@", NSStringFromRect(_codeWindow.frame)];
    if (_designer.selected) [text appendFormat:@" selected=%@", StringFromC(_designer.selected->name)];
    if (_runtime) [text appendString:@" running=1"];
    return text;
}

- (void)dealloc
{
    if (_runtime) [_runtime.window close];
    if (_project) vb_project_free(_project);
}

@end
