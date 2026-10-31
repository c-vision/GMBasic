#ifndef VB_GMB_SOURCE_WINDOW_H
#define VB_GMB_SOURCE_WINDOW_H

#import <Cocoa/Cocoa.h>

/* Standalone GMBasic source editor: edits one .bas file in GMBasic language mode and
   drives the external compiler on its folder. */
@interface VBGmbSourceWindowController : NSWindowController
@property (nonatomic, readonly, copy) NSString *path;

- (instancetype)initWithPath:(NSString *)path;
- (void)refreshTheme;
- (void)startDebug;
- (BOOL)isDebugging;
- (void)stepDebug;
- (void)continueDebug;
- (void)stopDebug;
- (void)toggleCurrentBreakpoint;
- (void)clearAllBreakpoints;

@end

#endif
