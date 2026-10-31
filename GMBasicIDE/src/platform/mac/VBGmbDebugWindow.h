#ifndef VB_GMB_DEBUG_WINDOW_H
#define VB_GMB_DEBUG_WINDOW_H

#import <Cocoa/Cocoa.h>

/* Interactive debugger for one GMBasic source file.
 *
 * The IDE compiles the .bas to an A1 chunk and launches gmb_vm_host with
 * --debug-interactive. The host pauses on a breakpoint and prints a line protocol on
 * stderr (`@@GMBDBG PAUSED <line>`); this window sends continue/step through stdin and
 * forwards the paused line to the source editor. */
@interface VBGmbDebugWindow : NSWindowController <NSWindowDelegate>

@property (nonatomic, copy) void (^onPausedLine)(NSInteger line);
@property (nonatomic, copy) void (^onExit)(void);
@property (nonatomic, copy) NSArray<NSDictionary *> *sourceMap;

- (instancetype)initWithTitle:(NSString *)title;
- (void)startHost:(NSString *)host
            chunk:(NSString *)chunkPath
          workdir:(NSString *)workdir
      breakpoints:(NSArray<NSNumber *> *)breakpoints;
- (void)continueExecution;
- (void)stepExecution;
- (void)stopDebug;

@end

#endif
