// StdLib.Browser.cs — CUA Browser namespace (BROWSER.NAVIGATE/SNAPSHOT/CLICK/FILL/EVALUATE/SCREENSHOT)

/* ==== GMB-STDLIB-AUDIT-BEGIN ====
 * FILE: Browser — namespace 'BROWSER'
 * 21 funzioni | 0 comandi | 0 tipi | 0 costanti
 *
 * FUNZIONI (21):
 *   AccessTree
 *   Auth
 *   ClearCookies
 *   Click
 *   Console
 *   Evaluate
 *   Fill
 *   Find
 *   GetCookies
 *   HumanClick
 *   HumanFill
 *   HumanScroll
 *   Navigate
 *   NetLog
 *   Observe
 *   Screenshot
 *   SetCookie
 *   SetHeader
 *   SetUserAgent
 *   Snapshot
 *   Start
    ==== GMB-STDLIB-AUDIT-END ==== */

using System.Collections.Immutable;

namespace Basic;

public sealed partial class StdLib
{
    private void CreateBrowserNamespace()
    {
        const string ns = "BROWSER";
        var pub = SymbolFlags.IsPublic;
        var procedures = new ProcedureDefinition[]
        {
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BrowserNavigate, "Navigate"), [P("url", false)], new StringType(), TokenKind.BrowserNavigate, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BrowserSnapshot, "Snapshot"), [], new StringType(), TokenKind.BrowserSnapshot, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BrowserClick, "Click"), [P("selector", false)], new StringType(), TokenKind.BrowserClick, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BrowserFill, "Fill"), [P("selector", false), P("value", false)], new StringType(), TokenKind.BrowserFill, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BrowserEvaluate, "Evaluate"), [P("expression", false)], new StringType(), TokenKind.BrowserEvaluate, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BrowserStart, "Start"), [P("engine", true), P("port", true), P("profile", true)], new StringType(), TokenKind.BrowserStart, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BrowserAccessTree, "AccessTree"), [], new StringType(), TokenKind.BrowserAccessTree, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BrowserScreenshot, "Screenshot"), [P("path", true)], new StringType(), TokenKind.BrowserScreenshot, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BrowserGetCookies, "GetCookies"), [], new StringType(), TokenKind.BrowserGetCookies, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BrowserSetCookie, "SetCookie"), [P("name", false), P("value", false), P("url", true)], new StringType(), TokenKind.BrowserSetCookie, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BrowserClearCookies, "ClearCookies"), [], new StringType(), TokenKind.BrowserClearCookies, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BrowserSetHeader, "SetHeader"), [P("name", false), P("value", false)], new StringType(), TokenKind.BrowserSetHeader, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BrowserSetUserAgent, "SetUserAgent"), [P("userAgent", false)], new StringType(), TokenKind.BrowserSetUserAgent, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BrowserHumanClick, "HumanClick"), [P("selector", false)], new StringType(), TokenKind.BrowserHumanClick, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BrowserHumanFill, "HumanFill"), [P("selector", false), P("value", false)], new StringType(), TokenKind.BrowserHumanFill, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BrowserHumanScroll, "HumanScroll"), [P("dy", false)], new StringType(), TokenKind.BrowserHumanScroll, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BrowserObserve, "Observe"), [P("preset", true)], new StringType(), TokenKind.BrowserObserve, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BrowserFind, "Find"), [P("text", false), P("regex", true)], new StringType(), TokenKind.BrowserFind, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BrowserNetLog, "NetLog"), [P("clear", true)], new StringType(), TokenKind.BrowserNetLog, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BrowserConsole, "Console"), [P("clear", true)], new StringType(), TokenKind.BrowserConsole, ExecutionKind: NativeExecutionKind.Pure),
            new NativeProcedureDefinition(-1, pub, T(TokenKind.BrowserAuth, "Auth"), [P("op", false), P("name", false)], new StringType(), TokenKind.BrowserAuth, ExecutionKind: NativeExecutionKind.Pure),
        };
        RegisterBuiltinNamespace(ns, procedures, ImmutableDictionary<string, DataType>.Empty);
    }
}
