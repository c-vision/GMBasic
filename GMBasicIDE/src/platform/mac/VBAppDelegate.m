#import "VBAppDelegate.h"
#import "VBEditorWindow.h"
#import "VBGmbRunner.h"
#import "VBTheme.h"
#import "VBTheme.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#include "core/vb_gmb.h"
#include "core/vb_parse.h"
#include "core/vb_util.h"

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

@implementation VBAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    (void)notification;
    [NSApp setAppearance:[NSAppearance appearanceNamed:NSAppearanceNameAqua]];

    _editor = [[VBEditorWindowController alloc] init];
    [_editor showWindow:nil];
    [_editor.window makeKeyAndOrderFront:nil];
    [NSApp activateIgnoringOtherApps:YES];

    NSString *savedTheme = [VBGmbRunner themeName];
    [VBGmbRunner loadThemeNamed:savedTheme completion:^(NSDictionary *json) {
        if (!json) return;
        VBThemeApplyJSON(json);
        [self.editor refreshTheme];
    }];

    BOOL loaded = NO;
    BOOL cliSession = NSProcessInfo.processInfo.arguments.count > 1;
    if (self.startupPath.length) {
        loaded = [_editor loadProjectAtPath:self.startupPath];
    }
    if (!loaded && cliSession) {
        NSString *sample = [self sampleProjectPath];
        if (sample) loaded = [_editor loadProjectAtPath:sample];
    }
    if (!loaded) [_editor newProject];

    NSString *snapshotPath = nil;
    NSString *windowSnapshotPath = nil;
    NSString *runtimeSnapshotPath = nil;
    NSString *codeSnapshotPath = nil;
    NSString *tabOrderSnapshotPath = nil;
    NSString *runtimeTabSnapshotPath = nil;
    NSString *runtimeAccessKeySnapshotPath = nil;
    NSString *runtimeFocusSnapshotPath = nil;
    NSString *runtimeErrorSnapshotPath = nil;
    NSString *runtimeBreakSnapshotPath = nil;
    NSString *breakpointSnapshotPath = nil;
    NSString *debugHandler = nil;
    BOOL selfTest = NO;
    BOOL designerTest = NO;
    BOOL runtimeListTest = NO;
    BOOL runtimeTabTest = NO;
    BOOL runtimeAccessKeyTest = NO;
    BOOL runtimeFocusTest = NO;
    BOOL runtimeErrorTest = NO;
    BOOL runtimeBreakTest = NO;
    BOOL breakpointTest = NO;
    BOOL completionTest = NO;
    NSString *gmbCheckPath = nil;
    NSString *gmbProjectTestPath = nil;
    NSString *gmbFormExportPath = nil;
    NSString *gmbFormExportOut = nil;
    BOOL gmbNewProjectTest = NO;
    BOOL gmbExportProjectTest = NO;
    BOOL gmbExportAllTest = NO;
    NSString *gmbDebugTestPath = nil;
    NSString *gmbCommand = @"check";
    BOOL themeTest = NO;
    BOOL showCode = NO;
    BOOL runProject = NO;
    NSArray<NSString *> *arguments = NSProcessInfo.processInfo.arguments;
    for (NSUInteger i = 1; i < arguments.count; i++) {
        if ([arguments[i] isEqualToString:@"--snapshot"] && i + 1 < arguments.count) snapshotPath = arguments[++i];
        else if ([arguments[i] isEqualToString:@"--window-snapshot"] && i + 1 < arguments.count) windowSnapshotPath = arguments[++i];
        else if ([arguments[i] isEqualToString:@"--snapshot-code"] && i + 1 < arguments.count) codeSnapshotPath = arguments[++i];
        else if ([arguments[i] isEqualToString:@"--runtime-snapshot"] && i + 1 < arguments.count) runtimeSnapshotPath = arguments[++i];
        else if ([arguments[i] isEqualToString:@"--tab-order-snapshot"] && i + 1 < arguments.count) tabOrderSnapshotPath = arguments[++i];
        else if ([arguments[i] isEqualToString:@"--runtime-list-test"]) runtimeListTest = YES;
        else if ([arguments[i] isEqualToString:@"--runtime-tab-test"]) runtimeTabTest = YES;
        else if ([arguments[i] isEqualToString:@"--runtime-tab-snapshot"] && i + 1 < arguments.count) runtimeTabSnapshotPath = arguments[++i];
        else if ([arguments[i] isEqualToString:@"--runtime-access-key-test"]) runtimeAccessKeyTest = YES;
        else if ([arguments[i] isEqualToString:@"--runtime-access-key-snapshot"] && i + 1 < arguments.count) runtimeAccessKeySnapshotPath = arguments[++i];
        else if ([arguments[i] isEqualToString:@"--runtime-focus-test"]) runtimeFocusTest = YES;
        else if ([arguments[i] isEqualToString:@"--runtime-focus-snapshot"] && i + 1 < arguments.count) runtimeFocusSnapshotPath = arguments[++i];
        else if ([arguments[i] isEqualToString:@"--runtime-error-test"]) runtimeErrorTest = YES;
        else if ([arguments[i] isEqualToString:@"--runtime-break-test"]) runtimeBreakTest = YES;
        else if ([arguments[i] isEqualToString:@"--runtime-break-snapshot"] && i + 1 < arguments.count) runtimeBreakSnapshotPath = arguments[++i];
        else if ([arguments[i] isEqualToString:@"--breakpoint-test"]) breakpointTest = YES;
        else if ([arguments[i] isEqualToString:@"--completion-test"]) completionTest = YES;
        else if ([arguments[i] isEqualToString:@"--gmb-check"] && i + 1 < arguments.count) { gmbCheckPath = arguments[++i]; gmbCommand = @"check"; }
        else if ([arguments[i] isEqualToString:@"--gmb-build"] && i + 1 < arguments.count) { gmbCheckPath = arguments[++i]; gmbCommand = @"build"; }
        else if ([arguments[i] isEqualToString:@"--gmb-run"] && i + 1 < arguments.count) { gmbCheckPath = arguments[++i]; gmbCommand = @"run"; }
        else if ([arguments[i] isEqualToString:@"--gmb-project-test"] && i + 1 < arguments.count) gmbProjectTestPath = arguments[++i];
        else if ([arguments[i] isEqualToString:@"--gmb-new-project-test"]) gmbNewProjectTest = YES;
        else if ([arguments[i] isEqualToString:@"--gmb-export-project-test"]) gmbExportProjectTest = YES;
        else if ([arguments[i] isEqualToString:@"--gmb-export-all-test"]) gmbExportAllTest = YES;
        else if ([arguments[i] isEqualToString:@"--gmb-debug-test"] && i + 1 < arguments.count) gmbDebugTestPath = arguments[++i];
        else if ([arguments[i] isEqualToString:@"--gmb-form-export"] && i + 1 < arguments.count) gmbFormExportPath = arguments[++i];
        else if ([arguments[i] isEqualToString:@"--gmb-form-export-out"] && i + 1 < arguments.count) gmbFormExportOut = arguments[++i];
        else if ([arguments[i] isEqualToString:@"--theme-test"]) themeTest = YES;
        else if ([arguments[i] isEqualToString:@"--breakpoint-snapshot"] && i + 1 < arguments.count) breakpointSnapshotPath = arguments[++i];
        else if ([arguments[i] isEqualToString:@"--runtime-error-snapshot"] && i + 1 < arguments.count) runtimeErrorSnapshotPath = arguments[++i];
        else if ([arguments[i] isEqualToString:@"--handler"] && i + 1 < arguments.count) debugHandler = arguments[++i];
        else if ([arguments[i] isEqualToString:@"--selftest"]) selfTest = YES;
        else if ([arguments[i] isEqualToString:@"--designer-test"]) designerTest = YES;
        else if ([arguments[i] isEqualToString:@"--code"]) showCode = YES;
        else if ([arguments[i] isEqualToString:@"--run"]) runProject = YES;
    }
    if (showCode) [_editor showCode];
    if (runProject) [_editor runProject];
    if (runtimeListTest) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
            BOOL ok = [self.editor runRuntimeListSelfTest];
            fprintf(stderr, "runtime-list-selftest: result=%s\n", ok ? "PASS" : "FAIL");
            exit(ok ? 0 : 1);
        });
        return;
    }
    if (runtimeTabTest) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
            BOOL ok = [self.editor runRuntimeTabSelfTest];
            if (runtimeTabSnapshotPath.length) {
                NSWindow *runtime = self.editor.runtimeTabTestWindow;
                if (runtime) [self writeWindow:runtime toPath:runtimeTabSnapshotPath];
            }
            exit(ok ? 0 : 1);
        });
        return;
    }
    if (themeTest) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
            NSArray<NSString *> *names = [VBGmbRunner themeNames];
            BOOL known = [names containsObject:@"vb6"] && [names containsObject:@"qbasic"];
            [VBGmbRunner loadThemeNamed:@"qbasic" completion:^(NSDictionary *json) {
                if (!json) { fprintf(stderr, "theme: FAIL (json)\n"); exit(1); }
                VBThemeApplyJSON(json);
                NSColor *face = [VBColorFace() colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
                BOOL dark = face.redComponent < 0.1 && face.greenComponent < 0.1 && face.blueComponent > 0.3;
                fprintf(stderr, "theme: %s (names=%lu face=%.2f/%.2f/%.2f)\n",
                        (known && dark) ? "PASS" : "FAIL", (unsigned long)names.count,
                        face.redComponent, face.greenComponent, face.blueComponent);
                exit((known && dark) ? 0 : 1);
            }];
        });
        return;
    }
    if (gmbDebugTestPath.length) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
            NSString *tmp = [NSTemporaryDirectory() stringByAppendingPathComponent:
                             [NSString stringWithFormat:@"gmbide-debug-test-%d", getpid()]];
            [[NSFileManager defaultManager] removeItemAtPath:tmp error:nil];
            [[NSFileManager defaultManager] createDirectoryAtPath:tmp withIntermediateDirectories:YES attributes:nil error:nil];
            NSString *chunk = [tmp stringByAppendingPathComponent:@"source.chunk"];
            NSString *compileOut = nil;
            BOOL compiled = [VBGmbRunner compileChunkForSource:gmbDebugTestPath toPath:chunk output:&compileOut];
            NSString *host = [VBGmbRunner vmHostPath];
            NSString *debugOut = @"";
            int status = 1;
            if (compiled && host.length) {
                NSTask *task = [[NSTask alloc] init];
                task.executableURL = [NSURL fileURLWithPath:host];
                task.arguments = @[chunk, @"--debug-interactive", @"--break", @"2"];
                task.currentDirectoryURL = [NSURL fileURLWithPath:gmbDebugTestPath.stringByDeletingLastPathComponent isDirectory:YES];
                NSPipe *inPipe = [NSPipe pipe];
                NSPipe *errPipe = [NSPipe pipe];
                task.standardInput = inPipe;
                task.standardOutput = [NSPipe pipe];
                task.standardError = errPipe;
                @try { [task launch]; } @catch (NSException *e) { debugOut = e.reason ?: @""; }
                if (task.isRunning || task.terminationStatus == 0) {
                    NSData *commands = [@"bt\ns\nc\nq\n" dataUsingEncoding:NSUTF8StringEncoding];
                    @try { [inPipe.fileHandleForWriting writeData:commands]; } @catch (NSException *e) { (void)e; }
                    NSData *err = [errPipe.fileHandleForReading readDataToEndOfFile];
                    [task waitUntilExit];
                    debugOut = [[NSString alloc] initWithData:err encoding:NSUTF8StringEncoding] ?: @"";
                    status = [debugOut containsString:@"@@GMBDBG PAUSED"] && [debugOut containsString:@"@@GMBDBG STACK"] && [debugOut containsString:@"@@GMBDBG EXIT"] ? 0 : 1;
                }
            }
            fprintf(stderr, "debug: %s (compiled=%d status=%d)\n%s",
                    status == 0 ? "PASS" : "FAIL", compiled, status, debugOut.UTF8String);
            [[NSFileManager defaultManager] removeItemAtPath:tmp error:nil];
            exit(status == 0 ? 0 : 1);
        });
        return;
    }
    if (gmbExportAllTest) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
            NSString *tmp = [NSTemporaryDirectory() stringByAppendingPathComponent:
                             [NSString stringWithFormat:@"gmbide-export-all-%d", getpid()]];
            [[NSFileManager defaultManager] removeItemAtPath:tmp error:nil];
            NSString *error = nil;
            BOOL created = [VBGmbRunner createProjectAtPath:tmp name:@"ExportAll" error:&error];
            NSUInteger count = created ? [self.editor exportAllFormsToProject:tmp error:&error] : 0;
            NSDictionary *manifest = [VBGmbRunner manifestForProject:tmp];
            NSArray *forms = manifest[@"forms"];
            BOOL ok = created && count > 0 && forms.count == count;
            fprintf(stderr, "export-all: %s (forms=%lu manifests=%lu)\n",
                    ok ? "PASS" : "FAIL", (unsigned long)count, (unsigned long)forms.count);
            [[NSFileManager defaultManager] removeItemAtPath:tmp error:nil];
            exit(ok ? 0 : 1);
        });
        return;
    }
    if (gmbExportProjectTest) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
            NSString *tmp = [NSTemporaryDirectory() stringByAppendingPathComponent:
                             [NSString stringWithFormat:@"gmbide-export-project-%d", getpid()]];
            [[NSFileManager defaultManager] removeItemAtPath:tmp error:nil];
            NSString *error = nil;
            BOOL created = [VBGmbRunner createProjectAtPath:tmp name:@"ExportProject" error:&error];
            VbForm *form = vb_form_load("samples/HelloWorld/frmHello.frm");
            char *code = form ? vb_gmb_form_export(form) : NULL;
            NSString *out = [tmp stringByAppendingPathComponent:@"Form1.ui.bas"];
            BOOL written = code && vb_write_file(out.UTF8String, code, strlen(code)) != 0;
            free(code);
            if (form) vb_form_free(form);
            BOOL added = written && [VBGmbRunner addSource:out form:@"Form1" toProject:tmp error:&error];
            NSDictionary *manifest = [VBGmbRunner manifestForProject:tmp];
            NSArray *sources = manifest[@"sources"];
            NSArray *forms = manifest[@"forms"];
            BOOL inManifest = [sources containsObject:@"Form1.ui.bas"] && forms.count == 1;
            NSString *output = nil;
            int status = (created && added && inManifest)
                ? [VBGmbRunner runSynchronousCommand:@"check" project:tmp
                                               entry:[tmp stringByAppendingPathComponent:@"Main.bas"]
                                              output:&output] : 1;
            BOOL ok = created && added && inManifest && status == 0;
            fprintf(stderr, "export-project: %s (sources=%lu forms=%lu status=%d)\n",
                    ok ? "PASS" : "FAIL", (unsigned long)sources.count,
                    (unsigned long)forms.count, status);
            [[NSFileManager defaultManager] removeItemAtPath:tmp error:nil];
            exit(ok ? 0 : 1);
        });
        return;
    }
    if (gmbFormExportPath.length) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
            VbForm *form = vb_form_load(gmbFormExportPath.UTF8String);
            char *code = form ? vb_gmb_form_export(form) : NULL;
            NSString *tmpDir = [NSTemporaryDirectory() stringByAppendingPathComponent:
                                [NSString stringWithFormat:@"gmbide-form-export-%d", getpid()]];
            [[NSFileManager defaultManager] removeItemAtPath:tmpDir error:nil];
            [[NSFileManager defaultManager] createDirectoryAtPath:tmpDir withIntermediateDirectories:YES attributes:nil error:nil];
            NSString *out = gmbFormExportOut.length ? gmbFormExportOut :
                [tmpDir stringByAppendingPathComponent:@"Form1.ui.bas"];
            BOOL written = code && vb_write_file(out.UTF8String, code, strlen(code)) != 0;
            free(code);
            if (form) vb_form_free(form);
            NSString *output = nil;
            int status = written ? [VBGmbRunner runSynchronousCommand:@"check"
                                                               project:out.stringByDeletingLastPathComponent
                                                                 entry:out output:&output] : 1;
            fprintf(stderr, "form-export: %s (written=%d status=%d out=%s)\n%s",
                    (written && status == 0) ? "PASS" : "FAIL", written, status,
                    out.UTF8String, output.UTF8String ?: "");
            if (!gmbFormExportOut.length) [[NSFileManager defaultManager] removeItemAtPath:tmpDir error:nil];
            exit((written && status == 0) ? 0 : 1);
        });
        return;
    }
    if (gmbNewProjectTest) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
            NSString *path = [NSTemporaryDirectory() stringByAppendingPathComponent:
                              [NSString stringWithFormat:@"gmbide-new-project-%d", getpid()]];
            [[NSFileManager defaultManager] removeItemAtPath:path error:nil];
            NSString *error = nil;
            BOOL created = [VBGmbRunner createProjectAtPath:path name:@"NewProject" error:&error];
            NSString *entry = [VBGmbRunner entryForProject:path];
            NSString *output = nil;
            int status = created ? [VBGmbRunner runSynchronousCommand:@"check" project:path entry:entry output:&output] : 1;
            BOOL ok = created && entry.length && status == 0;
            fprintf(stderr, "new-project: %s (error=%s)\n", ok ? "PASS" : "FAIL", error.UTF8String ?: "-");
            [[NSFileManager defaultManager] removeItemAtPath:path error:nil];
            exit(ok ? 0 : 1);
        });
        return;
    }
    if (gmbProjectTestPath.length) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
            [VBGmbRunner setProjectPath:gmbProjectTestPath];
            NSString *stored = [VBGmbRunner projectPath];
            NSString *entry = [VBGmbRunner entryForProject:stored ?: @""];
            NSDictionary *manifest = [VBGmbRunner manifestForProject:stored ?: @""];
            NSString *name = [manifest[@"name"] isKindOfClass:NSString.class] ? manifest[@"name"] : nil;
            NSString *output = nil;
            int status = [VBGmbRunner runSynchronousCommand:@"check" project:stored ?: @"" entry:entry output:&output];
            NSString *expected = gmbProjectTestPath.isAbsolutePath ? gmbProjectTestPath :
                [[[NSFileManager defaultManager] currentDirectoryPath] stringByAppendingPathComponent:gmbProjectTestPath];
            BOOL ok = [stored isEqualToString:expected.stringByStandardizingPath] && entry.length &&
                      [name isEqualToString:@"HelloGMBasic"] && status == 0;
            fprintf(stderr, "project: %s (stored=%s entry=%s)\n", ok ? "PASS" : "FAIL",
                    stored.UTF8String ?: "(null)", entry.lastPathComponent.UTF8String ?: "(null)");
            exit(ok ? 0 : 1);
        });
        return;
    }
    if (gmbCheckPath.length) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
            NSString *entry = [VBGmbRunner entryForProject:gmbCheckPath];
            NSString *output = nil;
            int status = [VBGmbRunner runSynchronousCommand:gmbCommand
                                                    project:gmbCheckPath
                                                      entry:entry
                                                     output:&output];
            fprintf(stderr, "%s", output.UTF8String);
            exit(status == 0 ? 0 : 1);
        });
        return;
    }
    if (completionTest) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
            BOOL ok = [self.editor runCompletionSelfTest];
            exit(ok ? 0 : 1);
        });
        return;
    }
    if (breakpointTest || breakpointSnapshotPath.length) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
            BOOL ok = [self.editor runBreakpointSelfTest:breakpointSnapshotPath];
            exit(ok ? 0 : 1);
        });
        return;
    }
    if (runtimeBreakTest || runtimeBreakSnapshotPath.length) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
            BOOL ok = [self.editor runRuntimeBreakSelfTest:runtimeBreakSnapshotPath];
            exit(ok ? 0 : 1);
        });
        return;
    }
    if (runtimeErrorTest) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
            BOOL ok = [self.editor runRuntimeErrorSelfTest];
            exit(ok ? 0 : 1);
        });
        return;
    }
    if (runtimeErrorSnapshotPath.length) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
            /* The End/Debug dialog runs its own modal loop: capture it from a timer added
               to that loop, then abort the modal so the process can exit. */
            NSTimer *captureTimer = [NSTimer timerWithTimeInterval:1.2 repeats:NO block:^(NSTimer *timer) {
                (void)timer;
                NSWindow *dialog = NSApp.keyWindow;
                if (dialog && dialog != self.editor.window) {
                    [self writeWindow:dialog toPath:runtimeErrorSnapshotPath];
                }
                [NSApp abortModal];
                dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.3 * NSEC_PER_SEC)),
                               dispatch_get_main_queue(), ^{
                    [NSApp terminate:nil];
                });
            }];
            [[NSRunLoop mainRunLoop] addTimer:captureTimer forMode:NSModalPanelRunLoopMode];
            [[NSRunLoop mainRunLoop] addTimer:captureTimer forMode:NSDefaultRunLoopMode];
            [self.editor showRuntimeErrorDialogForSnapshot];
        });
        return;
    }
    if (runtimeFocusTest) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
            BOOL ok = [self.editor runRuntimeFocusSelfTest];
            if (runtimeFocusSnapshotPath.length) {
                NSWindow *runtime = self.editor.runtimeFocusTestWindow;
                if (runtime) [self writeWindow:runtime toPath:runtimeFocusSnapshotPath];
            }
            exit(ok ? 0 : 1);
        });
        return;
    }
    if (runtimeAccessKeyTest) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
            BOOL ok = [self.editor runRuntimeAccessKeySelfTest];
            if (runtimeAccessKeySnapshotPath.length) {
                NSWindow *runtime = self.editor.runtimeAccessKeyTestWindow;
                if (runtime) [self writeWindow:runtime toPath:runtimeAccessKeySnapshotPath];
            }
            exit(ok ? 0 : 1);
        });
        return;
    }
    if (tabOrderSnapshotPath.length) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
            /* The dialog runs a modal session, which drains timers but not the main
               dispatch queue: capture from an NSTimer added to the modal run loop. */
            NSTimer *captureTimer = [NSTimer timerWithTimeInterval:0.5 repeats:NO block:^(NSTimer *timer) {
                (void)timer;
                NSWindow *dialog = NSApp.keyWindow;
                if (dialog && dialog != self.editor.window) {
                    [self writeWindow:dialog toPath:tabOrderSnapshotPath];
                }
                [NSApp abortModal];
                dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(0.3 * NSEC_PER_SEC)),
                               dispatch_get_main_queue(), ^{
                    [NSApp terminate:nil];
                });
            }];
            [[NSRunLoop mainRunLoop] addTimer:captureTimer forMode:NSModalPanelRunLoopMode];
            [[NSRunLoop mainRunLoop] addTimer:captureTimer forMode:NSDefaultRunLoopMode];
            [self.editor showTabOrder:nil];
        });
        return;
    }
    if (snapshotPath.length || windowSnapshotPath.length || runtimeSnapshotPath.length ||
        codeSnapshotPath.length || selfTest || designerTest) {
        dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(1.0 * NSEC_PER_SEC)),
                       dispatch_get_main_queue(), ^{
            BOOL designerPassed = YES;
            if (selfTest) fprintf(stderr, "selftest: %s\n", self.editor.diagnostics.UTF8String);
            /* Selection test first, then snapshots (so the evidence shows the multi-selection),
               then the Format menu test which rearranges the fixture. */
            if (designerTest && ![self.editor runDesignerSelectionSelfTest]) designerPassed = NO;
            if (debugHandler.length) [self.editor runDebugHandler:debugHandler];
            if (snapshotPath.length) [self captureSnapshot:snapshotPath];
            if (windowSnapshotPath.length) [self captureWindowSnapshot:windowSnapshotPath];
            if (runtimeSnapshotPath.length) [self captureRuntimeSnapshot:runtimeSnapshotPath];
            if (codeSnapshotPath.length && [self.editor writeCodeEditorSnapshot:codeSnapshotPath]) {
                fprintf(stderr, "code snapshot: %s\n", codeSnapshotPath.UTF8String);
            }
            if (designerTest) {
                if (![self.editor runFormatSelfTest]) designerPassed = NO;
                fprintf(stderr, "designer-selftest: result=%s\n", designerPassed ? "PASS" : "FAIL");
                fflush(stderr);
                exit(designerPassed ? 0 : 1);
            }
            [NSApp terminate:nil];
        });
    }
}

- (BOOL)writeWindow:(NSWindow *)window toPath:(NSString *)path
{
    typedef CGImageRef (*CaptureFn)(CGRect, CGWindowListOption, CGWindowID, CGWindowImageOption);
    CaptureFn capture = (CaptureFn)dlsym(RTLD_DEFAULT, "CGWindowListCreateImage");
    if (!capture || !window) return NO;
    [window.contentView layoutSubtreeIfNeeded];
    [window displayIfNeeded];
    [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.15]];
    CGWindowID number = (CGWindowID)window.windowNumber;
    CGImageRef image = capture(CGRectNull, kCGWindowListOptionIncludingWindow,
                               number, kCGWindowImageBoundsIgnoreFraming);
    if (!image) return NO;
    NSBitmapImageRep *rep = [[NSBitmapImageRep alloc] initWithCGImage:image];
    CGImageRelease(image);
    NSData *data = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    if (![data writeToFile:path atomically:YES]) return NO;
    fprintf(stderr, "window snapshot: %s (%lu bytes)\n", path.UTF8String, (unsigned long)data.length);
    return YES;
}

- (void)captureWindowSnapshot:(NSString *)path
{
    [self writeWindow:self.editor.window toPath:path];
}

- (void)captureRuntimeSnapshot:(NSString *)path
{
    for (NSWindow *window in NSApp.windows) {
        if (window == self.editor.window || !window.isVisible) continue;
        if ([window.windowController isKindOfClass:NSClassFromString(@"VBRuntimeWindow")]) {
            [self writeWindow:window toPath:path];
            return;
        }
    }
}

- (void)captureSnapshot:(NSString *)path
{
    NSWindow *window = self.editor.window;
    NSView *view = window.contentView;
    [window displayIfNeeded];
    [view layoutSubtreeIfNeeded];
    [view displayIfNeeded];
    NSBitmapImageRep *rep = [view bitmapImageRepForCachingDisplayInRect:view.bounds];
    if (!rep) return;
    [view cacheDisplayInRect:view.bounds toBitmapImageRep:rep];
    NSData *data = [rep representationUsingType:NSBitmapImageFileTypePNG properties:@{}];
    if ([data writeToFile:path atomically:YES]) {
        fprintf(stderr, "snapshot: %s (%lu bytes, %.0fx%.0f)\n", path.UTF8String,
                (unsigned long)data.length, NSWidth(view.bounds), NSHeight(view.bounds));
    }
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
    (void)sender;
    return YES;
}

- (void)application:(NSApplication *)application openFiles:(NSArray<NSString *> *)filenames
{
    (void)application;
    if (filenames.count > 0) [_editor loadProjectAtPath:filenames.firstObject];
    [NSApp replyToOpenOrPrint:NSApplicationDelegateReplySuccess];
}

- (NSString *)sampleProjectPath
{
    NSString *path = [[NSBundle mainBundle] pathForResource:@"HelloWorld" ofType:@"vbp" inDirectory:@"Samples/HelloWorld"];
    if (path.length) return path;
    NSString *relative = [[NSFileManager defaultManager] currentDirectoryPath];
    relative = [relative stringByAppendingPathComponent:@"samples/HelloWorld/HelloWorld.vbp"];
    if ([[NSFileManager defaultManager] fileExistsAtPath:relative]) return relative;
    return nil;
}

#pragma mark - Menu

- (NSMenuItem *)item:(NSString *)title action:(SEL)action key:(NSString *)key target:(id)target
{
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title action:action keyEquivalent:key ?: @""];
    item.target = target;
    return item;
}

- (void)buildMenu
{
    NSMenu *main = [[NSMenu alloc] initWithTitle:@"MainMenu"];

    NSMenuItem *appItem = [[NSMenuItem alloc] initWithTitle:@"GMBasicIDE" action:nil keyEquivalent:@""];
    NSMenu *appMenu = [[NSMenu alloc] initWithTitle:@"GMBasicIDE"];
    [appMenu addItem:[self item:@"About GMBasicIDE" action:@selector(showAbout:) key:@"" target:self]];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItem:[self item:@"Hide GMBasicIDE" action:@selector(hide:) key:@"h" target:nil]];
    [appMenu addItem:[self item:@"Hide Others" action:@selector(hideOtherApplications:) key:@"h" target:nil]];
    appMenu.itemArray.lastObject.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagOption;
    [appMenu addItem:[self item:@"Show All" action:@selector(unhideAllApplications:) key:@"" target:nil]];
    [appMenu addItem:[NSMenuItem separatorItem]];
    [appMenu addItem:[self item:@"Quit GMBasicIDE" action:@selector(terminate:) key:@"q" target:nil]];
    appItem.submenu = appMenu;
    [main addItem:appItem];

    NSMenuItem *fileItem = [[NSMenuItem alloc] initWithTitle:@"File" action:nil keyEquivalent:@""];
    NSMenu *fileMenu = [[NSMenu alloc] initWithTitle:@"File"];
    [fileMenu addItem:[self item:@"New Project" action:@selector(newProject:) key:@"n" target:self]];
    [fileMenu addItem:[self item:@"Open Project…" action:@selector(openProject:) key:@"o" target:self]];
    [fileMenu addItem:[self item:@"Open Sample Project" action:@selector(openSampleProject:) key:@"" target:self]];
    [fileMenu addItem:[NSMenuItem separatorItem]];
    [fileMenu addItem:[self item:@"Save Project" action:@selector(saveProject:) key:@"s" target:self]];
    NSMenuItem *saveAs = [self item:@"Save Project As…" action:@selector(saveProjectAs:) key:@"s" target:self];
    saveAs.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagShift;
    [fileMenu addItem:saveAs];
    [fileMenu addItem:[NSMenuItem separatorItem]];
    NSMenuItem *addForm = [self item:@"Add Form" action:@selector(addForm:) key:@"n" target:self];
    addForm.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagShift;
    [fileMenu addItem:addForm];
    [fileMenu addItem:[NSMenuItem separatorItem]];
    [fileMenu addItem:[self item:@"Close Window" action:@selector(performClose:) key:@"w" target:nil]];
    fileItem.submenu = fileMenu;
    [main addItem:fileItem];

    NSMenuItem *editItem = [[NSMenuItem alloc] initWithTitle:@"Edit" action:nil keyEquivalent:@""];
    NSMenu *editMenu = [[NSMenu alloc] initWithTitle:@"Edit"];
    [editMenu addItem:[self item:@"Undo" action:@selector(undo:) key:@"z" target:nil]];
    NSMenuItem *redo = [self item:@"Redo" action:@selector(redo:) key:@"z" target:nil];
    redo.keyEquivalentModifierMask = NSEventModifierFlagCommand | NSEventModifierFlagShift;
    [editMenu addItem:redo];
    [editMenu addItem:[NSMenuItem separatorItem]];
    [editMenu addItem:[self item:@"Cut" action:@selector(cut:) key:@"x" target:nil]];
    [editMenu addItem:[self item:@"Copy" action:@selector(copy:) key:@"c" target:nil]];
    [editMenu addItem:[self item:@"Paste" action:@selector(paste:) key:@"v" target:nil]];
    [editMenu addItem:[self item:@"Delete" action:@selector(delete:) key:@"" target:nil]];
    [editMenu addItem:[self item:@"Select All" action:@selector(selectAll:) key:@"a" target:nil]];
    editItem.submenu = editMenu;
    [main addItem:editItem];

    NSMenuItem *viewItem = [[NSMenuItem alloc] initWithTitle:@"View" action:nil keyEquivalent:@""];
    NSMenu *viewMenu = [[NSMenu alloc] initWithTitle:@"View"];
    [viewMenu addItem:[self item:@"Designer" action:@selector(showDesigner:) key:@"1" target:self]];
    [viewMenu addItem:[self item:@"Code" action:@selector(showCode:) key:@"2" target:self]];
    viewItem.submenu = viewMenu;
    [main addItem:viewItem];

    NSMenuItem *runItem = [[NSMenuItem alloc] initWithTitle:@"Run" action:nil keyEquivalent:@""];
    NSMenu *runMenu = [[NSMenu alloc] initWithTitle:@"Run"];
    NSMenuItem *start = [self item:@"Start" action:@selector(runProject:) key:@"" target:self];
    start.keyEquivalent = [NSString stringWithFormat:@"%C", (unichar)NSF5FunctionKey];
    start.keyEquivalentModifierMask = 0;
    [runMenu addItem:start];
    NSMenuItem *stop = [self item:@"Stop" action:@selector(stopProject:) key:@"" target:self];
    stop.keyEquivalent = [NSString stringWithFormat:@"%C", (unichar)NSF5FunctionKey];
    stop.keyEquivalentModifierMask = NSEventModifierFlagShift;
    [runMenu addItem:stop];
    runItem.submenu = runMenu;
    [main addItem:runItem];

    NSMenuItem *windowItem = [[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent:@""];
    NSMenu *windowMenu = [[NSMenu alloc] initWithTitle:@"Window"];
    [windowMenu addItem:[self item:@"Minimize" action:@selector(performMiniaturize:) key:@"m" target:nil]];
    [windowMenu addItem:[self item:@"Zoom" action:@selector(performZoom:) key:@"" target:nil]];
    windowItem.submenu = windowMenu;
    [main addItem:windowItem];
    NSApp.windowsMenu = windowMenu;

    NSApp.mainMenu = main;
}

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
    SEL action = menuItem.action;
    if (action == @selector(runProject:)) return _editor.hasProject && !_editor.isRunning;
    if (action == @selector(stopProject:)) return _editor.isRunning;
    if (action == @selector(saveProject:) || action == @selector(saveProjectAs:) ||
        action == @selector(addForm:) || action == @selector(showDesigner:) ||
        action == @selector(showCode:)) {
        return _editor.hasProject;
    }
    return YES;
}

#pragma mark - Actions

- (void)newProject:(id)sender
{
    (void)sender;
    [_editor newProject];
    [_editor.window makeKeyAndOrderFront:nil];
}

- (void)openProject:(id)sender
{
    (void)sender;
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.title = @"Open Visual Basic Project";
    panel.allowedContentTypes = @[[UTType typeWithFilenameExtension:@"vbp"]];
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = NO;
    if ([panel runModal] == NSModalResponseOK && panel.URL) {
        [_editor loadProjectAtPath:panel.URL.path];
    }
}

- (void)openSampleProject:(id)sender
{
    (void)sender;
    NSString *path = [self sampleProjectPath];
    if (!path) {
        NSAlert *alert = [[NSAlert alloc] init];
        alert.messageText = @"Sample project not found";
        alert.informativeText = @"The HelloWorld sample was not found inside the application bundle.";
        [alert addButtonWithTitle:@"OK"];
        [alert runModal];
        return;
    }
    [_editor loadProjectAtPath:path];
}

- (void)saveProject:(id)sender
{
    (void)sender;
    [_editor saveProject];
}

- (void)saveProjectAs:(id)sender
{
    (void)sender;
    [_editor saveProjectAs];
}

- (void)addForm:(id)sender
{
    (void)sender;
    [_editor addForm];
}

- (void)runProject:(id)sender
{
    (void)sender;
    [_editor runProject];
}

- (void)stopProject:(id)sender
{
    (void)sender;
    [_editor stopProject];
}

- (void)showDesigner:(id)sender
{
    (void)sender;
    [_editor showDesigner];
}

- (void)showCode:(id)sender
{
    (void)sender;
    [_editor showCode];
}

- (void)showAbout:(id)sender
{
    (void)sender;
    NSAlert *alert = [[NSAlert alloc] init];
    alert.messageText = @"GMBasicIDE";
    alert.informativeText = @"Native macOS IDE for GMBasic projects.\n\nForm designer, GMBasic source editor, build, debug and terminal.";
    [alert addButtonWithTitle:@"OK"];
    [alert runModal];
}

@end
