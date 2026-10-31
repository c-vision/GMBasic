// PCodeProcessor.Builtins.ToolRegistry.cs — ToolRegistry per CUA (TOOL.SEARCH)
using Basic.Runtime.Gpm;
using Compiler;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    private static readonly HashSet<BuiltInFunction> TOOLBuiltInFunctions = new()
    {
        BuiltInFunction.TOOLSEARCH,
        BuiltInFunction.UIFIND,
        BuiltInFunction.UICLICK,
        BuiltInFunction.UIFINDALL,
        BuiltInFunction.UITYPE,
        BuiltInFunction.TOOLREGISTER,
        BuiltInFunction.BROWSERNAVIGATE,
        BuiltInFunction.BROWSERSNAPSHOT,
        BuiltInFunction.BROWSERCLICK,
        BuiltInFunction.BROWSERFILL,
        BuiltInFunction.BROWSEREVALUATE,
        BuiltInFunction.BROWSERSTART,
        BuiltInFunction.BROWSERACCESSTREE,
        BuiltInFunction.BROWSERSCREENSHOT,
        BuiltInFunction.BROWSERGETCOOKIES,
        BuiltInFunction.BROWSERSETCOOKIE,
        BuiltInFunction.BROWSERCLEARCOOKIES,
        BuiltInFunction.BROWSERSETHEADER,
        BuiltInFunction.BROWSERSETUSERAGENT,
        BuiltInFunction.BROWSERHUMANCLICK,
        BuiltInFunction.BROWSERHUMANFILL,
        BuiltInFunction.BROWSERHUMANSCROLL,
        BuiltInFunction.BROWSEROBSERVE,
        BuiltInFunction.BROWSERFIND,
        BuiltInFunction.BROWSERNETLOG,
        BuiltInFunction.BROWSERCONSOLE,
        BuiltInFunction.BROWSERAUTH,
        BuiltInFunction.AGENTRUNCS,
        BuiltInFunction.AGENTRUN,
        BuiltInFunction.AGENTRUNGRAPH,
        BuiltInFunction.AGENTRUNRAG,
        BuiltInFunction.AGENTRUNCODE,
        BuiltInFunction.COMPUTERSCREENSHOT,
        BuiltInFunction.COMPUTERSCREENRECORD,
        BuiltInFunction.COMPUTERSCREENRECORDSTART,
        BuiltInFunction.COMPUTERSCREENRECORDSTOP,
        BuiltInFunction.COMPUTERSCREENRECORDSTATUS,
        BuiltInFunction.COMPUTERCLICK,
        BuiltInFunction.COMPUTERMOVE,
        BuiltInFunction.COMPUTERTYPE,
        BuiltInFunction.COMPUTERKEY,
        BuiltInFunction.COMPUTERWINDOWS,
        BuiltInFunction.COMPUTERDOUBLECLICK,
        BuiltInFunction.COMPUTERHOTKEY,
    };

    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _toolDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> TOOLBuiltInDispatch
        => _toolDispatch ??= new Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>
        {
            [BuiltInFunction.TOOLSEARCH] = (task, args) => Basic.IlBackend.IlRuntime.ToolSearch(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
            [BuiltInFunction.TOOLREGISTER] = (task, args) => Basic.IlBackend.IlRuntime.ToolRegister(args.Length > 0 ? args[0]?.ToString() ?? "" : "", args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
            [BuiltInFunction.UIFIND] = (task, args) => Basic.IlBackend.IlRuntime.UiFind(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
            [BuiltInFunction.UICLICK] = (task, args) => Basic.IlBackend.IlRuntime.UiClick(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
            [BuiltInFunction.UIFINDALL] = (task, args) => Basic.IlBackend.IlRuntime.UiFindAll(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
            [BuiltInFunction.UITYPE] = (task, args) => Basic.IlBackend.IlRuntime.UiType(args.Length > 0 ? args[0]?.ToString() ?? "" : "", args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
            [BuiltInFunction.BROWSERNAVIGATE] = (task, args) => Basic.IlBackend.IlRuntime.BrowserNavigate(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
            [BuiltInFunction.BROWSERSNAPSHOT] = (task, args) => Basic.IlBackend.IlRuntime.BrowserSnapshot(),
            [BuiltInFunction.BROWSERACCESSTREE] = (task, args) => Basic.IlBackend.IlRuntime.BrowserAccessTree(),
            [BuiltInFunction.BROWSERSCREENSHOT] = (task, args) => Basic.IlBackend.IlRuntime.BrowserScreenshot(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
            [BuiltInFunction.BROWSERGETCOOKIES] = (task, args) => Basic.IlBackend.IlRuntime.BrowserGetCookies(),
            [BuiltInFunction.BROWSERSETCOOKIE] = (task, args) => Basic.IlBackend.IlRuntime.BrowserSetCookie(args.Length > 0 ? args[0]?.ToString() ?? "" : "", args.Length > 1 ? args[1]?.ToString() ?? "" : "", args.Length > 2 ? args[2]?.ToString() ?? "" : ""),
            [BuiltInFunction.BROWSERCLEARCOOKIES] = (task, args) => Basic.IlBackend.IlRuntime.BrowserClearCookies(),
            [BuiltInFunction.BROWSERSETHEADER] = (task, args) => Basic.IlBackend.IlRuntime.BrowserSetHeader(args.Length > 0 ? args[0]?.ToString() ?? "" : "", args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
            [BuiltInFunction.BROWSERSETUSERAGENT] = (task, args) => Basic.IlBackend.IlRuntime.BrowserSetUserAgent(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
            [BuiltInFunction.BROWSERHUMANCLICK] = (task, args) => Basic.IlBackend.IlRuntime.BrowserHumanClick(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
            [BuiltInFunction.BROWSERHUMANFILL] = (task, args) => Basic.IlBackend.IlRuntime.BrowserHumanFill(args.Length > 0 ? args[0]?.ToString() ?? "" : "", args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
            [BuiltInFunction.BROWSERHUMANSCROLL] = (task, args) => Basic.IlBackend.IlRuntime.BrowserHumanScroll(args.Length > 0 && int.TryParse(args[0]?.ToString(), out var d) ? d : 0),
            [BuiltInFunction.BROWSEROBSERVE] = (task, args) => Basic.IlBackend.IlRuntime.BrowserObserve(args.Length > 0 ? args[0]?.ToString() : null),
            [BuiltInFunction.BROWSERFIND] = (task, args) => Basic.IlBackend.IlRuntime.BrowserFind(args.Length > 0 ? args[0]?.ToString() ?? "" : "", args.Length > 1 && args[1] is double d && d != 0),
            [BuiltInFunction.BROWSERNETLOG] = (task, args) => Basic.IlBackend.IlRuntime.BrowserNetLog(args.Length > 0 && args[0] is double cd && cd != 0),
            [BuiltInFunction.BROWSERCONSOLE] = (task, args) => Basic.IlBackend.IlRuntime.BrowserConsole(args.Length > 0 && args[0] is double cd && cd != 0),
            [BuiltInFunction.BROWSERAUTH] = (task, args) => Basic.IlBackend.IlRuntime.BrowserAuth(args.Length > 0 ? args[0]?.ToString() ?? "" : "", args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
            [BuiltInFunction.BROWSERCLICK] = (task, args) => Basic.IlBackend.IlRuntime.BrowserClick(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
            [BuiltInFunction.BROWSERFILL] = (task, args) => Basic.IlBackend.IlRuntime.BrowserFill(args.Length > 0 ? args[0]?.ToString() ?? "" : "", args.Length > 1 ? args[1]?.ToString() ?? "" : ""),
            [BuiltInFunction.BROWSEREVALUATE] = (task, args) => Basic.IlBackend.IlRuntime.BrowserEvaluate(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
            [BuiltInFunction.BROWSERSTART] = (task, args) => Basic.IlBackend.IlRuntime.BrowserStartEngine(args.Length > 0 ? Num(args, 0) : 0, args.Length > 1 && int.TryParse(args[1]?.ToString(), out var pp) ? pp : 0, args.Length > 2 ? args[2]?.ToString() ?? "" : ""),
            [BuiltInFunction.AGENTRUNCS] = (task, args) => Basic.IlBackend.IlRuntime.AgentRunCua(args.Length > 0 ? args[0]?.ToString() ?? "" : "", args.Length > 1 ? args[1]?.ToString() ?? "" : "", args.Length > 2 && int.TryParse(args[2]?.ToString(), out var pr) ? pr : 5),
            [BuiltInFunction.AGENTRUN] = (task, args) => Basic.IlBackend.IlRuntime.AgentRun(args.Length > 0 ? args[0]?.ToString() ?? "" : "", args.Length > 1 && int.TryParse(args[1]?.ToString(), out var ar2) ? ar2 : 8),
            [BuiltInFunction.AGENTRUNGRAPH] = (task, args) => Basic.IlBackend.IlRuntime.AgentRunGraph(args.Length > 0 ? args[0]?.ToString() ?? "" : "", args.Length > 1 ? args[1]?.ToString() ?? "" : "", args.Length > 2 && int.TryParse(args[2]?.ToString(), out var pr) ? pr : 5),
            [BuiltInFunction.AGENTRUNRAG] = (task, args) => Basic.IlBackend.IlRuntime.AgentRunRag(args.Length > 0 ? args[0]?.ToString() ?? "" : "", args.Length > 1 ? args[1]?.ToString() ?? "" : "", args.Length > 2 && int.TryParse(args[2]?.ToString(), out var pr) ? pr : 5),
            [BuiltInFunction.AGENTRUNCODE] = (task, args) => Basic.IlBackend.IlRuntime.AgentRunCode(args.Length > 0 ? args[0]?.ToString() ?? "" : "", args.Length > 1 ? args[1]?.ToString() ?? "" : "", args.Length > 2 && int.TryParse(args[2]?.ToString(), out var prc) ? prc : 5),
            [BuiltInFunction.COMPUTERSCREENSHOT] = (task, args) => Basic.IlBackend.IlRuntime.ComputerScreenshot(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
            [BuiltInFunction.COMPUTERSCREENRECORD] = (task, args) => Basic.IlBackend.IlRuntime.ComputerScreenRecord(args.Length > 0 ? args[0]?.ToString() ?? "" : "", args.Length > 1 ? Num(args, 1) : 10),
            [BuiltInFunction.COMPUTERSCREENRECORDSTART] = (task, args) => Basic.IlBackend.IlRuntime.ComputerScreenRecordStart(args.Length > 0 ? args[0]?.ToString() ?? "" : "", args.Length > 1 ? Num(args, 1) : 10),
            [BuiltInFunction.COMPUTERSCREENRECORDSTOP] = (task, args) => Basic.IlBackend.IlRuntime.ComputerScreenRecordStop(),
            [BuiltInFunction.COMPUTERSCREENRECORDSTATUS] = (task, args) => Basic.IlBackend.IlRuntime.ComputerScreenRecordStatus(),
            [BuiltInFunction.COMPUTERCLICK] = (task, args) => Basic.IlBackend.IlRuntime.ComputerClick(Num(args, 0), Num(args, 1)),
            [BuiltInFunction.COMPUTERMOVE] = (task, args) => Basic.IlBackend.IlRuntime.ComputerMove(Num(args, 0), Num(args, 1)),
            [BuiltInFunction.COMPUTERTYPE] = (task, args) => Basic.IlBackend.IlRuntime.ComputerType(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
            [BuiltInFunction.COMPUTERKEY] = (task, args) => Basic.IlBackend.IlRuntime.ComputerKey(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
            [BuiltInFunction.COMPUTERWINDOWS] = (task, args) => Basic.IlBackend.IlRuntime.ComputerWindows(),
            [BuiltInFunction.COMPUTERDOUBLECLICK] = (task, args) => Basic.IlBackend.IlRuntime.ComputerDoubleClick(Num(args, 0), Num(args, 1)),
            [BuiltInFunction.COMPUTERHOTKEY] = (task, args) => Basic.IlBackend.IlRuntime.ComputerHotKey(args.Length > 0 ? args[0]?.ToString() ?? "" : ""),
        };

    private static double Num(object?[] a, int i) =>
        i < a.Length && a[i] is IConvertible c
            ? System.Convert.ToDouble(c, System.Globalization.CultureInfo.InvariantCulture)
            : 0.0;

    private object? ExecuteBuiltInTool(PCodeTask task, BuiltInFunction fn, object?[] args)
        => TOOLBuiltInDispatch.TryGetValue(fn, out var h) ? h(task, args) : throw new InvalidOperationException($"TOOL {fn} not handled");
}
