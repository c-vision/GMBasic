#import "VBGmbSourceWindow.h"
#import "VBCodeEditor.h"
#import "VBGmbRunner.h"
#import "VBGmbDebugWindow.h"

@interface VBGmbSourceWindowController () <VBCodeEditorDelegate>
@end

static NSMutableArray<VBGmbSourceWindowController *> *gDebugSourceWindows;

static NSString *VBFileForCombined(NSArray<NSDictionary *> *segments, NSInteger combined)
{
    NSString *best = nil;
    NSInteger bestStart = -1;
    for (NSDictionary *segment in segments) {
        NSInteger start = [segment[@"start"] integerValue];
        if (start <= combined && start > bestStart) {
            best = segment[@"file"];
            bestStart = start;
        }
    }
    return best;
}

static NSInteger VBCombinedLineForFile(NSArray<NSDictionary *> *segments, NSString *file, NSInteger line)
{
    for (NSUInteger i = 0; i < segments.count; i++) {
        NSString *path = segments[i][@"file"];
        if (![path isEqualToString:file]) continue;
        NSInteger start = [segments[i][@"start"] integerValue];
        return start + line - 1;
    }
    return line;
}

static NSInteger VBFileLineForCombined(NSArray<NSDictionary *> *segments, NSString *file, NSInteger combined)
{
    for (NSUInteger i = 0; i < segments.count; i++) {
        NSString *path = segments[i][@"file"];
        if (![path isEqualToString:file]) continue;
        NSInteger start = [segments[i][@"start"] integerValue];
        NSInteger next = i + 1 < segments.count ? [segments[i + 1][@"start"] integerValue] : NSIntegerMax;
        if (combined >= start && combined < next) return combined - start + 1;
    }
    return 0;
}

@implementation VBGmbSourceWindowController {
    VBCodeEditor *_editor;
    NSString *_path;
    VBGmbDebugWindow *_debugWindow;
}

- (instancetype)initWithPath:(NSString *)path
{
    NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 820, 560)
                                                   styleMask:(NSWindowStyleMaskTitled |
                                                              NSWindowStyleMaskClosable |
                                                              NSWindowStyleMaskResizable)
                                                     backing:NSBackingStoreBuffered
                                                       defer:NO];
    self = [super initWithWindow:window];
    if (!self) return nil;

    _path = [path copy];
    window.title = [NSString stringWithFormat:@"%@ - GMBasic Source", path.lastPathComponent];
    NSView *content = window.contentView;

    CGFloat buttonH = 32;
    _editor = [[VBCodeEditor alloc] initWithFrame:NSMakeRect(0, buttonH,
                                                             NSWidth(content.bounds),
                                                             NSHeight(content.bounds) - buttonH)];
    _editor.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    _editor.gmbLanguage = YES;
    [_editor setCode:[NSString stringWithContentsOfFile:path encoding:NSUTF8StringEncoding error:nil] ?: @""];
    _editor.delegate = self;
    NSString *project = [VBGmbRunner projectPath];
    if (project.length && [path hasPrefix:[project stringByAppendingString:@"/"]]) {
        NSArray<NSNumber *> *saved = [VBGmbRunner breakpointsForSource:path project:project];
        NSMutableIndexSet *lines = [NSMutableIndexSet indexSet];
        for (NSNumber *number in saved) [lines addIndex:number.unsignedIntegerValue];
        _editor.breakpointLines = lines;
    }
    [content addSubview:_editor];

    NSArray<NSString *> *titles = @[@"Save", @"Check", @"Build", @"Run", @"Debug"];
    SEL actions[] = {@selector(saveSource:), @selector(checkSource:), @selector(buildSource:), @selector(runSource:), @selector(debugSource:)};
    CGFloat x = 6;
    for (NSUInteger i = 0; i < titles.count; i++) {
        NSButton *button = [NSButton buttonWithTitle:titles[i] target:self action:actions[i]];
        button.frame = NSMakeRect(x, 4, 72, 24);
        button.bezelStyle = NSBezelStyleRounded;
        [content addSubview:button];
        x += 78;
    }

    [window center];
    return self;
}

- (void)highlightDebugFile:(NSString *)path line:(NSInteger)line
{
    if (!gDebugSourceWindows) gDebugSourceWindows = [NSMutableArray array];
    VBGmbSourceWindowController *target = nil;
    for (VBGmbSourceWindowController *window in gDebugSourceWindows) {
        if ([window->_path isEqualToString:path]) { target = window; break; }
    }
    if (!target) {
        target = [[VBGmbSourceWindowController alloc] initWithPath:path];
        [gDebugSourceWindows addObject:target];
        [target showWindow:nil];
    }
    [target.window makeKeyAndOrderFront:nil];
    [target->_editor showDebugLine:line];
}

- (void)codeEditorDidChange:(VBCodeEditor *)editor
{
    (void)editor;
    self.window.documentEdited = YES;
}

- (void)codeEditorDidChangeBreakpoints:(VBCodeEditor *)editor
{
    NSString *project = [VBGmbRunner projectPath];
    if (!project.length || ![_path hasPrefix:[project stringByAppendingString:@"/"]]) return;
    NSMutableArray<NSNumber *> *lines = [NSMutableArray array];
    for (NSUInteger line = [editor.breakpointLines firstIndex]; line != NSNotFound;
         line = [editor.breakpointLines indexGreaterThanIndex:line])
        [lines addObject:@(line)];
    NSString *error = nil;
    if (![VBGmbRunner setBreakpoints:lines forSource:_path project:project error:&error] && error) {
        [VBGmbRunner showOutput:error title:@"GMBasic breakpoints"];
    }
}

- (void)refreshTheme
{
    [_editor refreshTheme];
    for (NSView *view in self.window.contentView.subviews) [view setNeedsDisplay:YES];
}

- (void)saveSource:(id)sender
{
    (void)sender;
    NSError *error = nil;
    if (![_editor.code writeToFile:_path atomically:YES encoding:NSUTF8StringEncoding error:&error]) {
        [VBGmbRunner showOutput:[NSString stringWithFormat:@"Save failed: %@\n", error.localizedDescription]
                          title:@"GMBasic source"];
    }
}

- (void)runCommand:(NSString *)command
{
    [self saveSource:nil];
    NSString *project = _path.stringByDeletingLastPathComponent;
    [VBGmbRunner runCommand:command project:project entry:_path completion:^(int status, NSString *output) {
        [VBGmbRunner showOutput:[NSString stringWithFormat:@"%@ (exit %d)\n\n%@",
                                 command.uppercaseString, status, output]
                          title:@"GMBasic"];
    }];
}

- (void)startDebug
{
    [self saveSource:nil];
    NSString *host = [VBGmbRunner vmHostPath];
    if (!host.length) {
        [VBGmbRunner showOutput:@"gmb_vm_host non trovato.\n" title:@"GMBasic debug"];
        return;
    }
    NSString *tmp = [NSTemporaryDirectory() stringByAppendingPathComponent:
                     [NSString stringWithFormat:@"gmbide-debug-%d-%u", getpid(), arc4random()]];
    [[NSFileManager defaultManager] createDirectoryAtPath:tmp withIntermediateDirectories:YES attributes:nil error:nil];
    NSString *chunk = [tmp stringByAppendingPathComponent:@"source.chunk"];
    NSString *mapPath = [tmp stringByAppendingPathComponent:@"source.map"];
    NSString *output = nil;
    if (![VBGmbRunner compileChunkForSource:_path toPath:chunk mapPath:mapPath output:&output]) {
        [VBGmbRunner showOutput:[NSString stringWithFormat:@"Debug compile failed\n\n%@", output ?: @""]
                          title:@"GMBasic debug"];
        [[NSFileManager defaultManager] removeItemAtPath:tmp error:nil];
        return;
    }
    NSArray<NSDictionary *> *segments = [VBGmbRunner sourceMapAtPath:mapPath];
    NSMutableArray<NSNumber *> *breakpoints = [NSMutableArray array];
    NSString *project = [VBGmbRunner projectPath];
    NSDictionary<NSString *, NSArray<NSNumber *> *> *projectBreakpoints = project.length
        ? [VBGmbRunner projectBreakpoints:project] : @{};
    for (NSString *file in projectBreakpoints) {
        NSString *full = file.isAbsolutePath ? file : [project stringByAppendingPathComponent:file];
        for (NSNumber *line in projectBreakpoints[file]) {
            NSInteger combined = VBCombinedLineForFile(segments, full, line.integerValue);
            if (combined > 0 && ![breakpoints containsObject:@(combined)]) [breakpoints addObject:@(combined)];
        }
    }
    if (!breakpoints.count) {
        for (NSUInteger line = [_editor.breakpointLines firstIndex]; line != NSNotFound;
             line = [_editor.breakpointLines indexGreaterThanIndex:line])
            [breakpoints addObject:@(VBCombinedLineForFile(segments, _path, (NSInteger)line))];
    }
    _debugWindow = [[VBGmbDebugWindow alloc] initWithTitle:
                    [NSString stringWithFormat:@"%@ - GMBasic Debug", _path.lastPathComponent]];
    __weak typeof(self) weakSelf = self;
    _debugWindow.onPausedLine = ^(NSInteger line) {
        __strong typeof(weakSelf) self = weakSelf;
        if (!self) return;
        if (line <= 0) { [self->_editor clearDebugLine]; return; }
        NSString *file = VBFileForCombined(segments, line);
        NSInteger fileLine = file ? VBFileLineForCombined(segments, file, line) : 0;
        if (fileLine > 0 && file.length) {
            if ([file isEqualToString:self->_path]) [self->_editor showDebugLine:fileLine];
            else [self highlightDebugFile:file line:fileLine];
        } else {
            [self->_editor clearDebugLine];
        }
    };
    _debugWindow.sourceMap = segments;
    _debugWindow.onExit = ^{
        [[NSFileManager defaultManager] removeItemAtPath:tmp error:nil];
    };
    [_debugWindow showWindow:nil];
    [_debugWindow.window makeKeyAndOrderFront:nil];
    [_debugWindow startHost:host chunk:chunk workdir:_path.stringByDeletingLastPathComponent
                 breakpoints:breakpoints];
}

- (BOOL)isDebugging { return _debugWindow != nil; }
- (void)stepDebug { [_debugWindow stepExecution]; }
- (void)continueDebug { [_debugWindow continueExecution]; }
- (void)stopDebug { [_debugWindow stopDebug]; }
- (void)toggleCurrentBreakpoint { [_editor toggleBreakpointAtLine:[_editor currentLineNumber]]; }
- (void)clearAllBreakpoints { [_editor setBreakpointLines:nil]; [self codeEditorDidChangeBreakpoints:_editor]; }

- (void)debugSource:(id)sender
{
    (void)sender;
    [self startDebug];
}

- (void)checkSource:(id)sender { (void)sender; [self runCommand:@"check"]; }
- (void)buildSource:(id)sender { (void)sender; [self runCommand:@"build"]; }
- (void)runSource:(id)sender { (void)sender; [self runCommand:@"run"]; }

@end
