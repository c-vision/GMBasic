#import "VBMenuBarView.h"
#import "VBTheme.h"

@implementation VBMenuBarView {
    NSInteger _openIndex;
    NSRect _openRect;
    NSInteger _pendingIndex;
    NSMenu *_activeMenu;
    NSTimer *_hoverTimer;
}

- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    if (!self) return nil;
    _openIndex = -1;
    return self;
}

- (BOOL)isFlipped { return YES; }

- (void)setTitles:(NSArray<NSString *> *)titles menus:(NSArray<NSMenu *> *)menus
{
    self.titles = titles ?: @[];
    self.menus = menus ?: @[];
    [self setNeedsDisplay:YES];
}

- (NSArray<NSNumber *> *)itemWidths
{
    NSMutableArray<NSNumber *> *widths = [NSMutableArray array];
    NSDictionary *attrs = @{NSFontAttributeName: VBFontUI()};
    for (NSString *title in self.titles) {
        CGFloat width = [title sizeWithAttributes:attrs].width + 16;
        [widths addObject:@(MAX(28, ceil(width)))];
    }
    return widths;
}

- (void)drawRect:(NSRect)dirtyRect
{
    (void)dirtyRect;
    VBFillFace(self.bounds);

    NSDictionary *attrs = @{
        NSFontAttributeName: VBFontUI(),
        NSForegroundColorAttributeName: VBColorText()
    };
    CGFloat x = 2;
    NSArray<NSNumber *> *widths = [self itemWidths];
    for (NSUInteger i = 0; i < self.titles.count; i++) {
        CGFloat width = widths[i].doubleValue;
        NSRect item = NSMakeRect(x, 1, width, NSHeight(self.bounds) - 3);
        if ((NSInteger)i == _openIndex) {
            VBDrawPressedBevel(item);
        }
        NSString *title = self.titles[i];
        NSSize size = [title sizeWithAttributes:attrs];
        [title drawAtPoint:NSMakePoint(NSMidX(item) - size.width / 2.0,
                                       NSMidY(item) - size.height / 2.0)
            withAttributes:attrs];
        x += width;
    }

    [[VBColorShadow() colorWithAlphaComponent:0.9] setFill];
    NSRectFill(NSMakeRect(0, NSHeight(self.bounds) - 1, NSWidth(self.bounds), 1));
}

- (NSInteger)menuIndexAtPoint:(NSPoint)point
{
    NSArray<NSNumber *> *widths = [self itemWidths];
    CGFloat x = 2;
    for (NSUInteger i = 0; i < self.titles.count && i < self.menus.count; i++) {
        CGFloat width = widths[i].doubleValue;
        NSRect item = NSMakeRect(x, 1, width, NSHeight(self.bounds) - 3);
        if (NSPointInRect(point, item)) return (NSInteger)i;
        x += width;
    }
    return -1;
}

- (void)openMenuAtIndex:(NSInteger)index
{
    while (index >= 0 && index < (NSInteger)self.menus.count) {
        _openIndex = index;
        _pendingIndex = -1;
        [self setNeedsDisplay:YES];
        NSArray<NSNumber *> *widths = [self itemWidths];
        CGFloat x = 2;
        for (NSInteger i = 0; i < index; i++) x += widths[(NSUInteger)i].doubleValue;
        NSRect item = NSMakeRect(x, 1, widths[(NSUInteger)index].doubleValue, NSHeight(self.bounds) - 3);
        _openRect = item;
        NSMenu *menu = self.menus[(NSUInteger)index];
        _activeMenu = menu;
        /* NSMenu runs its own modal tracking loop; local event monitors are not
           guaranteed to see mouse-moved events there. Poll the pointer in the
           tracking run-loop mode instead, so the open menu follows the pointer
           across the horizontal menu bar exactly like the Windows/VB6 IDE. */
        [_hoverTimer invalidate];
        _hoverTimer = [NSTimer timerWithTimeInterval:0.016
                                              target:self
                                            selector:@selector(hoverTimerFired:)
                                            userInfo:nil
                                             repeats:YES];
        [[NSRunLoop currentRunLoop] addTimer:_hoverTimer forMode:NSEventTrackingRunLoopMode];
        [[NSRunLoop currentRunLoop] addTimer:_hoverTimer forMode:NSRunLoopCommonModes];
        NSPoint screenPoint = [self.window convertPointToScreen:
                               [self convertPoint:NSMakePoint(NSMinX(item), NSMaxY(item)) toView:nil]];
        [menu popUpMenuPositioningItem:nil atLocation:screenPoint inView:nil];
        [_hoverTimer invalidate];
        _hoverTimer = nil;
        _activeMenu = nil;
        NSInteger next = _pendingIndex;
        _openIndex = -1;
        [self setNeedsDisplay:YES];
        index = next;
    }
}

- (void)hoverTimerFired:(NSTimer *)timer
{
    (void)timer;
    if (_openIndex < 0 || !_activeMenu || !self.window) return;
    NSPoint screen = [NSEvent mouseLocation];
    NSPoint inWindow = [self.window convertPointFromScreen:screen];
    NSPoint point = [self convertPoint:inWindow fromView:nil];
    NSInteger hover = [self menuIndexAtPoint:point];
    if (hover >= 0 && hover != _openIndex) {
        _pendingIndex = hover;
        [_activeMenu cancelTracking];
    }
}

- (void)mouseDown:(NSEvent *)event
{
    NSPoint point = [self convertPoint:event.locationInWindow fromView:nil];
    NSInteger index = [self menuIndexAtPoint:point];
    if (index >= 0) [self openMenuAtIndex:index];
}

@end
