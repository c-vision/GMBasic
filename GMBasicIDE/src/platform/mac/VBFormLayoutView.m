#import "VBFormLayoutView.h"
#import "VBTheme.h"

@implementation VBFormLayoutView {
    VbForm *_form;
}

- (BOOL)isFlipped { return YES; }

- (void)setForm:(VbForm *)form
{
    _form = form;
    [self setNeedsDisplay:YES];
}

- (void)drawRect:(NSRect)dirtyRect
{
    (void)dirtyRect;
    [VBColorWorkspace() setFill];
    NSRectFill(self.bounds);
    CGFloat top = 0;
    if (self.showsHeader) {
        VBDrawTitleBar(NSMakeRect(0, 0, NSWidth(self.bounds), 20), @"Form Layout", YES, NO, NO, NO);
        top = 21;
    }

    NSRect desktop = NSMakeRect(14, top + 12, NSWidth(self.bounds) - 28,
                                MAX(20, NSHeight(self.bounds) - top - 26));
    [[VBColorTitleBar() colorWithAlphaComponent:0.9] setFill];
    NSRectFill(desktop);
    [VBColorDarkShadow() setStroke];
    [NSBezierPath strokeRect:NSInsetRect(desktop, 0.5, 0.5)];

    if (!_form || !_form->root) {
        NSDictionary *attrs = @{NSFontAttributeName: VBFontUI(),
                                NSForegroundColorAttributeName: VBColorMutedText()};
        [@"No form" drawAtPoint:NSMakePoint(NSMidX(self.bounds) - 24, NSMidY(self.bounds) - 7)
                 withAttributes:attrs];
        return;
    }

    CGFloat fw = VBTwipsToPoints(vb_form_client_width(_form));
    CGFloat fh = VBTwipsToPoints(vb_form_client_height(_form));
    if (fw < 80) fw = 320;
    if (fh < 60) fh = 240;
    CGFloat availableW = NSWidth(desktop) - 36;
    CGFloat availableH = NSHeight(desktop) - 36;
    CGFloat scale = MIN(availableW / fw, availableH / fh);
    if (scale > 0.8) scale = 0.8;
    if (scale <= 0) scale = 0.2;
    NSRect formRect = NSMakeRect(NSMidX(desktop) - fw * scale / 2,
                                 NSMidY(desktop) - fh * scale / 2,
                                 fw * scale, fh * scale);

    [VBColorFace() setFill];
    NSRectFill(formRect);
    [VBColorDarkShadow() setStroke];
    [NSBezierPath strokeRect:NSInsetRect(formRect, 0.5, 0.5)];

    NSRect title = NSMakeRect(NSMinX(formRect), NSMinY(formRect), NSWidth(formRect), 13);
    [VBColorTitleBar() setFill];
    NSRectFill(title);
    NSDictionary *attrs = @{NSFontAttributeName: [NSFont boldSystemFontOfSize:9],
                            NSForegroundColorAttributeName: VBColorTitleText()};
    NSString *name = _form->root->name ? [NSString stringWithUTF8String:_form->root->name] : @"Form";
    [name drawInRect:NSInsetRect(title, 2, 0) withAttributes:attrs];
}

@end
