#ifndef VB_COLOR_PALETTE_VIEW_H
#define VB_COLOR_PALETTE_VIEW_H

#import <Cocoa/Cocoa.h>

@interface VBColorPaletteView : NSView
@property (nonatomic, weak) id target;
@property (nonatomic) SEL action;
@property (nonatomic) BOOL pickingBackColor;
@end

#endif
