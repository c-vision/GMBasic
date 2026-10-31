// FILE: Basic.IlBackend.PCode.PCodeProcessor.Builtins.Grep.cs
// P-CODE VM: superficie GREP.MATCHES/COUNT/FILES — delega al motore C condiviso (NativeGrep).

using Basic.Runtime;
using Basic.Runtime.Gpm;
using Compiler;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    /// <summary>GREP: funzioni delegate al motore C condiviso gmb_grep (via P/Invoke).</summary>
    private static readonly HashSet<BuiltInFunction> GREPBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.GREPMATCHES,
        BuiltInFunction.GREPCOUNT,
        BuiltInFunction.GREPFILES,
    };

    private object? ExecuteBuiltInGrep(PCodeTask task, BuiltInFunction fn, object?[] args)
    {
        var pattern = ArgOr(args, 0)?.ToString() ?? "";
        var path = ArgOr(args, 1)?.ToString() ?? "";
        return fn switch
        {
            BuiltInFunction.GREPMATCHES => NativeGrep.MatchesArray(pattern, path),
            BuiltInFunction.GREPFILES => NativeGrep.FilesArray(pattern, path),
            BuiltInFunction.GREPCOUNT => (double)NativeGrep.Count(pattern, path),
            _ => throw new InvalidOperationException($"GREP: fn {fn} non gestito (ExecuteBuiltInGrep)."),
        };
    }
}
