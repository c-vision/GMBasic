#import "VBGmbRunner.h"

static NSString *const kToolPathKey = @"GMBasicToolPath";
static NSMutableArray<NSWindow *> *gOutputWindows;

@implementation VBGmbRunner

+ (NSString *)detectedToolPath
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *saved = [defaults stringForKey:kToolPathKey];
    if (saved.length && [[NSFileManager defaultManager] isExecutableFileAtPath:saved])
        return saved;

    NSMutableArray<NSString *> *candidates = [NSMutableArray array];
    NSURL *url = [[NSBundle mainBundle] bundleURL];
    for (int i = 0; i < 10 && url; i++) {
        NSString *root = url.path;
        [candidates addObject:[root stringByAppendingPathComponent:@"GMBasic/native/gmb_compiler_c/build/gmb"]];
        [candidates addObject:[root stringByAppendingPathComponent:@"native/gmb_compiler_c/build/gmb"]];
        [candidates addObject:[root stringByAppendingPathComponent:@"GMBasicMonoRepo/GMBasic/native/gmb_compiler_c/build/gmb"]];
        url = [url URLByDeletingLastPathComponent];
    }
    NSString *cwd = [[NSFileManager defaultManager] currentDirectoryPath];
    if (cwd.length) {
        [candidates addObject:[cwd stringByAppendingPathComponent:@"GMBasic/native/gmb_compiler_c/build/gmb"]];
        [candidates addObject:[cwd stringByAppendingPathComponent:@"native/gmb_compiler_c/build/gmb"]];
    }
    // Installazione locale: `gmb` accanto all'app o nel PATH (nessun percorso di package manager).
    [candidates addObjectsFromArray:@[@"/usr/local/bin/gmb"]];
    for (NSString *candidate in candidates) {
        if ([[NSFileManager defaultManager] isExecutableFileAtPath:candidate]) return candidate;
    }
    return nil;
}

+ (NSString *)toolPath
{
    NSString *path = [self detectedToolPath];
    if (path.length) return path;
    return [[NSUserDefaults standardUserDefaults] stringForKey:kToolPathKey] ?: @"";
}

+ (void)setToolPath:(NSString *)path
{
    [[NSUserDefaults standardUserDefaults] setObject:path ?: @"" forKey:kToolPathKey];
}


+ (NSString *)chunkToolPath
{
    NSString *gmb = [self toolPath];
    if (!gmb.length) return nil;
    NSString *candidate = [[gmb stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"gmb_chunk_driver"];
    return [[NSFileManager defaultManager] isExecutableFileAtPath:candidate] ? candidate : nil;
}

+ (NSString *)vmHostPath
{
    NSString *gmb = [self toolPath];
    if (!gmb.length) return nil;
    NSString *native = [[[gmb stringByDeletingLastPathComponent] stringByDeletingLastPathComponent]
                        stringByDeletingLastPathComponent];
    NSArray<NSString *> *candidates = @[
        [native stringByAppendingPathComponent:@"gmb_vm_c/build/gmb_vm_host"],
        [[gmb stringByDeletingLastPathComponent] stringByAppendingPathComponent:@"gmb_vm_host"]
    ];
    for (NSString *candidate in candidates)
        if ([[NSFileManager defaultManager] isExecutableFileAtPath:candidate]) return candidate;
    return nil;
}

+ (BOOL)compileChunkForSource:(NSString *)sourcePath toPath:(NSString *)chunkPath output:(NSString **)output
{
    return [self compileChunkForSource:sourcePath toPath:chunkPath mapPath:nil output:output];
}

+ (BOOL)compileChunkForSource:(NSString *)sourcePath toPath:(NSString *)chunkPath mapPath:(NSString *)mapPath output:(NSString **)output
{
    NSString *tool = [self chunkToolPath];
    if (!tool.length) {
        if (output) *output = @"gmb_chunk_driver non trovato accanto a gmb.\n";
        return NO;
    }
    NSString *cwd = [[NSFileManager defaultManager] currentDirectoryPath] ?: @"/";
    NSString *source = sourcePath.isAbsolutePath ? sourcePath : [cwd stringByAppendingPathComponent:sourcePath];
    source = source.stringByStandardizingPath;
    NSString *chunk = chunkPath.isAbsolutePath ? chunkPath : [cwd stringByAppendingPathComponent:chunkPath];
    chunk = chunk.stringByStandardizingPath;
    NSTask *task = [[NSTask alloc] init];
    task.executableURL = [NSURL fileURLWithPath:tool];
    NSMutableArray<NSString *> *args = [NSMutableArray arrayWithObjects:source, chunk, nil];
    if (mapPath.length) [args addObjectsFromArray:@[@"--map", mapPath]];
    task.arguments = args;
    task.currentDirectoryURL = [NSURL fileURLWithPath:source.stringByDeletingLastPathComponent isDirectory:YES];
    NSPipe *outPipe = [NSPipe pipe];
    NSPipe *errPipe = [NSPipe pipe];
    task.standardOutput = outPipe;
    task.standardError = errPipe;
    @try { [task launch]; } @catch (NSException *exception) {
        if (output) *output = [NSString stringWithFormat:@"Launch failed: %@\n", exception.reason];
        return NO;
    }
    NSData *outData = [outPipe.fileHandleForReading readDataToEndOfFile];
    NSData *errData = [errPipe.fileHandleForReading readDataToEndOfFile];
    [task waitUntilExit];
    NSString *text = [[NSString alloc] initWithData:outData encoding:NSUTF8StringEncoding] ?: @"";
    NSString *err = [[NSString alloc] initWithData:errData encoding:NSUTF8StringEncoding] ?: @"";
    if (err.length) text = [text stringByAppendingString:err];
    if (output) *output = text.length ? text : @"(no output)\n";
    return task.terminationStatus == 0 && [[NSFileManager defaultManager] fileExistsAtPath:chunk];
}

+ (NSArray<NSDictionary *> *)sourceMapAtPath:(NSString *)path
{
    NSData *data = [NSData dataWithContentsOfFile:path];
    if (!data) return @[];
    id json = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
    NSArray *segments = [json isKindOfClass:NSDictionary.class] ? json[@"segments"] : nil;
    return [segments isKindOfClass:NSArray.class] ? segments : @[];
}

+ (NSString *)themeToolPath
{
    NSString *gmb = [self toolPath];
    if (!gmb.length) return nil;
    NSString *native = [[[gmb stringByDeletingLastPathComponent] stringByDeletingLastPathComponent] stringByDeletingLastPathComponent];
    NSString *candidate = [native stringByAppendingPathComponent:@"gmb_runtime_c/build/gmb_theme"];
    if ([[NSFileManager defaultManager] isExecutableFileAtPath:candidate]) return candidate;
    return nil;
}

+ (NSString *)runThemeTool:(NSArray<NSString *> *)arguments
{
    NSString *tool = [self themeToolPath];
    if (!tool.length) return nil;
    NSTask *task = [[NSTask alloc] init];
    task.executableURL = [NSURL fileURLWithPath:tool];
    task.arguments = arguments;
    NSPipe *pipe = [NSPipe pipe];
    task.standardOutput = pipe;
    task.standardError = [NSPipe pipe];
    @try { [task launch]; } @catch (NSException *exception) { return nil; }
    NSData *data = [pipe.fileHandleForReading readDataToEndOfFile];
    [task waitUntilExit];
    return [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding];
}

+ (NSString *)themeName
{
    NSString *name = [[NSUserDefaults standardUserDefaults] stringForKey:@"GMBasicTheme"];
    return name.length ? name : @"vb6";
}

+ (void)setThemeName:(NSString *)name
{
    [[NSUserDefaults standardUserDefaults] setObject:name ?: @"vb6" forKey:@"GMBasicTheme"];
}

+ (NSArray<NSString *> *)themeNames
{
    NSString *out = [self runThemeTool:@[@"--list"]];
    if (!out.length) return @[];
    NSData *data = [out dataUsingEncoding:NSUTF8StringEncoding];
    NSArray *names = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
    if (![names isKindOfClass:NSArray.class]) return @[];
    NSMutableArray<NSString *> *result = [NSMutableArray array];
    for (id name in names) if ([name isKindOfClass:NSString.class]) [result addObject:name];
    return result;
}

+ (void)loadThemeNamed:(NSString *)name completion:(void (^)(NSDictionary *))completion
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSString *out = [self runThemeTool:@[@"--name", name ?: @"turbo"]];
        NSDictionary *json = nil;
        if (out.length) {
            NSData *data = [out dataUsingEncoding:NSUTF8StringEncoding];
            NSError *err = nil;
            id value = [NSJSONSerialization JSONObjectWithData:data options:0 error:&err];
            if ([value isKindOfClass:NSDictionary.class]) json = value;
        }
        dispatch_async(dispatch_get_main_queue(), ^{ if (completion) completion(json); });
    });
}

+ (NSString *)projectPath
{
    NSString *path = [[NSUserDefaults standardUserDefaults] stringForKey:@"GMBasicProjectPath"];
    if (!path.length) return nil;
    BOOL isDir = NO;
    if (![[NSFileManager defaultManager] fileExistsAtPath:path isDirectory:&isDir] || !isDir) return nil;
    return path;
}

+ (void)setProjectPath:(NSString *)path
{
    if (!path.length) {
        [[NSUserDefaults standardUserDefaults] removeObjectForKey:@"GMBasicProjectPath"];
        return;
    }
    NSString *full = path.isAbsolutePath ? path : [[[NSFileManager defaultManager] currentDirectoryPath] stringByAppendingPathComponent:path];
    [[NSUserDefaults standardUserDefaults] setObject:full.stringByStandardizingPath forKey:@"GMBasicProjectPath"];
}

+ (BOOL)createProjectAtPath:(NSString *)projectPath name:(NSString *)name error:(NSString **)error
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *projectName = name.length ? name : projectPath.lastPathComponent;
    NSString *manifest = [projectPath stringByAppendingPathComponent:
                          [projectName stringByAppendingString:@".gmbide.json"]];
    NSString *main = [projectPath stringByAppendingPathComponent:@"Main.bas"];
    if ([fm fileExistsAtPath:manifest] || [fm fileExistsAtPath:main]) {
        if (error) *error = @"Project already contains a GMBasic manifest or Main.bas.";
        return NO;
    }
    NSError *dirError = nil;
    if (![fm createDirectoryAtPath:projectPath withIntermediateDirectories:YES attributes:nil error:&dirError]) {
        if (error) *error = dirError.localizedDescription;
        return NO;
    }
    NSDictionary *json = @{@"version": @1, @"name": projectName, @"language": @"gmbasic",
                           @"backend": @"c99native", @"entry": @"Main.bas",
                           @"sources": @[@"Main.bas"], @"forms": @[]};
    NSData *data = [NSJSONSerialization dataWithJSONObject:json options:NSJSONWritingPrettyPrinted error:nil];
    if (![data writeToFile:manifest atomically:YES]) {
        if (error) *error = @"Cannot write project manifest.";
        return NO;
    }
    NSString *source = [NSString stringWithFormat:@"SUB Main\n  PRINT \"Hello from %@\"\nEND SUB\n", projectName];
    if (![source writeToFile:main atomically:YES encoding:NSUTF8StringEncoding error:nil]) {
        if (error) *error = @"Cannot write Main.bas.";
        return NO;
    }
    return YES;
}

+ (BOOL)addForm:(NSString *)filePath name:(NSString *)formName toProject:(NSString *)projectPath error:(NSString **)error
{
    if (!filePath.length || !projectPath.length) return NO;
    NSDictionary *info = [self manifestForProject:projectPath];
    if (!info[@"manifestPath"]) {
        if (error) *error = @"Project has no .gmbide.json manifest.";
        return NO;
    }
    NSMutableDictionary *manifest = [info mutableCopy];
    [manifest removeObjectForKey:@"manifestPath"];
    NSString *relative = filePath;
    NSString *prefix = [projectPath stringByAppendingString:@"/"];
    if ([filePath hasPrefix:prefix]) relative = [filePath substringFromIndex:prefix.length];
    NSMutableArray *forms = [manifest[@"forms"] isKindOfClass:NSArray.class]
        ? [manifest[@"forms"] mutableCopy] : [NSMutableArray array];
    NSString *name = formName.length ? formName : relative.lastPathComponent.stringByDeletingPathExtension;
    BOOL found = NO;
    for (id entry in forms)
        if ([entry isKindOfClass:NSDictionary.class] && [entry[@"name"] isEqualToString:name]) { found = YES; break; }
    if (!found) [forms addObject:@{@"name": name, @"file": relative}];
    manifest[@"forms"] = forms;
    NSData *data = [NSJSONSerialization dataWithJSONObject:manifest
                                                   options:NSJSONWritingPrettyPrinted | NSJSONWritingSortedKeys
                                                     error:nil];
    if (!data || ![data writeToFile:info[@"manifestPath"] atomically:YES]) {
        if (error) *error = @"Cannot update project manifest.";
        return NO;
    }
    return YES;
}

+ (BOOL)removeFormNamed:(NSString *)formName fromProject:(NSString *)projectPath error:(NSString **)error
{
    NSDictionary *info = [self manifestForProject:projectPath];
    NSString *manifestPath = [info[@"manifestPath"] isKindOfClass:NSString.class] ? info[@"manifestPath"] : nil;
    if (!manifestPath.length) {
        if (error) *error = @"Project has no .gmbide.json manifest.";
        return NO;
    }
    NSMutableDictionary *manifest = [info mutableCopy];
    [manifest removeObjectForKey:@"manifestPath"];
    NSArray *forms = [manifest[@"forms"] isKindOfClass:NSArray.class] ? manifest[@"forms"] : @[];
    NSMutableArray *remaining = [NSMutableArray array];
    for (id entry in forms) {
        if (![entry isKindOfClass:NSDictionary.class]) continue;
        if ([entry[@"name"] isEqualToString:formName]) continue;
        [remaining addObject:entry];
    }
    manifest[@"forms"] = remaining;
    NSData *data = [NSJSONSerialization dataWithJSONObject:manifest options:NSJSONWritingPrettyPrinted error:nil];
    if (!data || ![data writeToFile:manifestPath atomically:YES]) {
        if (error) *error = @"Cannot update project manifest.";
        return NO;
    }
    return YES;
}

+ (NSDictionary<NSString *, NSArray<NSNumber *> *> *)projectBreakpoints:(NSString *)projectPath
{
    NSDictionary *info = [self manifestForProject:projectPath];
    NSDictionary *all = info[@"breakpoints"];
    return [all isKindOfClass:NSDictionary.class] ? all : @{};
}

+ (NSArray<NSNumber *> *)breakpointsForSource:(NSString *)sourcePath project:(NSString *)projectPath
{
    if (!sourcePath.length || !projectPath.length) return @[];
    NSDictionary *info = [self manifestForProject:projectPath];
    NSDictionary *all = info[@"breakpoints"];
    NSString *relative = sourcePath;
    NSString *prefix = [projectPath stringByAppendingString:@"/"];
    if ([sourcePath hasPrefix:prefix]) relative = [sourcePath substringFromIndex:prefix.length];
    NSArray *lines = [all isKindOfClass:NSDictionary.class] ? all[relative] : nil;
    return [lines isKindOfClass:NSArray.class] ? lines : @[];
}

+ (BOOL)setBreakpoints:(NSArray<NSNumber *> *)lines forSource:(NSString *)sourcePath project:(NSString *)projectPath error:(NSString **)error
{
    if (!sourcePath.length || !projectPath.length) return NO;
    NSDictionary *info = [self manifestForProject:projectPath];
    if (!info[@"manifestPath"]) {
        if (error) *error = @"Project has no .gmbide.json manifest.";
        return NO;
    }
    NSMutableDictionary *manifest = [info mutableCopy];
    [manifest removeObjectForKey:@"manifestPath"];
    NSString *relative = sourcePath;
    NSString *prefix = [projectPath stringByAppendingString:@"/"];
    if ([sourcePath hasPrefix:prefix]) relative = [sourcePath substringFromIndex:prefix.length];
    NSMutableDictionary *all = [manifest[@"breakpoints"] isKindOfClass:NSDictionary.class]
        ? [manifest[@"breakpoints"] mutableCopy] : [NSMutableDictionary dictionary];
    all[relative] = lines ?: @[];
    manifest[@"breakpoints"] = all;
    NSData *data = [NSJSONSerialization dataWithJSONObject:manifest
                                                   options:NSJSONWritingPrettyPrinted | NSJSONWritingSortedKeys
                                                     error:nil];
    if (!data || ![data writeToFile:info[@"manifestPath"] atomically:YES]) {
        if (error) *error = @"Cannot update project manifest.";
        return NO;
    }
    return YES;
}

+ (BOOL)updateProjectManifest:(NSDictionary<NSString *, id> *)updates
                      project:(NSString *)projectPath
                        error:(NSString **)error
{
    NSDictionary *info = [self manifestForProject:projectPath];
    NSString *manifestPath = [info[@"manifestPath"] isKindOfClass:NSString.class] ? info[@"manifestPath"] : nil;
    if (!manifestPath.length) {
        if (error) *error = @"Project has no .gmbide.json manifest.";
        return NO;
    }
    NSMutableDictionary *manifest = [info mutableCopy];
    [manifest removeObjectForKey:@"manifestPath"];
    [updates enumerateKeysAndObjectsUsingBlock:^(NSString *key, id value, BOOL *stop) {
        (void)stop;
        manifest[key] = value;
    }];
    NSData *data = [NSJSONSerialization dataWithJSONObject:manifest options:NSJSONWritingPrettyPrinted error:nil];
    if (!data || ![data writeToFile:manifestPath atomically:YES]) {
        if (error) *error = @"Cannot update project manifest.";
        return NO;
    }
    return YES;
}

+ (BOOL)addSource:(NSString *)filePath form:(NSString *)formName toProject:(NSString *)projectPath error:(NSString **)error
{
    if (!filePath.length || !projectPath.length) return NO;
    NSDictionary *info = [self manifestForProject:projectPath];
    if (!info[@"manifestPath"]) {
        if (error) *error = @"Project has no .gmbide.json manifest.";
        return NO;
    }
    NSMutableDictionary *manifest = [info mutableCopy];
    [manifest removeObjectForKey:@"manifestPath"];

    NSString *relative = filePath;
    NSString *prefix = [projectPath stringByAppendingString:@"/"];
    if ([filePath hasPrefix:prefix]) relative = [filePath substringFromIndex:prefix.length];

    NSMutableArray *sources = [manifest[@"sources"] isKindOfClass:NSArray.class]
        ? [manifest[@"sources"] mutableCopy] : [NSMutableArray array];
    if (![sources containsObject:relative]) [sources addObject:relative];
    manifest[@"sources"] = sources;

    NSData *data = [NSJSONSerialization dataWithJSONObject:manifest
                                                   options:NSJSONWritingPrettyPrinted | NSJSONWritingSortedKeys
                                                     error:nil];
    if (!data || ![data writeToFile:info[@"manifestPath"] atomically:YES]) {
        if (error) *error = @"Cannot update project manifest.";
        return NO;
    }
    return YES;
}

+ (NSDictionary *)manifestForProject:(NSString *)projectPath
{
    NSFileManager *fm = [NSFileManager defaultManager];
    for (NSString *name in [fm contentsOfDirectoryAtPath:projectPath error:nil]) {
        if (![name.pathExtension.lowercaseString isEqualToString:@"json"] ||
            ![name.lowercaseString hasSuffix:@".gmbide.json"]) continue;
        NSString *candidate = [projectPath stringByAppendingPathComponent:name];
        NSData *data = [NSData dataWithContentsOfFile:candidate];
        if (!data) continue;
        id json = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
        if ([json isKindOfClass:NSDictionary.class]) {
            NSMutableDictionary *info = [json mutableCopy];
            info[@"manifestPath"] = candidate;
            return info;
        }
    }
    return nil;
}

+ (NSString *)entryForProject:(NSString *)projectPath
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSDictionary *manifest = [self manifestForProject:projectPath];
    NSString *entry = [manifest[@"entry"] isKindOfClass:NSString.class] ? manifest[@"entry"] : nil;
    if (entry.length) {
        NSString *full = entry.isAbsolutePath ? entry : [projectPath stringByAppendingPathComponent:entry];
        if ([fm fileExistsAtPath:full]) return full;
    }

    NSString *main = [projectPath stringByAppendingPathComponent:@"Main.bas"];
    if ([fm fileExistsAtPath:main]) return main;

    NSArray<NSString *> *entries = [fm contentsOfDirectoryAtPath:projectPath error:nil];
    for (NSString *name in [entries sortedArrayUsingSelector:@selector(localizedCaseInsensitiveCompare:)]) {
        if (![name.pathExtension.lowercaseString isEqualToString:@"bas"]) continue;
        NSString *full = [projectPath stringByAppendingPathComponent:name];
        if ([fm fileExistsAtPath:full]) return full;
    }
    return nil;
}

+ (int)runSynchronousCommand:(NSString *)command
                     project:(NSString *)projectPath
                       entry:(NSString *)entryPath
                      output:(NSString **)output
{
    NSString *tool = [self toolPath];
    if (!tool.length) {
        if (output) *output = @"GMBasic tool not found. Use GMBasic > Set Toolchain...\n";
        return 1;
    }
    if (!entryPath.length) {
        if (output) *output = @"No entry .bas found in the selected project.\n";
        return 1;
    }

    NSString *cwd = [[NSFileManager defaultManager] currentDirectoryPath] ?: @"/";
    NSString *project = [projectPath isAbsolutePath] ? projectPath : [cwd stringByAppendingPathComponent:projectPath];
    project = [project stringByStandardizingPath];
    NSString *entry = [entryPath isAbsolutePath] ? entryPath : [cwd stringByAppendingPathComponent:entryPath];
    entry = [entry stringByStandardizingPath];

    NSTask *task = [[NSTask alloc] init];
    task.executableURL = [NSURL fileURLWithPath:tool];
    NSMutableArray<NSString *> *args = [NSMutableArray arrayWithObjects:command, @"--project", project,
                                       @"--entry", entry, nil];
    if (![command isEqualToString:@"run"]) [args addObject:@"--json"];
    task.arguments = args;
    task.currentDirectoryURL = [NSURL fileURLWithPath:project isDirectory:YES];

    NSPipe *outPipe = [NSPipe pipe];
    NSPipe *errPipe = [NSPipe pipe];
    task.standardOutput = outPipe;
    task.standardError = errPipe;

    @try {
        [task launch];
    } @catch (NSException *exception) {
        if (output) *output = [NSString stringWithFormat:@"Cannot launch gmb: %@\n", exception.reason];
        return 1;
    }

    NSData *outData = [outPipe.fileHandleForReading readDataToEndOfFile];
    NSData *errData = [errPipe.fileHandleForReading readDataToEndOfFile];
    [task waitUntilExit];

    NSString *stdoutText = [[NSString alloc] initWithData:outData encoding:NSUTF8StringEncoding] ?: @"";
    NSString *stderrText = [[NSString alloc] initWithData:errData encoding:NSUTF8StringEncoding] ?: @"";
    NSString *text = stdoutText;
    if (stderrText.length) {
        if (text.length && ![text hasSuffix:@"\n"]) text = [text stringByAppendingString:@"\n"];
        text = [text stringByAppendingString:stderrText];
    }
    if (output) *output = text.length ? text : @"(no output)\n";
    return task.terminationStatus;
}

+ (void)runCommand:(NSString *)command
           project:(NSString *)projectPath
             entry:(NSString *)entryPath
        completion:(void (^)(int, NSString *))completion
{
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0), ^{
        NSString *text = nil;
        int status = [self runSynchronousCommand:command project:projectPath entry:entryPath output:&text];
        dispatch_async(dispatch_get_main_queue(), ^{
            if (completion) completion(status, text ?: @"");
        });
    });
}

+ (void)showOutput:(NSString *)text title:(NSString *)title
{
    dispatch_async(dispatch_get_main_queue(), ^{
        if (!gOutputWindows) gOutputWindows = [NSMutableArray array];
        NSWindow *window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0, 0, 760, 380)
                                                       styleMask:(NSWindowStyleMaskTitled |
                                                                  NSWindowStyleMaskClosable |
                                                                  NSWindowStyleMaskResizable)
                                                         backing:NSBackingStoreBuffered
                                                           defer:NO];
        window.title = title ?: @"GMBasic output";
        NSScrollView *scroll = [[NSScrollView alloc] initWithFrame:window.contentView.bounds];
        scroll.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        scroll.hasVerticalScroller = YES;
        NSTextView *view = [[NSTextView alloc] initWithFrame:scroll.bounds];
        view.editable = NO;
        view.font = [NSFont monospacedSystemFontOfSize:12 weight:NSFontWeightRegular];
        view.string = text ?: @"";
        scroll.documentView = view;
        window.contentView = scroll;
        [window center];
        [window makeKeyAndOrderFront:nil];
        [gOutputWindows addObject:window];
    });
}

@end
