#import "VBTerminalWindow.h"
#import "VBTheme.h"

#include <signal.h>
#include <unistd.h>

@implementation VBTerminalWindow {
    NSTextView *_output;
    NSTextField *_input;
    NSTask *_task;
    NSFileHandle *_inputHandle;
    BOOL _running;
}

- (instancetype)initWithWorkingDirectory:(NSString *)directory
{
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 780, 440)
                                                   styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                                              NSWindowStyleMaskResizable)
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    self = [super initWithWindow:window];
    if (!self) return nil;
    window.title = @"GMBasic Terminal";
    window.delegate = self;
    window.backgroundColor = VBColorFace();

    NSView *content = window.contentView;
    NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:NSMakeRect(8, 40,
                                                                          NSWidth(content.bounds) - 16,
                                                                          NSHeight(content.bounds) - 48)];
    scroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    scroll.hasVerticalScroller = YES;
    scroll.drawsBackground = YES;
    scroll.backgroundColor = VBColorCodeBackground();
    _output = [[NSTextView alloc] initWithFrame:scroll.bounds];
    _output.editable = NO;
    _output.font = VBFontCode();
    _output.textColor = VBColorCodeText();
    _output.backgroundColor = VBColorCodeBackground();
    _output.string = @"";
    scroll.documentView = _output;
    [content addSubview:scroll];

    _input = [NSTextField textFieldWithString:@""];
    _input.frame = NSMakeRect(8, 8, NSWidth(content.bounds) - 218, 24);
    _input.autoresizingMask = NSViewWidthSizable;
    _input.placeholderString = @"Comando shell…";
    _input.font = VBFontCode();
    _input.target = self;
    _input.action = @selector(sendInput:);
    [content addSubview:_input];

    NSButton *clear = [NSButton buttonWithTitle:@"Clear" target:self action:@selector(clearOutput)];
    clear.frame = NSMakeRect(NSWidth(content.bounds) - 206, 7, 54, 26);
    clear.autoresizingMask = NSViewMinXMargin;
    [content addSubview:clear];
    NSButton *interrupt = [NSButton buttonWithTitle:@"Ctrl+C" target:self action:@selector(interrupt)];
    interrupt.frame = NSMakeRect(NSWidth(content.bounds) - 148, 7, 68, 26);
    interrupt.autoresizingMask = NSViewMinXMargin;
    [content addSubview:interrupt];
    NSButton *send = [NSButton buttonWithTitle:@"Send" target:self action:@selector(sendInput:)];
    send.frame = NSMakeRect(NSWidth(content.bounds) - 76, 7, 68, 26);
    send.autoresizingMask = NSViewMinXMargin;
    [content addSubview:send];

    [self startShell:directory];
    [window center];
    return self;
}

- (void)append:(NSString *)text
{
    if (!text.length) return;
    dispatch_async(dispatch_get_main_queue(), ^{
        NSString *old = self->_output.string ?: @"";
        self->_output.string = [old stringByAppendingString:text];
        [self->_output scrollToEndOfDocument:nil];
    });
}

- (void)readPipe:(NSFileHandle *)handle
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        int fd = handle.fileDescriptor;
        char buf[4096];
        while (self->_running) {
            ssize_t n = read(fd, buf, sizeof(buf));
            if (n <= 0) break;
            NSString *text = [[NSString alloc] initWithBytes:buf length:(NSUInteger)n encoding:NSUTF8StringEncoding];
            if (text.length) [self append:text];
        }
    });
}

- (void)startShell:(NSString *)directory
{
    _running = YES;
    _task = [[NSTask alloc] init];
    _task.executableURL = [NSURL fileURLWithPath:@"/bin/zsh"];
    _task.arguments = @[@"-l"];
    if (directory.length) _task.currentDirectoryURL = [NSURL fileURLWithPath:directory isDirectory:YES];
    NSPipe *inPipe = [NSPipe pipe];
    NSPipe *outPipe = [NSPipe pipe];
    NSPipe *errPipe = [NSPipe pipe];
    _task.standardInput = inPipe;
    _task.standardOutput = outPipe;
    _task.standardError = errPipe;
    _inputHandle = inPipe.fileHandleForWriting;
    __weak typeof(self) weakSelf = self;
    _task.terminationHandler = ^(NSTask *task) {
        dispatch_async(dispatch_get_main_queue(), ^{
            __strong typeof(weakSelf) self = weakSelf;
            if (!self) return;
            self->_running = NO;
            [self append:[NSString stringWithFormat:@"\n[shell exited %d]\n", task.terminationStatus]];
        });
    };
    @try {
        [_task launch];
    } @catch (NSException *exception) {
        _running = NO;
        [self append:[NSString stringWithFormat:@"Cannot start shell: %@\n", exception.reason]];
        return;
    }
    [self readPipe:outPipe.fileHandleForReading];
    [self readPipe:errPipe.fileHandleForReading];
}

- (void)sendCommand:(NSString *)command
{
    if (!_running || !_inputHandle) return;
    NSString *line = command ?: @"";
    [self append:[NSString stringWithFormat:@"$ %@\n", line]];
    NSData *data = [[line stringByAppendingString:@"\n"] dataUsingEncoding:NSUTF8StringEncoding];
    @try { [_inputHandle writeData:data]; } @catch (NSException *exception) { (void)exception; }
}

- (void)sendInput:(id)sender
{
    (void)sender;
    NSString *command = _input.stringValue;
    _input.stringValue = @"";
    [self sendCommand:command];
}

- (void)interrupt
{
    if (_running && _task.isRunning) kill(_task.processIdentifier, SIGINT);
}

- (void)clearOutput
{
    _output.string = @"";
}

- (void)windowWillClose:(NSNotification *)notification
{
    (void)notification;
    _running = NO;
    if (_task.isRunning) [_task terminate];
}

@end
