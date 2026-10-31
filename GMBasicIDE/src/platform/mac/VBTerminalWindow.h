#ifndef VB_TERMINAL_WINDOW_H
#define VB_TERMINAL_WINDOW_H

#import <Cocoa/Cocoa.h>

/* Integrated shell terminal. It runs `/bin/zsh -l` with pipes (no PTY/job control):
   commands are sent line by line, stdout/stderr are appended live. */
@interface VBTerminalWindow : NSWindowController <NSWindowDelegate>

- (instancetype)initWithWorkingDirectory:(NSString *)directory;
- (void)sendCommand:(NSString *)command;
- (void)interrupt;
- (void)clearOutput;

@end

#endif
