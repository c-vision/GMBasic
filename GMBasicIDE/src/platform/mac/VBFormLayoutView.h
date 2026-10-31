#ifndef VB_FORM_LAYOUT_VIEW_H
#define VB_FORM_LAYOUT_VIEW_H

#import <Cocoa/Cocoa.h>
#include "core/vb_model.h"

@interface VBFormLayoutView : NSView
@property (nonatomic) BOOL showsHeader;
- (void)setForm:(VbForm *)form;
@end

#endif
