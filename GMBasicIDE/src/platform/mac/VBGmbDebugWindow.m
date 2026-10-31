#import "VBGmbDebugWindow.h"
#import "VBTheme.h"

#include <unistd.h>
#include <signal.h>

@implementation VBGmbDebugWindow {
    NSTextView *_output;
    NSTextField *_status;
    NSTextField *_watchField;
    NSTask *_task;
    NSFileHandle *_input;
    NSMutableData *_stderrBuffer;
    BOOL _running;
    BOOL _stopped;
}

- (instancetype)initWithTitle:(NSString *)title
{
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 760, 320)
                                                   styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                              NSWindowStyleMaskResizable)
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    self = [super initWithWindow:window];
    if (!self) return nil;
    window.title = title ?: @"GMBasic debug";
    window.delegate = self;
    window.backgroundColor = VBColorFace();

    NSView *content = window.contentView;
    _status = [NSTextField labelWithString:@"Debug non avviato"];
    _status.frame = NSMakeRect(10, NSHeight(content.bounds) - 28, NSWidth(content.bounds) - 20, 18);
    _status.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    _status.font = VBFontUIBold();
    _status.textColor = VBColorText();
    [content addSubview:_status];

    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(10, 70,
                                                                          NSWidth(content.bounds) - 20,
                                                                          NSHeight(content.bounds) - 106)];
    scroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    scroll.hasVerticalScroller = YES;
    scroll.drawsBackground = YES;
    scroll.backgroundColor = VBColorWindow();
    _output = [[NSTextView alloc] initWithFrame:scroll.bounds];
    _output.editable = NO;
    _output.font = VBFontCode();
    _output.textColor = VBColorCodeText();
    _output.backgroundColor = VBColorCodeBackground();
    _output.string = @"";
    scroll.documentView = _output;
    [content addSubview:scroll];

    NSButton *cont = [NSButton buttonWithTitle:@"Continue" target:self action:@selector(continueExecution)];
    cont.frame = NSMakeRect(10, 7, 100, 26);
    [content addSubview:cont];
    NSButton *step = [NSButton buttonWithTitle:@"Step" target:self action:@selector(stepExecution)];
    step.frame = NSMakeRect(116, 7, 80, 26);
    [content addSubview:step];
    NSButton *pause = [NSButton buttonWithTitle:@"Pause" target:self action:@selector(pauseExecution)];
    pause.frame = NSMakeRect(202, 7, 70, 26);
    [content addSubview:pause];
    NSButton *stop = [NSButton buttonWithTitle:@"Stop" target:self action:@selector(stopDebug)];
    stop.frame = NSMakeRect(278, 7, 64, 26);
    [content addSubview:stop];
    NSButton *stack = [NSButton buttonWithTitle:@"Stack" target:self action:@selector(showStack)];
    stack.frame = NSMakeRect(348, 7, 64, 26);
    [content addSubview:stack];
    _watchField = [NSTextField textFieldWithString:@""];
    _watchField.frame = NSMakeRect(10, 42, NSWidth(content.bounds) - 100, 22);
    _watchField.autoresizingMask = NSViewWidthSizable;
    _watchField.placeholderString = @"Watch variable or b/d/bt/c/s/q";
    _watchField.font = VBFontUI();
    [content addSubview:_watchField];
    NSButton *watch = [NSButton buttonWithTitle:@"Watch" target:self action:@selector(watchVariable)];
    watch.frame = NSMakeRect(NSWidth(content.bounds) - 82, 40, 72, 26);
    watch.autoresizingMask = NSViewMinXMargin;
    [content addSubview:watch];

    [window center];
    return self;
}

- (void)appendOutput:(NSString *)text
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (!text.length) return;
        NSString *old = self->_output.string ?: @"";
        self->_output.string = [old stringByAppendingString:text];
        [self->_output scrollToEndOfDocument:nil];
    });
}

- (void)setStatusText:(NSString *)text
{
    dispatch_async(dispatch_get_main_queue(), ^{
        self->_status.stringValue = text ?: @"";
    });
}

- (void)writeCommand:(NSString *)command
{
    if (!_input || !_running) return;
    NSData *data = [[command stringByAppendingString:@"\n"] dataUsingEncoding:NSUTF8StringEncoding];
    @try { [_input writeData:data]; } @catch (NSException *exception) { (void)exception; }
}

- (void)handleDebugLine:(NSString *)line
{
    if ([line hasPrefix:@"@@GMBDBG PAUSED "]) {
        NSInteger number = [[line substringFromIndex:17] integerValue];
        [self setStatusText:[NSString stringWithFormat:@"Paused at line %ld", (long)number]];
        if (self.onPausedLine) self.onPausedLine(number);
        return;
    }
    if ([line hasPrefix:@"@@GMBDBG READY"]) {
        [self setStatusText:@"Debugger ready"];
        return;
    }
    if ([line hasPrefix:@"@@GMBDBG CONTINUE"]) {
        [self setStatusText:@"Running..."];
        if (self.onPausedLine) self.onPausedLine(0);
        return;
    }
    if ([line hasPrefix:@"@@GMBDBG STEP"]) {
        [self setStatusText:@"Stepping..."];
        return;
    }
    if ([line hasPrefix:@"@@GMBDBG EXIT"]) {
        _running = NO;
        [self setStatusText:@"Stopped"];
        if (self.onPausedLine) self.onPausedLine(0);
        return;
    }
    if ([line hasPrefix:@"@@GMBDBG STACK"]) {
        NSString *raw = [line substringFromIndex:16];
        NSMutableArray<NSString *> *parts = [NSMutableArray array];
        for (NSString *number in [raw componentsSeparatedByString:@" "]) {
            if (!number.length) continue;
            NSInteger combined = number.integerValue;
            NSString *bestFile = nil;
            NSInteger bestStart = -1, bestLine = 0;
            for (NSDictionary *segment in self.sourceMap) {
                NSInteger start = [segment[@"start"] integerValue];
                if (start <= combined && start > bestStart) {
                    bestFile = segment[@"file"];
                    bestStart = start;
                    bestLine = combined - start + 1;
                }
            }
            if (bestFile.length) [parts addObject:[NSString stringWithFormat:@"%@:%ld", bestFile.lastPathComponent, (long)bestLine]];
            else [parts addObject:number];
        }
        [self appendOutput:[NSString stringWithFormat:@"Stack: %@\n", [parts componentsJoinedByString:@" <- "]]];
        return;
    }
    if ([line hasPrefix:@"@@GMBDBG VALUE "]) {
        [self appendOutput:[line substringFromIndex:16]];
        [self appendOutput:@"\n"];
        return;
    }
    if ([line hasPrefix:@"@@GMBDBG "]) return;
    [self appendOutput:[line stringByAppendingString:@"\n"]];
}

- (void)readStderr
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        int fd = ((NSPipe *)self->_task.standardError).fileHandleForReading.fileDescriptor;
        char buf[4096];
        while (self->_running) {
            ssize_t n = read(fd, buf, sizeof(buf));
            if (n <= 0) break;
            @synchronized (self->_stderrBuffer) {
                [self->_stderrBuffer appendBytes:buf length:(NSUInteger)n];
                while (1) {
                    const char *bytes = self->_stderrBuffer.bytes;
                    NSUInteger len = self->_stderrBuffer.length;
                    const char *nl = memchr(bytes, '\n', len);
                    if (!nl) break;
                    NSUInteger lineLen = (NSUInteger)(nl - bytes);
                    NSString *line = [[NSString alloc] initWithBytes:bytes length:lineLen encoding:NSUTF8StringEncoding] ?: @"";
                    [self->_stderrBuffer replaceBytesInRange:NSMakeRange(0, lineLen + 1) withBytes:NULL length:0];
                    dispatch_async(dispatch_get_main_queue(), ^{ [self handleDebugLine:line]; });
                }
            }
        }
    });
}

- (void)readStdout
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        int fd = ((NSPipe *)self->_task.standardOutput).fileHandleForReading.fileDescriptor;
        char buf[4096];
        while (self->_running) {
            ssize_t n = read(fd, buf, sizeof(buf));
            if (n <= 0) break;
            NSString *text = [[NSString alloc] initWithBytes:buf length:(NSUInteger)n encoding:NSUTF8StringEncoding];
            if (text.length) [self appendOutput:text];
        }
    });
}

- (void)startHost:(NSString *)host
            chunk:(NSString *)chunkPath
          workdir:(NSString *)workdir
      breakpoints:(NSArray<NSNumber *> *)breakpoints
{
    if (_running) [self stopDebug];
    _running = YES;
    _stopped = NO;
    _stderrBuffer = [NSMutableData data];
    [self appendOutput:[NSString stringWithFormat:@"Debug %@\n", chunkPath.lastPathComponent]];

    _task = [[NSTask alloc] init];
    _task.executableURL = [NSURL fileURLWithPath:host];
    NSMutableArray<NSString *> *args = [NSMutableArray arrayWithObjects:chunkPath, @"--debug-interactive", nil];
    for (NSNumber *number in breakpoints) [args addObjectsFromArray:@[@"--break", number.stringValue]];
    _task.arguments = args;
    _task.currentDirectoryURL = [NSURL fileURLWithPath:workdir isDirectory:YES];
    NSPipe *inPipe = [NSPipe pipe];
    NSPipe *outPipe = [NSPipe pipe];
    NSPipe *errPipe = [NSPipe pipe];
    _task.standardInput = inPipe;
    _task.standardOutput = outPipe;
    _task.standardError = errPipe;
    _input = inPipe.fileHandleForWriting;

    __weak typeof(self) weakSelf = self;
    _task.terminationHandler = ^(NSTask *task) {
        dispatch_async(dispatch_get_main_queue(), ^{
            __strong typeof(weakSelf) self = weakSelf;
            if (!self) return;
            self->_running = NO;
            [self setStatusText:[NSString stringWithFormat:@"Exited (%d)", task.terminationStatus]];
            if (self.onPausedLine) self.onPausedLine(0);
            if (self.onExit) self.onExit();
        });
    };
    @try {
        [_task launch];
    } @catch (NSException *exception) {
        _running = NO;
        [self appendOutput:[NSString stringWithFormat:@"Launch failed: %@\n", exception.reason]];
        return;
    }
    [self readStdout];
    [self readStderr];
}

- (void)watchVariable
{
    NSString *name = [_watchField.stringValue stringByTrimmingCharactersInSet:
                      [NSCharacterSet whitespaceAndNewlineCharacterSet]];
    if (!name.length) return;
    NSArray<NSString *> *parts = [name componentsSeparatedByString:@" "];
    NSString *verb = parts.firstObject.lowercaseString;
    if ([verb isEqualToString:@"b"] || [verb isEqualToString:@"d"] ||
        [verb isEqualToString:@"bt"] || [verb isEqualToString:@"c"] ||
        [verb isEqualToString:@"s"] || [verb isEqualToString:@"q"]) {
        [self writeCommand:name];
    } else {
        [self writeCommand:[@"p " stringByAppendingString:name]];
    }
}

- (void)showStack { [self writeCommand:@"bt"]; }

- (void)pauseExecution
{
    if (_running && _task.isRunning) kill(_task.processIdentifier, SIGUSR1);
}

- (void)continueExecution { [self writeCommand:@"c"]; }
- (void)stepExecution { [self writeCommand:@"s"]; }

- (void)stopDebug
{
    if (!_running) return;
    [self writeCommand:@"q"];
    _running = NO;
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.5 * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        if (self->_task.isRunning) [self->_task terminate];
    });
}

- (void)windowWillClose:(NSNotification *)notification
{
    (void)notification;
    [self stopDebug];
}

@end
