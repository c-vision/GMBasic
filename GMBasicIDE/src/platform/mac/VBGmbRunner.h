#ifndef VB_GMB_RUNNER_H
#define VB_GMB_RUNNER_H

#import <Cocoa/Cocoa.h>

/* External bridge to the GMBasic compiler CLI. The IDE never links the compiler:
   it launches `gmb check|build|run --project <dir> --entry <file> --json`. */
@interface VBGmbRunner : NSObject

+ (NSString *)detectedToolPath;
+ (NSString *)toolPath;
+ (void)setToolPath:(NSString *)path;

+ (NSString *)chunkToolPath;
+ (NSString *)vmHostPath;
+ (BOOL)compileChunkForSource:(NSString *)sourcePath toPath:(NSString *)chunkPath output:(NSString **)output;
+ (BOOL)compileChunkForSource:(NSString *)sourcePath toPath:(NSString *)chunkPath mapPath:(NSString *)mapPath output:(NSString **)output;
+ (NSArray<NSDictionary *> *)sourceMapAtPath:(NSString *)path;
+ (NSString *)themeToolPath;
+ (NSArray<NSString *> *)themeNames;
+ (NSString *)themeName;
+ (void)setThemeName:(NSString *)name;
+ (void)loadThemeNamed:(NSString *)name completion:(void (^)(NSDictionary *json))completion;

+ (NSString *)projectPath;
+ (void)setProjectPath:(NSString *)path;
+ (BOOL)createProjectAtPath:(NSString *)projectPath name:(NSString *)name error:(NSString **)error;
+ (NSDictionary *)manifestForProject:(NSString *)projectPath;
+ (BOOL)updateProjectManifest:(NSDictionary<NSString *, id> *)updates
                      project:(NSString *)projectPath
                        error:(NSString **)error;
+ (BOOL)addSource:(NSString *)filePath form:(NSString *)formName toProject:(NSString *)projectPath error:(NSString **)error;
+ (BOOL)addForm:(NSString *)filePath name:(NSString *)formName toProject:(NSString *)projectPath error:(NSString **)error;
+ (BOOL)removeFormNamed:(NSString *)formName fromProject:(NSString *)projectPath error:(NSString **)error;
+ (NSDictionary<NSString *, NSArray<NSNumber *> *> *)projectBreakpoints:(NSString *)projectPath;
+ (NSArray<NSNumber *> *)breakpointsForSource:(NSString *)sourcePath project:(NSString *)projectPath;
+ (BOOL)setBreakpoints:(NSArray<NSNumber *> *)lines forSource:(NSString *)sourcePath project:(NSString *)projectPath error:(NSString **)error;
+ (NSString *)entryForProject:(NSString *)projectPath;
+ (void)runCommand:(NSString *)command
           project:(NSString *)projectPath
             entry:(NSString *)entryPath
        completion:(void (^)(int status, NSString *output))completion;

/* Synchronous variant used by the headless integration self-test. */
+ (int)runSynchronousCommand:(NSString *)command
                     project:(NSString *)projectPath
                       entry:(NSString *)entryPath
                      output:(NSString **)output;

+ (void)showOutput:(NSString *)text title:(NSString *)title;

@end

#endif
