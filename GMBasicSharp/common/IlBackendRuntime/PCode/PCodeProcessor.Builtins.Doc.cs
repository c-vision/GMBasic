// FILE: Basic.IlBackend.PCode.PCodeProcessor.Builtins.cs
// P-CODE VM: ExecuteBuiltIn — refactored from PCodeProcessor.cs (2026-08-03)

using Basic.Runtime;
using Basic.Runtime.Gpm;
using Compiler;
using Microsoft.Data.Sqlite;
using Basic.AI.Rag;
using Basic.Databases.Rdd;
using Basic.IlBackend.PCode.Http;
using CommonLib.Http;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    /// <summary>DOC: delegazione dominio — case contigui estratti
    /// da PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> DOCBuiltInFunctions = new HashSet<BuiltInFunction>
    {
        BuiltInFunction.DOCEXTRACTENGINE,
        BuiltInFunction.DOCEXTRACT,
        BuiltInFunction.DOCOPEN,
        BuiltInFunction.DOCCLOSE,
        BuiltInFunction.DOCPAGECOUNT,
        BuiltInFunction.DOCTEXT,
        BuiltInFunction.DOCMARKDOWN,
        BuiltInFunction.DOCHTML,
        BuiltInFunction.DOCMETADATA,
        BuiltInFunction.DOCSEARCH,
        BuiltInFunction.DOCIMAGES,
        BuiltInFunction.DOCRENDER,
    };

    /// <summary>Doc: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).</summary>
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>>? _docDispatch;
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> DOCDispatch
        => _docDispatch ??= BuildDocDispatch();
    private Dictionary<BuiltInFunction, Func<PCodeTask, object?[], object?>> BuildDocDispatch() => new()
    {
        [BuiltInFunction.DOCEXTRACTENGINE] = (task, args) =>
        {
                { if (args.Length >= 1 && args[0] is string xe) return IlRuntime.DocExtractEngine(xe);
                return ""; }
        },
        [BuiltInFunction.DOCEXTRACT] = (task, args) =>
        {
                { if (args.Length >= 1 && args[0] is string xp) return NativeDocPdf.Extract(xp);
                return ""; }
        },
        [BuiltInFunction.DOCOPEN] = (task, args) =>
        {
                { if (args.Length >= 1 && args[0] is string dp) return (double)NativeDocPdf.Open(dp);
                return 0.0; }
        },
        [BuiltInFunction.DOCCLOSE] = (task, args) =>
        {
                { if (args.Length >= 1 && args[0] is double hc) NativeDocPdf.Close((int)hc);
                return 0.0; }
        },
        [BuiltInFunction.DOCPAGECOUNT] = (task, args) =>
        {
                { if (args.Length >= 1 && args[0] is double hp) return (double)NativeDocPdf.PageCount((int)hp);
                return 0.0; }
        },
        [BuiltInFunction.DOCTEXT] = (task, args) =>
        {
                { if (args.Length >= 1 && args[0] is double ht)
                return NativeDocPdf.Text((int)ht, args.Length >= 2 && args[1] is double pt ? (int)pt : 0);
                return ""; }
        },
        [BuiltInFunction.DOCMARKDOWN] = (task, args) =>
        {
                { if (args.Length >= 1 && args[0] is double hm)
                return NativeDocPdf.Markdown((int)hm, args.Length >= 2 && args[1] is double pm ? (int)pm : -1);
                return ""; }
        },
        [BuiltInFunction.DOCHTML] = (task, args) =>
        {
                { if (args.Length >= 1 && args[0] is double hh)
                return NativeDocPdf.Html((int)hh, args.Length >= 2 && args[1] is double ph ? (int)ph : 0);
                return ""; }
        },
        [BuiltInFunction.DOCMETADATA] = (task, args) =>
        {
                { if (args.Length >= 1 && args[0] is double hmd) return NativeDocPdf.Metadata((int)hmd);
                return ""; }
        },
        [BuiltInFunction.DOCSEARCH] = (task, args) =>
        {
                { if (args.Length >= 2 && args[0] is double hs && args[1] is string pat)
                return NativeDocPdf.Search((int)hs, pat);
                return ""; }
        },
        [BuiltInFunction.DOCIMAGES] = (task, args) =>
        {
                { if (args.Length >= 2 && args[0] is double hi && args[2 - 1] is string dir)
                return (double)NativeDocPdf.Images((int)hi, args.Length >= 3 && args[1] is double pi ? (int)pi : 0, dir);
                return 0.0; }
        },
        [BuiltInFunction.DOCRENDER] = (task, args) =>
        {
                { if (args.Length >= 3 && args[0] is double hr && args[args.Length - 1] is string outp)
                return (double)NativeDocPdf.RenderPng((int)hr, args[1] is double pg ? (int)pg : 0, args.Length >= 4 && args[2] is double pd ? (int)pd : 150, outp);
                return -1.0; }
        },
    };

    private object? ExecuteBuiltInDoc(PCodeTask task, BuiltInFunction fn, object?[] args)
        => DOCDispatch.TryGetValue(fn, out var handler)
            ? handler(task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInDoc (fn={fn}).");

}
