#import "VBPropertyPanel.h"
#import "VBTheme.h"

#include "core/vb_util.h"

static const CGFloat kHeaderHeight = 20.0;
static const CGFloat kComboHeight = 18.0;
static const CGFloat kRowHeight = 18.0;
static const CGFloat kTabsHeight = 22.0;
static const CGFloat kDescriptionHeight = 42.0;

@interface VBPropertyRowView : NSTableRowView
@end

@implementation VBPropertyRowView
- (void)drawBackgroundInRect:(NSRect)dirtyRect
{
    (void)dirtyRect;
    [[VBColorFace() colorWithAlphaComponent:0.72] setFill];
    NSRectFill(self.bounds);
    [[VBColorShadow() colorWithAlphaComponent:0.35] setFill];
    NSRectFill(NSMakeRect(0, NSHeight(self.bounds) - 1, NSWidth(self.bounds), 1));
    [[VBColorLight() colorWithAlphaComponent:0.5] setFill];
    NSRectFill(NSMakeRect(NSWidth(self.bounds) / 2, 0, 1, NSHeight(self.bounds)));
}
@end

@interface VBClassicTabView : NSView
@property (nonatomic) NSInteger selectedIndex;
@property (nonatomic, weak) id target;
@property (nonatomic) SEL action;
@end

@implementation VBClassicTabView

- (BOOL)isFlipped { return YES; }

- (void)drawRect:(NSRect)dirtyRect
{
    (void)dirtyRect;
    VBFillFace(self.bounds);
    NSArray<NSString *> *titles = @[@"Alphabetic", @"Categorized"];
    CGFloat width = NSWidth(self.bounds) / 2.0;
    NSDictionary *attrs = @{NSFontAttributeName: VBFontUI(),
                            NSForegroundColorAttributeName: VBColorText()};
    for (NSInteger i = 0; i < 2; i++) {
        NSRect tab = NSMakeRect(i * width + 1, 1, width - 2, NSHeight(self.bounds) - 2);
        if (i == self.selectedIndex) {
            [VBColorFace() setFill];
            NSRectFill(tab);
            VBDrawRaisedBevel(tab);
        } else {
            [VBColorFace() setFill];
            NSRectFill(NSMakeRect(tab.origin.x, tab.origin.y + 2, tab.size.width, tab.size.height - 2));
            [[VBColorShadow() colorWithAlphaComponent:0.8] setStroke];
            [NSBezierPath strokeRect:NSInsetRect(tab, 0.5, 0.5)];
        }
        NSString *title = titles[(NSUInteger)i];
        NSSize size = [title sizeWithAttributes:attrs];
        [title drawAtPoint:NSMakePoint(NSMidX(tab) - size.width / 2.0,
                                       NSMidY(tab) - size.height / 2.0)
            withAttributes:attrs];
    }
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
    NSInteger index = point.x < NSWidth(self.bounds) / 2.0 ? 0 : 1;
    self.selectedIndex = index;
    [self setNeedsDisplay:YES];
    if (self.target && self.action && [self.target respondsToSelector:self.action])
        ((void (*)(id, SEL, id))[self.target methodForSelector:self.action])(self.target, self.action, self);
}

@end

@implementation VBPropertyPanel {
    NSTableView *_table;
    NSScrollView *_scroll;
    NSMutableArray<NSString *> *_keys;
    NSMutableArray<NSNumber *> *_groupRows;
    NSMutableArray<NSString *> *_groupTitles;
    BOOL _categorized;
    NSTextField *_description;
    VBClassicTabView *_categoryTabs;
    NSArray<NSDictionary *> *_objects;
    VbControl *_control;
    NSRect _comboRect;
    BOOL _comboPressed;
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (!self) return nil;

    _keys = [NSMutableArray array];
    self.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;

    _scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(1, kHeaderHeight + kComboHeight + kTabsHeight + 8,
                                                             NSWidth(frameRect) - 2,
                                                             NSHeight(frameRect) - kHeaderHeight - kComboHeight - kTabsHeight - 9 - kDescriptionHeight)];
    _scroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _scroll.hasVerticalScroller = YES;
    _scroll.borderType = NSNoBorder;
    _scroll.drawsBackground = YES;
    _scroll.backgroundColor = VBColorFace();
    _scroll.scrollerStyle = NSScrollerStyleLegacy;

    _table = [[NSTableView alloc] initWithFrame:_scroll.bounds];
    _table.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _table.headerView = nil;
    _table.rowHeight = kRowHeight;
    _table.intercellSpacing = NSMakeSize(0, 0);
    _table.gridStyleMask = NSTableViewGridNone;
    _table.backgroundColor = VBColorFace();
    _table.usesAlternatingRowBackgroundColors = NO;
    _table.selectionHighlightStyle = NSTableViewSelectionHighlightStyleRegular;
    _table.allowsMultipleSelection = NO;
    _table.dataSource = self;
    _table.delegate = self;

    NSTableColumn *nameColumn = [[NSTableColumn alloc] initWithIdentifier:@"key"];
    nameColumn.width = 116;
    nameColumn.minWidth = 90;
    nameColumn.resizingMask = NSTableColumnUserResizingMask;
    [_table addTableColumn:nameColumn];

    NSTableColumn *valueColumn = [[NSTableColumn alloc] initWithIdentifier:@"value"];
    valueColumn.width = 160;
    valueColumn.minWidth = 80;
    valueColumn.resizingMask = NSTableColumnAutoresizingMask;
    [_table addTableColumn:valueColumn];

    _scroll.documentView = _table;
    [self addSubview:_scroll];

    _categoryTabs = [[VBClassicTabView alloc] initWithFrame:NSMakeRect(5, kHeaderHeight + kComboHeight + 3,
                                                                     NSWidth(frameRect) - 10, kTabsHeight - 2)];
    _categoryTabs.autoresizingMask = NSViewWidthSizable;
    _categoryTabs.selectedIndex = 0;
    _categoryTabs.target = self;
    _categoryTabs.action = @selector(categoryTabChanged:);
    [self addSubview:_categoryTabs];

    _description = [[NSTextField alloc] initWithFrame:NSMakeRect(1, NSHeight(frameRect) - kDescriptionHeight,
                                                                 NSWidth(frameRect) - 2, kDescriptionHeight - 2)];
    _description.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    _description.bezeled = YES;
    _description.bordered = YES;
    _description.editable = NO;
    _description.selectable = NO;
    _description.drawsBackground = YES;
    _description.backgroundColor = VBColorWindow();
    _description.textColor = VBColorText();
    _description.font = [NSFont systemFontOfSize:10];
    _description.cell.wraps = YES;
    _description.cell.scrollable = NO;
    _description.cell.lineBreakMode = NSLineBreakByWordWrapping;
    _description.stringValue = @"";
    [self addSubview:_description];

    return self;
}

- (BOOL)isFlipped { return YES; }

- (void)refreshTheme
{
    _scroll.backgroundColor = VBColorFace();
    _table.backgroundColor = VBColorFace();
    [_table reloadData];
    [self setNeedsDisplay:YES];
}

- (VbControl *)control { return _control; }

- (void)setObjects:(NSArray<NSDictionary *> *)objects
{
    _objects = [objects copy] ?: @[];
    [self setNeedsDisplay:YES];
}

- (void)setControl:(VbControl *)control
{
    _control = control;
    [self reload];
}

- (void)addKeyIfMissing:(NSMutableArray *)all key:(NSString *)key
{
    if (![all containsObject:key]) [all addObject:key];
}

- (void)reload
{
    [_keys removeAllObjects];
    [_groupRows removeAllObjects];
    [_groupTitles removeAllObjects];
    if (_control) {
        NSString *type = _control->type ? [NSString stringWithUTF8String:_control->type] : @"";
        NSMutableArray *all = [NSMutableArray array];
        [all addObject:@"Name"];

        if ([type isEqualToString:@"VB.Form"]) {
            [all addObjectsFromArray:@[@"Caption", @"ClientWidth", @"ClientHeight", @"BackColor",
                                       @"ForeColor", @"BorderStyle", @"StartUpPosition", @"MinButton",
                                       @"MaxButton", @"ShowInTaskbar"]];
        } else {
            BOOL hasCaption = [type isEqualToString:@"VB.Label"] || [type isEqualToString:@"VB.CommandButton"] ||
                              [type isEqualToString:@"VB.CheckBox"] || [type isEqualToString:@"VB.OptionButton"] ||
                              [type isEqualToString:@"VB.Frame"] || [type isEqualToString:@"VB.Data"];
            if (hasCaption) [all addObject:@"Caption"];
            if ([type isEqualToString:@"VB.TextBox"] || [type isEqualToString:@"VB.ComboBox"] ||
                [type isEqualToString:@"VB.ListBox"]) {
                [all addObject:@"Text"];
            }
            if ([type isEqualToString:@"VB.ComboBox"] || [type isEqualToString:@"VB.ListBox"]) {
                [all addObjectsFromArray:@[@"List", @"ListIndex", @"Appearance", @"MousePointer",
                                           @"RightToLeft", @"Locked"]];
            }
            if ([type isEqualToString:@"VB.TextBox"]) {
                [all addObjectsFromArray:@[@"MultiLine", @"PasswordChar"]];
            }
            if ([type isEqualToString:@"VB.Label"]) {
                [all addObjectsFromArray:@[@"Alignment", @"Appearance", @"AutoSize", @"BackStyle",
                                           @"BorderStyle", @"MousePointer", @"RightToLeft",
                                           @"UseMnemonic", @"WhatsThisHelpID", @"WordWrap"]];
            }
            if ([type isEqualToString:@"VB.CommandButton"]) {
                [all addObjectsFromArray:@[@"Appearance", @"MousePointer"]];
            }
            if ([type isEqualToString:@"VB.Timer"]) [all addObject:@"Interval"];
            if ([type isEqualToString:@"VB.CheckBox"] || [type isEqualToString:@"VB.OptionButton"]) {
                [all addObjectsFromArray:@[@"Value", @"Appearance", @"MousePointer", @"RightToLeft"]];
            }
            if ([type isEqualToString:@"VB.HScrollBar"] || [type isEqualToString:@"VB.VScrollBar"]) {
                [all addObjectsFromArray:@[@"Min", @"Max", @"Value", @"LargeChange", @"SmallChange",
                                           @"CausesValidation", @"MousePointer"]];
            }
            if ([type isEqualToString:@"VB.PictureBox"]) {
                [all addObjectsFromArray:@[@"Align", @"Appearance", @"AutoRedraw", @"AutoSize",
                                           @"BorderStyle", @"CausesValidation", @"FillColor",
                                           @"FillStyle", @"MousePointer", @"Picture"]];
            }
            if ([type isEqualToString:@"VB.Data"]) {
                [all addObjectsFromArray:@[@"Connect", @"DatabaseName", @"RecordSource", @"RecordsetType",
                                           @"BOFAction", @"EOFAction", @"ReadOnly", @"Exclusive",
                                           @"Options"]];
            }
            if ([type isEqualToString:@"VB.DataGrid"]) {
                [all addObjectsFromArray:@[@"DataSource", @"AllowAddNew", @"AllowDelete", @"AllowUpdate",
                                           @"ColumnHeaders", @"DefColWidth", @"RowHeight",
                                           @"AllowRowSizing", @"AllowColSizing"]];
            }
            if ([type isEqualToString:@"VB.CommonDialog"]) {
                [all addObjectsFromArray:@[@"Action", @"DialogTitle", @"Filter", @"FilterIndex",
                                           @"FileName", @"Flags", @"Color", @"CancelError"]];
            }
            [all addObjectsFromArray:@[@"Left", @"Top", @"Width", @"Height", @"Visible", @"Enabled",
                                       @"TabIndex", @"TabStop", @"BackColor", @"ForeColor",
                                       @"Font.Name", @"Font.Size", @"ToolTipText", @"Tag"]];
        }
        for (int i = 0; i < _control->nprops; i++) {
            NSString *key = [NSString stringWithUTF8String:_control->props[i].key];
            if (key) [self addKeyIfMissing:all key:key];
        }
        [all removeObject:@"Name"];
        if (_categorized) {
            NSArray *categoryOrder = @[@"Appearance", @"Behavior", @"Font", @"List", @"Position", @"Misc"];
            for (NSString *category in categoryOrder) {
                NSMutableArray *group = [NSMutableArray array];
                for (NSString *key in all) {
                    if ([[self categoryForKey:key] isEqualToString:category]) [group addObject:key];
                }
                if (!group.count) continue;
                /* Keep parallel arrays: group row then sorted properties. */
                [_groupRows addObject:@YES];
                [_groupTitles addObject:category];
                [_keys addObject:@""];
                [group sortUsingSelector:@selector(caseInsensitiveCompare:)];
                for (NSString *key in group) {
                    [_groupRows addObject:@NO];
                    [_groupTitles addObject:@""];
                    [_keys addObject:key];
                }
            }
            /* Any key not assigned to a category (future properties). */
            NSMutableArray *others = [NSMutableArray array];
            for (NSString *key in all) {
                BOOL seen = NO;
                for (NSString *category in categoryOrder)
                    if ([[self categoryForKey:key] isEqualToString:category]) seen = YES;
                if (!seen) [others addObject:key];
            }
            if (others.count) {
                [_groupRows addObject:@YES];
                [_groupTitles addObject:@"Misc"];
                [_keys addObject:@""];
                [others sortUsingSelector:@selector(caseInsensitiveCompare:)];
                for (NSString *key in others) {
                    [_groupRows addObject:@NO];
                    [_groupTitles addObject:@""];
                    [_keys addObject:key];
                }
            }
        } else {
            NSMutableArray *sorted = [[all sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)] mutableCopy];
            [sorted insertObject:@"Name" atIndex:0];
            for (NSString *key in sorted) {
                [_groupRows addObject:@NO];
                [_groupTitles addObject:@""];
                [_keys addObject:key];
            }
        }
    }
    [_table reloadData];
    [self setNeedsDisplay:YES];
}

- (void)setCategorized:(BOOL)categorized
{
    _categorized = categorized;
    _categoryTabs.selectedIndex = categorized ? 1 : 0;
    [self reload];
}

- (void)categoryTabChanged:(VBClassicTabView *)sender
{
    _categorized = sender.selectedIndex == 1;
    [self reload];
}

- (NSString *)categoryForKey:(NSString *)key
{
    static NSSet *appearance, *behavior, *font, *list, *position;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        appearance = [NSSet setWithObjects:@"BackColor", @"ForeColor", @"Appearance", @"MousePointer",
                      @"BorderStyle", @"BackStyle", @"WordWrap", @"Alignment", @"FillColor", @"FillStyle",
                      @"Picture", @"Align", nil];
        behavior = [NSSet setWithObjects:@"Visible", @"Enabled", @"TabIndex", @"TabStop", @"Locked",
                    @"CausesValidation", @"UseMnemonic", nil];
        font = [NSSet setWithObjects:@"Font.Name", @"Font.Size", nil];
        list = [NSSet setWithObjects:@"List", @"ListIndex", nil];
        position = [NSSet setWithObjects:@"Left", @"Top", @"Width", @"Height", @"AutoSize", nil];
    });
    if ([appearance containsObject:key]) return @"Appearance";
    if ([behavior containsObject:key]) return @"Behavior";
    if ([font containsObject:key]) return @"Font";
    if ([list containsObject:key]) return @"List";
    if ([position containsObject:key]) return @"Position";
    return @"Misc";
}

- (BOOL)tableView:(NSTableView *)tableView isGroupRow:(NSInteger)row
{
    (void)tableView;
    return row >= 0 && row < (NSInteger)_groupRows.count && _groupRows[(NSUInteger)row].boolValue;
}

- (BOOL)tableView:(NSTableView *)tableView shouldSelectRow:(NSInteger)row
{
    (void)tableView;
    return !(row >= 0 && row < (NSInteger)_groupRows.count && _groupRows[(NSUInteger)row].boolValue);
}

- (void)setFrameSize:(NSSize)newSize
{
    [super setFrameSize:newSize];
    if (_categoryTabs) {
        _categoryTabs.frame = NSMakeRect(5, kHeaderHeight + kComboHeight + 3,
                                         newSize.width - 10, kTabsHeight - 4);
    }
    if (_scroll) {
        _scroll.frame = NSMakeRect(1, kHeaderHeight + kComboHeight + kTabsHeight + 8,
                                   newSize.width - 2,
                                   newSize.height - kHeaderHeight - kComboHeight - kTabsHeight - 9 - kDescriptionHeight);
    }
    if (_description) {
        _description.frame = NSMakeRect(1, newSize.height - kDescriptionHeight,
                                        newSize.width - 2, kDescriptionHeight - 2);
    }
}

- (void)tableViewSelectionDidChange:(NSNotification *)notification
{
    (void)notification;
    NSInteger row = _table.selectedRow;
    NSString *key = (row >= 0 && row < (NSInteger)_keys.count) ? _keys[(NSUInteger)row] : @"";
    if (!key.length || (row < (NSInteger)_groupRows.count && _groupRows[(NSUInteger)row].boolValue)) {
        _description.stringValue = @"";
        return;
    }
    _description.stringValue = [self descriptionForKey:key];
}

- (NSString *)descriptionForKey:(NSString *)key
{
    if ([key isEqualToString:@"Name"]) return @"Nome usato nel codice per identificare il controllo.";
    if ([key isEqualToString:@"Caption"]) return @"Testo visualizzato sul controllo o nella barra del form.";
    if ([key isEqualToString:@"Text"]) return @"Testo contenuto nel controllo.";
    if ([key isEqualToString:@"Left"] || [key isEqualToString:@"Top"]) return @"Posizione del controllo rispetto al contenitore, in twip.";
    if ([key isEqualToString:@"Width"] || [key isEqualToString:@"Height"]) return @"Dimensioni del controllo in twip.";
    if ([key isEqualToString:@"Visible"]) return @"Determina se il controllo è visibile.";
    if ([key isEqualToString:@"Enabled"]) return @"Determina se il controllo risponde agli eventi utente.";
    if ([key isEqualToString:@"TabIndex"] || [key isEqualToString:@"TabStop"]) return @"Ordine e partecipazione del controllo al ciclo Tab.";
    if ([key isEqualToString:@"Font.Name"] || [key isEqualToString:@"FontName"]) return @"Famiglia del carattere usato dal controllo.";
    if ([key isEqualToString:@"Font.Size"] || [key isEqualToString:@"FontSize"]) return @"Dimensione del carattere in punti.";
    if ([key isEqualToString:@"ForeColor"] || [key isEqualToString:@"BackColor"]) return @"Colore di primo piano o di sfondo.";
    if ([key isEqualToString:@"Appearance"]) return @"Aspetto del controllo: 0 = Flat, 1 = 3D.";
    if ([key isEqualToString:@"MousePointer"]) return @"Tipo di cursore mostrato sul controllo.";
    if ([key isEqualToString:@"RightToLeft"]) return @"Direzione del testo e disposizione per sistemi bidirezionali.";
    if ([key isEqualToString:@"Locked"]) return @"Impedisce la modifica del contenuto del controllo.";
    if ([key isEqualToString:@"StartUpPosition"]) return @"Posizione iniziale della finestra: Manual, Center Owner, Center Screen, Windows Default.";
    if ([key isEqualToString:@"ShowInTaskbar"]) return @"Indica se la finestra appare nella taskbar.";
    if ([key isEqualToString:@"WhatThisHelpID"] || [key isEqualToString:@"WhatsThisHelpID"]) return @"Identificativo della guida contestuale.";
    if ([key isEqualToString:@"WordWrap"]) return @"Abilita il ritorno a capo automatico del testo.";
    if ([key isEqualToString:@"AutoSize"]) return @"Ridimensiona automaticamente il controllo sul contenuto.";
    if ([key isEqualToString:@"BackStyle"]) return @"Sfondo trasparente (0) o opaco (1).";
    if ([key isEqualToString:@"BorderStyle"]) return @"Bordo del controllo: None (0) o Fixed Single (1).";
    if ([key isEqualToString:@"UseMnemonic"]) return @"Interpreta il carattere & come tasto di accesso.";
    if ([key isEqualToString:@"CausesValidation"]) return @"Esegue la validazione quando il controllo perde il fuoco.";
    if ([key isEqualToString:@"List"] || [key isEqualToString:@"ListIndex"]) return @"Elementi della lista e indice selezionato.";
    return @"Proprietà del controllo.";
}

- (NSString *)displayValueForKey:(NSString *)key
{
    if (!_control) return @"";
    if ([key isEqualToString:@"Name"]) {
        return _control->name ? [NSString stringWithUTF8String:_control->name] : @"";
    }
    if (vb_prop_is_boolean(key.UTF8String)) {
        /* Enabled/Visible/TabStop are on unless the form says otherwise; everything else
           (a check box, a read-only flag) defaults to False like in VB6. */
        /* ColumnHeaders and AllowUpdate are the DataGrid flags VB6 turns on by default. */
        int fallback = ([key isEqualToString:@"Visible"] || [key isEqualToString:@"Enabled"] ||
                        [key isEqualToString:@"TabStop"] || [key isEqualToString:@"ColumnHeaders"] ||
                        [key isEqualToString:@"AllowUpdate"] || [key isEqualToString:@"ShowInTaskbar"] ||
                        [key isEqualToString:@"CausesValidation"] || [key isEqualToString:@"UseMnemonic"]) ? -1 : 0;
        return vb_control_get_int(_control, key.UTF8String, fallback) ? @"True" : @"False";
    }
    if ([key isEqualToString:@"BackColor"]) {
        const char *value = vb_control_get(_control, "BackColor");
        return value ? [NSString stringWithUTF8String:value] : @"&H8000000F&";
    }
    if ([key isEqualToString:@"ForeColor"]) {
        const char *value = vb_control_get(_control, "ForeColor");
        return value ? [NSString stringWithUTF8String:value] : @"&H80000012&";
    }
    if ([key isEqualToString:@"Font.Name"]) {
        const char *value = vb_control_get(_control, "Font.Name");
        return value ? [NSString stringWithUTF8String:value] : @"MS Sans Serif";
    }
    if ([key isEqualToString:@"Font.Size"]) {
        const char *value = vb_control_get(_control, "Font.Size");
        return value ? [NSString stringWithUTF8String:value] : @"9";
    }
    if ([key isEqualToString:@"ListIndex"]) {
        return [NSString stringWithFormat:@"%d", vb_control_get_int(_control, "ListIndex", -1)];
    }
    if ([key isEqualToString:@"Appearance"] && !vb_control_has(_control, "Appearance")) {
        return @"1";
    }
    if ([key isEqualToString:@"MousePointer"]) {
        return [NSString stringWithFormat:@"%d", vb_control_get_int(_control, "MousePointer", 0)];
    }
    char *value = vb_control_get_display(_control, key.UTF8String);
    NSString *text = value ? [NSString stringWithUTF8String:value] : @"";
    free(value);
    return text;
}

- (void)drawRect:(NSRect)dirtyRect
{
    (void)dirtyRect;
    VBFillFace(self.bounds);

    NSString *name = _control && _control->name ? [NSString stringWithUTF8String:_control->name] : @"";
    NSString *title = @"Properties";
    VBDrawTitleBar(NSMakeRect(0, 0, NSWidth(self.bounds), kHeaderHeight), title, YES, NO, NO, NO);
    [[VBColorShadow() colorWithAlphaComponent:0.9] setFill];
    NSRectFill(NSMakeRect(0, kHeaderHeight, NSWidth(self.bounds), 1));

    // Object selector combo box
    _comboRect = NSMakeRect(5, kHeaderHeight + 3, NSWidth(self.bounds) - 10, kComboHeight);
    VBDrawSunkenBevel(_comboRect);
    NSRect arrow = NSMakeRect(NSMaxX(_comboRect) - 17, NSMinY(_comboRect) + 2, 15, NSHeight(_comboRect) - 4);
    if (_comboPressed) VBDrawPressedBevel(arrow);
    else VBDrawRaisedBevel(arrow);
    VBDrawClassicArrow(NSMakeRect(NSMidX(arrow) - 4, NSMidY(arrow) - 2, 8, 5), YES, VBColorText());

    NSString *comboTitle = name.length ? name : @"";
    NSDictionary *comboAttrs = @{NSFontAttributeName: VBFontUI(),
                                 NSForegroundColorAttributeName: VBColorText()};
    [comboTitle drawInRect:NSMakeRect(NSMinX(_comboRect) + 3, NSMidY(_comboRect) - 7,
                                      NSWidth(_comboRect) - 24, 14)
            withAttributes:comboAttrs];
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
    if (!NSPointInRect(point, _comboRect)) return;
    _comboPressed = YES;
    [self setNeedsDisplay:YES];

    if (_objects.count) {
        NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Objects"];
        for (NSDictionary *entry in _objects) {
            NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:entry[@"title"] action:nil keyEquivalent:@""];
            item.representedObject = entry[@"value"];
            item.target = self;
            item.action = @selector(selectObjectFromMenu:);
            [menu addItem:item];
        }
        NSPoint screen = [self.window convertPointToScreen:
                          [self convertPoint:NSMakePoint(NSMinX(_comboRect), NSMaxY(_comboRect)) toView:nil]];
        [menu popUpMenuPositioningItem:nil atLocation:screen inView:nil];
    }
    _comboPressed = NO;
    [self setNeedsDisplay:YES];
}

- (void)selectObjectFromMenu:(NSMenuItem *)sender
{
    VbControl *control = (VbControl *)[sender.representedObject pointerValue];
    if (!control) return;
    _control = control;
    [self reload];
    if (self.delegate) [self.delegate propertyPanel:self didSelectControl:control];
}

- (NSInteger)numberOfRowsInTableView:(NSTableView *)tableView
{
    (void)tableView;
    return (NSInteger)_keys.count;
}

- (NSTableRowView *)tableView:(NSTableView *)tableView rowViewForRow:(NSInteger)row
{
    (void)tableView;
    (void)row;
    return [[VBPropertyRowView alloc] initWithFrame:NSMakeRect(0, 0, 260, kRowHeight)];
}

- (NSView *)tableView:(NSTableView *)tableView viewForTableColumn:(NSTableColumn *)tableColumn row:(NSInteger)row
{
    (void)tableView;
    if (row < 0 || row >= (NSInteger)_keys.count) return nil;
    BOOL isValue = [tableColumn.identifier isEqualToString:@"value"];
    if (row < (NSInteger)_groupRows.count && _groupRows[(NSUInteger)row].boolValue) {
        if (isValue) return [[NSView alloc] initWithFrame:NSMakeRect(0, 0, 10, kRowHeight)];
        NSTableCellView *groupCell = (NSTableCellView *)[_table makeViewWithIdentifier:@"group" owner:self];
        if (!groupCell) {
            groupCell = [[NSTableCellView alloc] initWithFrame:NSMakeRect(0, 0, 150, kRowHeight)];
            groupCell.identifier = @"group";
            NSTextField *field = [[NSTextField alloc] initWithFrame:NSMakeRect(3, 2, 140, 14)];
            field.autoresizingMask = NSViewWidthSizable;
            field.bezeled = NO;
            field.bordered = NO;
            field.editable = NO;
            field.selectable = NO;
            field.drawsBackground = NO;
            field.font = [NSFont boldSystemFontOfSize:11];
            field.textColor = VBColorText();
            [groupCell addSubview:field];
            groupCell.textField = field;
        }
        groupCell.textField.stringValue = row < (NSInteger)_groupTitles.count ? _groupTitles[(NSUInteger)row] : @"";
        return groupCell;
    }
    NSString *key = _keys[(NSUInteger)row];
    if (isValue && ([key isEqualToString:@"BackColor"] || [key isEqualToString:@"ForeColor"] ||
                     [key isEqualToString:@"FillColor"] || [key isEqualToString:@"BorderColor"])) {
        NSTableCellView *colorCell = (NSTableCellView *)[_table makeViewWithIdentifier:@"color-value" owner:self];
        if (!colorCell) {
            colorCell = [[NSTableCellView alloc] initWithFrame:NSMakeRect(0, 0, 150, kRowHeight)];
            colorCell.identifier = @"color-value";
            NSColorWell *well = [[NSColorWell alloc] initWithFrame:NSMakeRect(3, 1, 140, 16)];
            well.identifier = @"color-well";
            well.autoresizingMask = NSViewWidthSizable;
            well.target = self;
            well.action = @selector(colorChanged:);
            [colorCell addSubview:well];
        }
        NSColorWell *well = nil;
        for (NSView *view in colorCell.subviews) {
            if ([view.identifier isEqualToString:@"color-well"]) { well = (NSColorWell *)view; break; }
        }
        well.tag = row;
        well.color = [self colorForKey:key];
        well.enabled = YES;
        return colorCell;
    }
    NSArray<NSString *> *enumOptions = [self enumOptionsForKey:key];
    if (isValue && enumOptions) {
        NSTableCellView *enumCell = (NSTableCellView *)[_table makeViewWithIdentifier:@"enum-value" owner:self];
        if (!enumCell) {
            enumCell = [[NSTableCellView alloc] initWithFrame:NSMakeRect(0, 0, 150, kRowHeight)];
            enumCell.identifier = @"enum-value";
            NSPopUpButton *popup = [[NSPopUpButton alloc] initWithFrame:NSMakeRect(3, 1, 140, 16) pullsDown:NO];
            popup.identifier = @"enum-button";
            popup.autoresizingMask = NSViewWidthSizable;
            popup.controlSize = NSControlSizeSmall;
            popup.font = VBFontUI();
            popup.target = self;
            popup.action = @selector(enumChanged:);
            [enumCell addSubview:popup];
        }
        NSPopUpButton *popup = nil;
        for (NSView *view in enumCell.subviews) {
            if ([view.identifier isEqualToString:@"enum-button"]) { popup = (NSPopUpButton *)view; break; }
        }
        popup.tag = row;
        [popup removeAllItems];
        [popup addItemsWithTitles:enumOptions];
        NSInteger selected = [self displayValueForKey:key].integerValue;
        if ([key isEqualToString:@"Value"] && selected < 0) selected = 1;
        if (selected < 0 || selected >= (NSInteger)enumOptions.count) selected = 0;
        [popup selectItemAtIndex:selected];
        popup.enabled = YES;
        return enumCell;
    }
    if (isValue && [key isEqualToString:@"List"]) {
        NSTableCellView *listCell = (NSTableCellView *)[_table makeViewWithIdentifier:@"list-value" owner:self];
        if (!listCell) {
            listCell = [[NSTableCellView alloc] initWithFrame:NSMakeRect(0, 0, 150, kRowHeight)];
            listCell.identifier = @"list-value";
            NSButton *button = [[NSButton alloc] initWithFrame:NSMakeRect(3, 1, 140, 16)];
            button.identifier = @"list-button";
            button.autoresizingMask = NSViewWidthSizable;
            button.bezelStyle = NSBezelStyleRounded;
            button.controlSize = NSControlSizeSmall;
            button.font = VBFontUI();
            button.target = self;
            button.action = @selector(editListItems:);
            [listCell addSubview:button];
        }
        NSButton *button = nil;
        for (NSView *view in listCell.subviews) {
            if ([view.identifier isEqualToString:@"list-button"]) { button = (NSButton *)view; break; }
        }
        NSInteger count = [self listItemCount];
        button.tag = row;
        button.title = [NSString stringWithFormat:@"Edit Items (%ld)...", (long)count];
        return listCell;
    }
    NSString *identifier = isValue ? @"value" : @"key";
    NSTableCellView *cell = (NSTableCellView *)[_table makeViewWithIdentifier:identifier owner:self];

    if (!cell) {
        cell = [[NSTableCellView alloc] initWithFrame:NSMakeRect(0, 0, 150, kRowHeight)];
        cell.identifier = identifier;
        NSTextField *field = [[NSTextField alloc] initWithFrame:NSMakeRect(3, 2, 140, 14)];
        field.autoresizingMask = NSViewWidthSizable;
        field.bezeled = NO;
        field.bordered = NO;
        field.focusRingType = NSFocusRingTypeNone;
        field.font = VBFontUI();
        field.lineBreakMode = NSLineBreakByTruncatingTail;
        if (isValue) {
            field.editable = YES;
            field.selectable = YES;
            field.drawsBackground = YES;
            field.backgroundColor = VBColorWindow();
            field.wantsLayer = YES;
            field.layer.borderWidth = 1;
            field.layer.borderColor = [[VBColorShadow() colorWithAlphaComponent:0.9] CGColor];
            field.delegate = self;
            field.textColor = VBColorText();
        } else {
            field.editable = NO;
            field.selectable = NO;
            field.drawsBackground = NO;
            field.textColor = VBColorText();
        }
        [cell addSubview:field];
        cell.textField = field;
    }

    NSTextField *field = cell.textField;
    field.font = VBFontUI();
    if (isValue) {
        field.drawsBackground = YES;
        field.backgroundColor = VBColorWindow();
        field.textColor = VBColorText();
        field.layer.borderColor = [[VBColorShadow() colorWithAlphaComponent:0.9] CGColor];
    } else {
        field.drawsBackground = NO;
        field.textColor = VBColorText();
    }
    cell.textField.stringValue = isValue ? [self displayValueForKey:key] : key;
    cell.textField.tag = row;
    cell.textField.enabled = isValue;
    return cell;
}

- (NSArray<NSString *> *)listItems
{
    NSString *raw = [self displayValueForKey:@"List"];
    if (!raw.length) return @[];
    NSArray<NSString *> *parts = [raw componentsSeparatedByCharactersInSet:
                                  [NSCharacterSet newlineCharacterSet]];
    NSMutableArray<NSString *> *items = [NSMutableArray array];
    for (NSString *item in parts) {
        if (item.length) [items addObject:item];
    }
    return items;
}

- (NSInteger)listItemCount
{
    return (NSInteger)[self listItems].count;
}

- (void)editListItems:(NSButton *)sender
{
    NSInteger row = sender.tag;
    if (row < 0 || row >= (NSInteger)_keys.count) return;
    NSString *key = _keys[(NSUInteger)row];
    if (![key isEqualToString:@"List"]) return;

    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"List Items";
    alert.informativeText = @"One item per line. Empty lines are ignored.";
    [alert addButtonWithTitle:@"OK"];
    [alert addButtonWithTitle:@"Cancel"];

    NSTextView *textView = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, 360, 180)];
    textView.font = [NSFont monospacedSystemFontOfSize:12 weight:NSFontWeightRegular];
    textView.richText = NO;
    textView.string = [[self listItems] componentsJoinedByString:@"\n"];
    textView.minSize = NSMakeSize(0, 180);
    textView.maxSize = NSMakeSize(CGFLOAT_MAX, CGFLOAT_MAX);
    textView.verticallyResizable = YES;
    textView.textContainer.widthTracksTextView = YES;

    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(0, 0, 360, 180)];
    scroll.hasVerticalScroller = YES;
    scroll.borderType = NSBezelBorder;
    scroll.documentView = textView;
    alert.accessoryView = scroll;

    if ([alert runModal] != NSAlertFirstButtonReturn) return;
    NSArray<NSString *> *parts = [textView.string componentsSeparatedByCharactersInSet:
                                  [NSCharacterSet newlineCharacterSet]];
    NSMutableArray<NSString *> *items = [NSMutableArray array];
    for (NSString *item in parts) {
        if (item.length) [items addObject:item];
    }
    NSString *value = [items componentsJoinedByString:@"\n"];
    if (!self.delegate || ![self.delegate propertyPanel:self setValue:value forKey:@"List"]) return;
    [self reload];
}

- (NSArray<NSString *> *)enumOptionsForKey:(NSString *)key
{
    if ([key isEqualToString:@"Appearance"]) return @[@"Flat", @"3D"];
    if ([key isEqualToString:@"Alignment"]) return @[@"Left", @"Right", @"Center"];
    if ([key isEqualToString:@"BorderStyle"]) {
        NSString *type = _control && _control->type ? [NSString stringWithUTF8String:_control->type] : @"";
        if ([type isEqualToString:@"VB.Form"])
            return @[@"None", @"Fixed Single", @"Sizable", @"Fixed Dialog"];
        return @[@"None", @"Fixed Single"];
    }
    if ([key isEqualToString:@"StartUpPosition"]) return @[@"Manual", @"Center Owner", @"Center Screen", @"Windows Default"];
    if ([key isEqualToString:@"Style"]) return @[@"Standard", @"Graphical"];
    if ([key isEqualToString:@"Value"]) {
        NSString *type = _control && _control->type ? [NSString stringWithUTF8String:_control->type] : @"";
        if ([type isEqualToString:@"VB.CheckBox"] || [type isEqualToString:@"VB.OptionButton"])
            return @[@"Unchecked", @"Checked", @"Grayed"];
        return nil;
    }
    if ([key isEqualToString:@"MousePointer"]) {
        return @[@"Default", @"Arrow", @"Cross", @"I-Beam", @"Icon", @"Size",
                 @"Size NE-SW", @"Size NS", @"Size NW-SE", @"Size WE", @"Up Arrow",
                 @"Hourglass", @"No Drop", @"Arrow", @"Hourglass", @"Arrow"];
    }
    return nil;
}

- (void)enumChanged:(NSPopUpButton *)sender
{
    NSInteger row = sender.tag;
    if (row < 0 || row >= (NSInteger)_keys.count) return;
    NSString *key = _keys[(NSUInteger)row];
    NSString *value = [NSString stringWithFormat:@"%ld", (long)sender.indexOfSelectedItem];
    if (!self.delegate || ![self.delegate propertyPanel:self setValue:value forKey:key]) return;
    [self reload];
}

- (NSColor *)colorForKey:(NSString *)key
{
    NSString *value = [self displayValueForKey:key];
    int r = 0, g = 0, b = 0;
    if (!vb_color_from_value(value.UTF8String, &r, &g, &b))
        return key.length && [key isEqualToString:@"BackColor"] ? VBColorFace() : VBColorText();
    return [NSColor colorWithSRGBRed:r / 255.0 green:g / 255.0 blue:b / 255.0 alpha:1.0];
}

- (void)colorChanged:(NSColorWell *)sender
{
    NSInteger row = sender.tag;
    if (row < 0 || row >= (NSInteger)_keys.count) return;
    NSString *key = _keys[(NSUInteger)row];
    NSColor *color = [sender.color colorUsingColorSpace:[NSColorSpace sRGBColorSpace]];
    if (!color) return;
    int r = (int)lround(color.redComponent * 255.0);
    int g = (int)lround(color.greenComponent * 255.0);
    int b = (int)lround(color.blueComponent * 255.0);
    NSString *value = [NSString stringWithFormat:@"&H%02X%02X%02X&", b, g, r];
    if (!self.delegate || ![self.delegate propertyPanel:self setValue:value forKey:key]) return;
}

- (void)controlTextDidEndEditing:(NSNotification *)notification
{
    NSTextField *field = notification.object;
    NSInteger row = field.tag;
    if (row < 0 || row >= (NSInteger)_keys.count) return;
    NSString *key = _keys[(NSUInteger)row];
    NSString *value = field.stringValue;
    if (!self.delegate || ![self.delegate propertyPanel:self setValue:value forKey:key]) {
        [_table reloadData];
        return;
    }
    [self reload];
}

@end
