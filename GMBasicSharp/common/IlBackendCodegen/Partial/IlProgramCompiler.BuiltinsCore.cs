// FILE: IlBackend/Partial/IlProgramCompiler.BuiltinsCore.cs
// PARTIAL — dominio BUILTIN CORE (ledger/udt/conv/ai/chart/function-ref/collection).
// Migrato da IlProgramCompiler.cs (2026-08-25 — protocollo 3-step).

using System.Collections.Immutable;
using System.Linq;
using System.Reflection;
using System.Reflection.Emit;
using System.Reflection.Metadata;
using System.Reflection.Metadata.Ecma335;
using System.Reflection.PortableExecutable;
using Basic.IlBackend.Gpm;
using Basic.Runtime.Gpm;
using Basic;

namespace Basic.IlBackend;

/// <summary>Partial del transpiler IL: dominio BUILTIN CORE.</summary>
public sealed partial class IlProgramCompiler
{

    private void EmitLedgerFunctionCall(BuiltinFunctionCallExpr expr)
    {
        switch (expr.Name.Kind)
        {
            case TokenKind.Eof or TokenKind.EofFunc:
                RequireLedgerArgCount(expr, 1);
                EmitLedgerAliasArg(expr.Args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerEof), typeof(string)));
                break;
            case TokenKind.Bof:
                RequireLedgerArgCount(expr, 1);
                EmitLedgerAliasArg(expr.Args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerBof), typeof(string)));
                break;
            case TokenKind.Found:
                RequireLedgerArgCount(expr, 1);
                EmitLedgerAliasArg(expr.Args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerFound), typeof(string)));
                break;
            case TokenKind.Recno:
                RequireLedgerArgCount(expr, 1);
                EmitLedgerAliasArg(expr.Args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerRecno), typeof(string)));
                break;
            case TokenKind.Count or TokenKind.Lastrec:
                RequireLedgerArgCount(expr, 1);
                EmitLedgerAliasArg(expr.Args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerCount), typeof(string)));
                break;
            case TokenKind.Get:
                if (expr.Args.Length == 1)
                {
                    // LEDGER.GET(h): transazione corrente del cursore.
                    EmitLedgerAliasArg(expr.Args[0]);
                    _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerGetCurrent), typeof(string)));
                }
                else if (expr.Args.Length == 2)
                {
                    // LEDGER.GET(h, id): transazione per ID.
                    EmitExpression(expr.Args[1]);
                    EmitCoerceTypes(NormalizeInferredType(InferType(expr.Args[1])), new DoubleType());
                    EmitLedgerAliasArg(expr.Args[0]);
                    _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerGet), typeof(double), typeof(string)));
                }
                else throw new NotSupportedException("Backend IL: LEDGER.GET richiede 1 o 2 argomenti (handle [, id]).");
                break;
            case TokenKind.Validate:
                RequireLedgerArgCount(expr, 1);
                EmitLedgerAliasArg(expr.Args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerValidate), typeof(string)));
                break;
            case TokenKind.Register or TokenKind.Balance:
            {
                RequireLedgerArgCountIn(expr, 1, 2);
                if (expr.Args.Length > 1) EmitStringArg(expr.Args[1]); else _il.Emit(OpCodes.Ldstr, "");
                EmitLedgerAliasArg(expr.Args[0]);
                var m = expr.Name.Kind == TokenKind.Register
                    ? RuntimeMethod(nameof(IlRuntime.LedgerRegister), typeof(string), typeof(string))
                    : RuntimeMethod(nameof(IlRuntime.LedgerBalance), typeof(string), typeof(string));
                _il.Emit(OpCodes.Call, m);
                break;
            }
            case TokenKind.Accounts:
            {
                RequireLedgerArgCountIn(expr, 1, 2);
                if (expr.Args.Length > 1) EmitStringArg(expr.Args[1]); else _il.Emit(OpCodes.Ldnull);
                EmitLedgerAliasArg(expr.Args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerAccounts), typeof(string), typeof(string)));
                break;
            }
            case TokenKind.Variance:
            {
                RequireLedgerArgCount(expr, 3);
                EmitStringArg(expr.Args[1]);
                EmitStringArg(expr.Args[2]);
                EmitLedgerAliasArg(expr.Args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerVariance), typeof(string), typeof(string), typeof(string)));
                break;
            }
            case TokenKind.Plot:
            {
                RequireLedgerArgCount(expr, 3);
                EmitStringArg(expr.Args[1]);
                EmitStringArg(expr.Args[2]);
                EmitLedgerAliasArg(expr.Args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerPlot), typeof(string), typeof(string), typeof(string)));
                break;
            }
            case TokenKind.Import:
            {
                RequireLedgerArgCount(expr, 3);
                EmitStringArg(expr.Args[1]);
                EmitStringArg(expr.Args[2]);
                EmitLedgerAliasArg(expr.Args[0]);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerImport), typeof(string), typeof(string), typeof(string)));
                break;
            }
            case TokenKind.Open:
                if (expr.Args.Length != 1) throw new NotSupportedException("Backend IL: LEDGER.OPEN richiede 1 argomento (path).");
                EmitStringArg(expr.Args[0]);
                _il.Emit(OpCodes.Ldnull);
                _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.LedgerOpen), typeof(string), typeof(string)));
                EmitCoerceTypes(new LongType(), NormalizeInferredType(InferType(expr)));
                break;
            case TokenKind.Add:
            case TokenKind.Edit:
                throw new NotSupportedException(
                    $"Backend IL: la forma funzione {expr.OriginalToken.Lexeme} non è supportata — usa la forma statement (LEDGER {expr.Name.Lexeme} ... INTO).");
            default:
                throw new NotSupportedException($"Backend IL: funzione LEDGER '{expr.OriginalToken.Lexeme}' non ancora supportata.");
        }
    }

    private bool TryEmitUserDefinedCTypeCall(BuiltinFunctionCallExpr expression)
    {
        if (expression.Args.IsEmpty) return false;
        var sourceType = NormalizeInferredType(InferType(expression.Args[0]));
        var ownerSymbolId = sourceType switch
        {
            ClassType classType => classType.Definition.SymbolId,
            UserDefinedType udt => udt.SymbolId,
            _ => -1,
        };
        var requestedReturn = expression.Args.Length > 1 && expression.Args[1] is StringExpr target
            ? target.Value.ToUpperInvariant().Replace(" ", string.Empty)
            : OperatorReturnKey(expression.ReturnType);
        if (ownerSymbolId < 0 || FindOperatorInHierarchy(ownerSymbolId, TokenKind.CType, 1,
                requestedReturn) is not { } method)
            return false;
        EmitExpression(expression.Args[0]);
        _il.Emit(OpCodes.Call, method);
        return true;
    }

    /// <summary>CONVERT.BATCH(dir [, fmt] [, rec]): dir obbligatorio, fmt/rec opzionali — ma
    /// IlRuntime.ConvertBatch(string,string?,bool) ha comunque 3 parametri IL reali (i default
    /// C# sono zucchero solo lato CHIAMANTE sorgente, non riducono l'arità del MethodInfo), va
    /// sempre chiamato con 3 valori sullo stack.</summary>
    /// <summary>CONVERT source[, format] INTO target: 1 arg = md, 2 arg = format selezionato.</summary>
    private void EmitConvDocCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException("Backend IL: CONVERT richiede 1 o 2 argomenti (source[, format]).");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length == 2)
        {
            EmitStringArg(expr.Args[1]);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ConvertDocDispatch), typeof(string), typeof(string)));
        }
        else
        {
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ConvertToMd), typeof(string)));
        }
    }

    private void EmitConvertBatchCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 3)
            throw new NotSupportedException("Backend IL: CONVERT.BATCH richiede da 1 a 3 argomenti.");
        EmitExpression(expr.Args[0]);
        if (expr.Args.Length >= 2) EmitExpression(expr.Args[1]); else _il.Emit(OpCodes.Ldnull);
        if (expr.Args.Length >= 3)
        {
            var recType = NormalizeInferredType(InferType(expr.Args[2]));
            EmitExpression(expr.Args[2]);
            EmitCoerceTypes(recType, new BitType());
        }
        else _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ConvertBatch)));
    }

    // ── DOCUMENT/PDF (Fase 4): handle-based. Argomenti numerici = double BASIC, cast nel runtime. ──
    private void EmitDocIntArg(BuiltinFunctionCallExpr expr, int index)
    {
        EmitExpression(expr.Args[index]);
    }

    // ── Fase 5 — Markdown AST ──
    private void EmitMdParseCall(BuiltinFunctionCallExpr expr) // MdParse(string text) → object
    {
        if (expr.Args.Length != 1) throw new NotSupportedException("Backend IL: CONVERT.PARSE richiede 1 argomento.");
        EmitExpression(expr.Args[0]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MdParse), typeof(string)));
    }

    private void EmitMdDocCall(BuiltinFunctionCallExpr expr, string method) // Md*(object doc)
    {
        if (expr.Args.Length != 1) throw new NotSupportedException($"Backend IL: {method} richiede 1 argomento.");
        EmitExpression(expr.Args[0]);
        _il.Emit(OpCodes.Call, RuntimeMethod(method, typeof(object)));
    }

    private void EmitMdHeadingsCall(BuiltinFunctionCallExpr expr) // MdHeadings(object doc, double level=0)
    {
        if (expr.Args.Length is < 1 or > 2) throw new NotSupportedException("Backend IL: CONVERT.HEADINGS richiede 1 o 2 argomenti.");
        EmitExpression(expr.Args[0]);
        if (expr.Args.Length >= 2) EmitExpression(expr.Args[1]); else _il.Emit(OpCodes.Ldc_R8, 0.0);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MdHeadings), typeof(object), typeof(double)));
    }

    private void EmitMdStripCall(BuiltinFunctionCallExpr expr) // MdStrip(string text)
    {
        if (expr.Args.Length != 1) throw new NotSupportedException("Backend IL: CONVERT.STRIP richiede 1 argomento.");
        EmitExpression(expr.Args[0]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MdStrip), typeof(string)));
    }

    private void EmitMdQueryCall(BuiltinFunctionCallExpr expr) // MdQuery(object doc, string selector)
    {
        if (expr.Args.Length != 2) throw new NotSupportedException("Backend IL: CONVERT.QUERY richiede 2 argomenti.");
        EmitExpression(expr.Args[0]);
        EmitExpression(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.MdQuery), typeof(object), typeof(string)));
    }

    private void EmitConvertExtractToCall(BuiltinFunctionCallExpr expr) // (string src, string dest)
    {
        if (expr.Args.Length != 2) throw new NotSupportedException("Backend IL: CONVERT EXTRACT richiede 2 argomenti (src, dest).");
        EmitExpression(expr.Args[0]); EmitExpression(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ConvertExtractTo), typeof(string), typeof(string)));
    }

    private void EmitConvertRenderToCall(BuiltinFunctionCallExpr expr) // (string src, string dest)
    {
        if (expr.Args.Length != 2) throw new NotSupportedException("Backend IL: CONVERT RENDER richiede 2 argomenti (src, dest).");
        EmitExpression(expr.Args[0]); EmitExpression(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ConvertRenderTo), typeof(string), typeof(string)));
    }

    private void EmitDocExtractEngineCall(BuiltinFunctionCallExpr expr) // DocExtractEngine(string path)
    {
        if (expr.Args.Length != 1) throw new NotSupportedException("Backend IL: DOCUMENT.EXTRACTENGINE richiede 1 argomento.");
        EmitExpression(expr.Args[0]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DocExtractEngine), typeof(string)));
    }

    private void EmitDocExtractCall(BuiltinFunctionCallExpr expr) // DocExtract(string path) → string
    {
        if (expr.Args.Length != 1) throw new NotSupportedException("Backend IL: DOCUMENT.EXTRACT richiede 1 argomento.");
        EmitExpression(expr.Args[0]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DocExtract), typeof(string)));
    }

    private void EmitDocOpenCall(BuiltinFunctionCallExpr expr)   // DocOpen(string path) → int
    {
        if (expr.Args.Length != 1) throw new NotSupportedException("Backend IL: DOCUMENT.OPEN richiede 1 argomento.");
        EmitExpression(expr.Args[0]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DocOpen), typeof(string)));
    }

    private void EmitDocCloseCall(BuiltinFunctionCallExpr expr)  // DocClose(int h)
    {
        if (expr.Args.Length != 1) throw new NotSupportedException("Backend IL: DOCUMENT.CLOSE richiede 1 argomento.");
        EmitDocIntArg(expr, 0);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DocClose), typeof(double)));
    }

    private void EmitDocPageCountCall(BuiltinFunctionCallExpr expr) // DocPageCount(int h) → int
    {
        if (expr.Args.Length != 1) throw new NotSupportedException("Backend IL: DOCUMENT.PAGECOUNT richiede 1 argomento.");
        EmitDocIntArg(expr, 0);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DocPageCount), typeof(double)));
    }

    private void EmitDocPageStringCall(BuiltinFunctionCallExpr expr, string method) // DocText/Markdown/Html(int h, int page=-1)
    {
        if (expr.Args.Length is < 1 or > 2) throw new NotSupportedException($"Backend IL: {method} richiede 1 o 2 argomenti.");
        EmitDocIntArg(expr, 0);
        if (expr.Args.Length >= 2) EmitDocIntArg(expr, 1); else _il.Emit(OpCodes.Ldc_R8, -1.0);
        _il.Emit(OpCodes.Call, RuntimeMethod(method, typeof(double), typeof(double)));
    }

    private void EmitDocMetadataCall(BuiltinFunctionCallExpr expr) // DocMetadata(int h) → string
    {
        if (expr.Args.Length != 1) throw new NotSupportedException("Backend IL: DOCUMENT.METADATA richiede 1 argomento.");
        EmitDocIntArg(expr, 0);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DocMetadata), typeof(double)));
    }

    private void EmitDocSearchCall(BuiltinFunctionCallExpr expr)  // DocSearch(int h, string pattern)
    {
        if (expr.Args.Length != 2) throw new NotSupportedException("Backend IL: DOCUMENT.SEARCH richiede 2 argomenti.");
        EmitDocIntArg(expr, 0);
        EmitExpression(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DocSearch), typeof(double), typeof(string)));
    }

    private void EmitDocImagesCall(BuiltinFunctionCallExpr expr)  // DocImages(int h, int page=0, string dir)
    {
        if (expr.Args.Length is < 2 or > 3) throw new NotSupportedException("Backend IL: DOCUMENT.IMAGES richiede 2 o 3 argomenti.");
        EmitDocIntArg(expr, 0);
        if (expr.Args.Length >= 3) EmitDocIntArg(expr, 1); else _il.Emit(OpCodes.Ldc_R8, 0.0);
        EmitExpression(expr.Args[expr.Args.Length - 1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DocImages), typeof(double), typeof(double), typeof(string)));
    }

    private void EmitDocRenderCall(BuiltinFunctionCallExpr expr)  // DocRender(int h, int page, int dpi=150, string out)
    {
        if (expr.Args.Length is < 3 or > 4) throw new NotSupportedException("Backend IL: DOCUMENT.RENDER richiede 3 o 4 argomenti.");
        EmitDocIntArg(expr, 0);
        EmitDocIntArg(expr, 1);
        if (expr.Args.Length >= 4) EmitDocIntArg(expr, 2); else _il.Emit(OpCodes.Ldc_R8, 150.0);
        EmitExpression(expr.Args[expr.Args.Length - 1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.DocRender), typeof(double), typeof(double), typeof(double), typeof(string)));
    }

    /// <summary>AI.OCR(path [, model]): il p-code VM (PCodeProcessor.Builtins.cs, case AIOCR)
    /// legge solo args[0] — il parametro "model" opzionale è dichiarato in StdLib.AI.cs ma non
    /// ancora usato da nessuna implementazione, VM o IL. Valutato comunque (side-effect di
    /// un'espressione argomento vanno sempre preservati) e scartato, mai passato oltre.</summary>
    /// <summary>AI.SETKEY(name,key)/AI.GETKEY(name): stesso TokenKind.AiKey condiviso per
    /// setter (2 arg) e getter (1 arg) — getter stub "OK" come il p-code VM.</summary>
    private void EmitAiSetKeyCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length == 2)
        {
            EmitExpression(expr.Args[0]);
            EmitExpression(expr.Args[1]);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiSetKey)));
            return;
        }
        EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiGetKey)));
    }

    /// <summary>Config AI: 2 argomenti → setter, 1 argomento → getter (es. AiTemp/AiBase/AiDef).</summary>
    private void EmitAiConfigBinaryOrUnaryCall(BuiltinFunctionCallExpr expr, string setMethod, string getMethod)
    {
        if (expr.Args.Length == 2)
        {
            EmitStringBinaryBuiltinCall(expr, RuntimeMethod(setMethod));
            return;
        }
        if (expr.Args.Length == 1)
        {
            EmitStringUnaryBuiltinCall(expr, RuntimeMethod(getMethod));
            return;
        }
        if (expr.Args.Length == 0)
        {
            EmitNoArgBuiltinCall(expr, RuntimeMethod(getMethod));
            return;
        }
        throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 0, 1 o 2 argomenti.");
    }

    /// <summary>AI.AGENTRUN name, goal [, rounds] — name+goal stringa, rounds opzionale
    /// (int, default 5). Firma IL: AgentRunCua(string, string, int).</summary>
    private void EmitAgentRunCuaCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 2 or > 3)
            throw new NotSupportedException("Backend IL: AI.AGENTRUN richiede name, goal [, rounds].");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        if (expr.Args.Length == 3) EmitInt32Argument(expr.Args[2]);
        else _il.Emit(OpCodes.Ldc_I4, 5);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AgentRunCua), typeof(string), typeof(string), typeof(int)));
    }

    /// <summary>AI.AGENTRUNGRAPH name, prompt [, rounds] — l'agente esplora il knowledge graph.
    /// Firma IL: AgentRunGraph(string, string, int).</summary>
    private void EmitAgentRunGraphCall(BuiltinFunctionCallExpr expr) =>
        EmitAgentRunXxxCall(expr, "AgentRunGraph", "AI.AGENTRUNGRAPH");

    /// <summary>AI.AGENTRUNRAG name, prompt [, rounds] — l'agente interroga i brain RAG.
    /// Firma IL: AgentRunRag(string, string, int).</summary>
    private void EmitAgentRunRagCall(BuiltinFunctionCallExpr expr) =>
        EmitAgentRunXxxCall(expr, "AgentRunRag", "AI.AGENTRUNRAG");

    /// <summary>AI.AGENTRUNCODE name, prompt [, rounds] — l'agente interroga il grafo del codice.
    /// Firma IL: AgentRunCode(string, string, int).</summary>
    private void EmitAgentRunCodeCall(BuiltinFunctionCallExpr expr) =>
        EmitAgentRunXxxCall(expr, "AgentRunCode", "AI.AGENTRUNCODE");

    private void EmitAgentRunXxxCall(BuiltinFunctionCallExpr expr, string runtimeMethod, string basicName)
    {
        if (expr.Args.Length is < 2 or > 3)
            throw new NotSupportedException($"Backend IL: {basicName} richiede name, prompt [, rounds].");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        if (expr.Args.Length == 3) EmitInt32Argument(expr.Args[2]);
        else _il.Emit(OpCodes.Ldc_I4, 5);
        _il.Emit(OpCodes.Call, RuntimeMethod(runtimeMethod, typeof(string), typeof(string), typeof(int)));
    }

    /// <summary>COMPUTER.SCREENSHOT([path]) — path opzionale (string, default ""). Firma (string).</summary>
    private void EmitComputerScreenshotCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length > 1)
            throw new NotSupportedException("Backend IL: COMPUTER.SCREENSHOT accetta al più il path.");
        if (expr.Args.Length == 1) EmitStringArg(expr.Args[0]);
        else _il.Emit(OpCodes.Ldstr, "");
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ComputerScreenshot), typeof(string)));
    }

    private void EmitComputerScreenRecordCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length > 2)
            throw new NotSupportedException("Backend IL: COMPUTER.SCREENRECORD accetta al più path e fps.");
        if (expr.Args.Length >= 1) EmitStringArg(expr.Args[0]); else _il.Emit(OpCodes.Ldstr, "");
        if (expr.Args.Length == 2) EmitDoubleArgument(expr.Args[1]); else _il.Emit(OpCodes.Ldc_R8, 10.0);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ComputerScreenRecord), typeof(string), typeof(double)));
    }

    private void EmitBrowserScreenshotCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length > 1)
            throw new NotSupportedException("Backend IL: BROWSER.SCREENSHOT accetta al più il path.");
        if (expr.Args.Length == 1) EmitStringArg(expr.Args[0]);
        else _il.Emit(OpCodes.Ldstr, "");
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.BrowserScreenshot), typeof(string)));
    }

    private void EmitBrowserSetCookieCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 2 or > 3)
            throw new NotSupportedException("Backend IL: BROWSER.SETCOOKIE richiede name, value e url opzionale.");
        EmitStringArg(expr.Args[0]); EmitStringArg(expr.Args[1]);
        if (expr.Args.Length == 3) EmitStringArg(expr.Args[2]); else _il.Emit(OpCodes.Ldstr, "");
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.BrowserSetCookie), typeof(string), typeof(string), typeof(string)));
    }

private void EmitBrowserHumanScrollCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException("Backend IL: BROWSER.HUMANSCROLL richiede dy.");
        EmitInt32Argument(expr.Args[0]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.BrowserHumanScroll), typeof(int)));
    }

    private void EmitBrowserObserveCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length > 1)
            throw new NotSupportedException("Backend IL: BROWSER.OBSERVE richiede al piu' 1 argomento (preset).");
        _il.Emit(OpCodes.Ldnull); // preset null default
        if (expr.Args.Length == 1)
        {
            // sostituisci la ldnull con l'argomento stringa
            _il.Emit(OpCodes.Pop);
            EmitStringArg(expr.Args[0]);
        }
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.BrowserObserve), typeof(string)));
    }

    private void EmitBrowserFindCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length < 1 || expr.Args.Length > 2)
            throw new NotSupportedException("Backend IL: BROWSER.FIND richiede text[, regex].");
        EmitStringArg(expr.Args[0]);
        _il.Emit(OpCodes.Ldc_I4_0);
        if (expr.Args.Length == 2)
        {
            _il.Emit(OpCodes.Pop);
            EmitConditionBool(expr.Args[1]);
        }
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.BrowserFind), typeof(string), typeof(bool)));
    }

    private void EmitBrowserNetLogCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length > 1)
            throw new NotSupportedException("Backend IL: BROWSER.NETLOG richiede al piu' 1 argomento.");
        _il.Emit(OpCodes.Ldc_I4_0);
        if (expr.Args.Length == 1)
        {
            _il.Emit(OpCodes.Pop);
            EmitConditionBool(expr.Args[0]);
        }
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.BrowserNetLog), typeof(bool)));
    }

    private void EmitBrowserConsoleCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length > 1)
            throw new NotSupportedException("Backend IL: BROWSER.CONSOLE richiede al piu' 1 argomento.");
        _il.Emit(OpCodes.Ldc_I4_0);
        if (expr.Args.Length == 1)
        {
            _il.Emit(OpCodes.Pop);
            EmitConditionBool(expr.Args[0]);
        }
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.BrowserConsole), typeof(bool)));
    }

    private void EmitBrowserAuthCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: BROWSER.AUTH richiede op e name.");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.BrowserAuth), typeof(string), typeof(string)));
    }

    /// <summary>Builtin con UN argomento stringa e ritorno numerico (es. COMPUTER.TYPE/KEY).</summary>
    private void EmitStringToDoubleCall(BuiltinFunctionCallExpr expr, MethodInfo method)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede esattamente 1 argomento.");
        EmitStringArg(expr.Args[0]);
        _il.Emit(OpCodes.Call, method);
    }

    /// <summary>TOOL.REGISTER(name, desc) → IlRuntime.ToolRegister(string,string) → double.</summary>
    private void EmitToolRegisterCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: TOOL.REGISTER richiede name e description.");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ToolRegister), typeof(string), typeof(string)));
    }

    /// <summary>UI.TYPE(target, text) → IlRuntime.UiType(string,string) → bool (BitType: -1/0).</summary>
    private void EmitUiTypeCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: UI.TYPE richiede target e text.");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.UiType), typeof(string), typeof(string)));
    }

    /// <summary>AGENT.RUN goal [, max_steps] — goal obbligatorio (string), max_steps opzionale
    /// (int, default 8). Firma IL: AgentRun(string, int).</summary>
    private void EmitAgentRunCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException("Backend IL: AGENT.RUN richiede goal [, max_steps].");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length == 2) EmitInt32Argument(expr.Args[1]);
        else _il.Emit(OpCodes.Ldc_I4, 8);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AgentRun), typeof(string), typeof(int)));
    }

    /// <summary>AI AGENT SAVE/LOAD name [, path] — path opzionale (default name.agent.json).</summary>
    private void EmitAiAgentSaveLoadCall(BuiltinFunctionCallExpr expr, string method)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 1 o 2 argomenti.");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length == 2) EmitStringArg(expr.Args[1]);
        else _il.Emit(OpCodes.Ldstr, "");
        _il.Emit(OpCodes.Call, RuntimeMethod(method, typeof(string), typeof(string)));
    }

    /// <summary>BROWSER.START([engine], [port], [profile]) — engine/port opzionali (default
    /// %AUTO=0 / 0). engine è il CODICE NUMERICO %AUTO/%CHROMIUM/%OBSCURA (mai stringhe "auto"/
    /// "chromium"/"obscura" nel BASIC); port numerico. Firma IL: BrowserStartEngine(double,int,string).</summary>
    private void EmitBrowserStartCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length > 3)
            throw new NotSupportedException("Backend IL: BROWSER.START accetta al più engine, port e profile.");
        if (expr.Args.Length >= 1) EmitDoubleArgument(expr.Args[0]);
        else _il.Emit(OpCodes.Ldc_R8, 0.0);
        if (expr.Args.Length >= 2) EmitInt32Argument(expr.Args[1]);
        else _il.Emit(OpCodes.Ldc_I4_0);
        if (expr.Args.Length >= 3) EmitStringArg(expr.Args[2]);
        else _il.Emit(OpCodes.Ldstr, "");
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.BrowserStartEngine), typeof(double), typeof(int), typeof(string)));
    }

    /// <summary>AI PREFILL alias, "testo" [, {json}] — json opzionale (chunk/sampling).</summary>
    private void EmitAiPrefillCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 2 or > 3)
            throw new NotSupportedException("Backend IL: AI PREFILL richiede 2 o 3 argomenti (alias, testo [, json]).");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        if (expr.Args.Length == 3) EmitStringArg(expr.Args[2]);
        else _il.Emit(OpCodes.Ldstr, "");
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiPrefill), typeof(string), typeof(string), typeof(string)));
    }

    /// <summary>AI DECODE alias [, max_tokens] — max_tokens opzionale (default del completer).</summary>
    /// <summary>AI LORA APPLY "ckptPrefisso" [, {model=..., dataset=...}] — deploy/verifica
    /// adapter: ckpt (string), dict opzioni opzionale passato come object.</summary>
    private void EmitAiLoraApplyCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException("Backend IL: AI LORA APPLY richiede 1 o 2 argomenti (ckpt [, optsDict]).");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length == 2)
        {
            EmitExpression(expr.Args[1]);
            var t = NormalizeInferredType(InferType(expr.Args[1]));
            var clr = DataTypeMapper.Map(t, _types, _classTypes);
            if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
        }
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiLoraApply), typeof(string), typeof(object)));
    }

    /// <summary>AI FINETUNE "dirModelo" [, {json|dict}] — path modello (string), dict di
    /// opzioni opzionale (passato come object: dataset/holdout/ckpt/rank/steps/batch/lr/seed).</summary>
    private void EmitAiFinetuneCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException("Backend IL: AI FINETUNE richiede 1 o 2 argomenti (model [, optsDict]).");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length == 2)
        {
            EmitExpression(expr.Args[1]);
            var t = NormalizeInferredType(InferType(expr.Args[1]));
            var clr = DataTypeMapper.Map(t, _types, _classTypes);
            if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
        }
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiFinetune), typeof(string), typeof(object)));
    }

    private void EmitAiDpoCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException("Backend IL: TRAIN DPO richiede 1 o 2 argomenti (model [, optsDict]).");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length == 2)
        {
            EmitExpression(expr.Args[1]);
            var t = NormalizeInferredType(InferType(expr.Args[1]));
            var clr = DataTypeMapper.Map(t, _types, _classTypes);
            if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
        }
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiDpo), typeof(string), typeof(object)));
    }

    private void EmitAiGrpoCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException("Backend IL: TRAIN GRPO richiede 1 o 2 argomenti (model [, optsDict]).");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length == 2)
        {
            EmitExpression(expr.Args[1]);
            var t = NormalizeInferredType(InferType(expr.Args[1]));
            var clr = DataTypeMapper.Map(t, _types, _classTypes);
            if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
        }
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiGrpo), typeof(string), typeof(object)));
    }

    /// <summary>AI DISTILL BUILD "jsonl" [, {json|dict}] — come AI FINETUNE, ritorna report string.</summary>
    private void EmitAiDistillBuildCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException("Backend IL: AI DISTILL BUILD richiede 1 o 2 argomenti (jsonl [, optsDict]).");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length == 2)
        {
            EmitExpression(expr.Args[1]);
            var t = NormalizeInferredType(InferType(expr.Args[1]));
            var clr = DataTypeMapper.Map(t, _types, _classTypes);
            if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
        }
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiDistillBuild), typeof(string), typeof(object)));
    }

    /// <summary>AI DISTILL SAMPLE prompts$ [, {json|dict}] — come BUILD, ritorna report string.</summary>
    private void EmitAiDistillSampleCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException("Backend IL: AI DISTILL SAMPLE richiede 1 o 2 argomenti (prompts [, optsDict]).");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length == 2)
        {
            EmitExpression(expr.Args[1]);
            var t = NormalizeInferredType(InferType(expr.Args[1]));
            var clr = DataTypeMapper.Map(t, _types, _classTypes);
            if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
        }
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiDistillSample), typeof(string), typeof(object)));
    }

    /// <summary>AI DISTILL EVAL prompts$ [, {json|dict}] — come SAMPLE, ritorna report string.</summary>
    private void EmitAiDistillEvalCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException("Backend IL: AI DISTILL EVAL richiede 1 o 2 argomenti (prompts [, optsDict]).");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length == 2)
        {
            EmitExpression(expr.Args[1]);
            var t = NormalizeInferredType(InferType(expr.Args[1]));
            var clr = DataTypeMapper.Map(t, _types, _classTypes);
            if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
        }
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiDistillEval), typeof(string), typeof(object)));
    }

    /// <summary>AI.DISTILLEVALTOK(idx) / AI.DISTILLEVALPASS(idx) — un argomento (indice 0/1).</summary>
    private void EmitAiDistillEvalIdxCall(BuiltinFunctionCallExpr expr, string method)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException($"Backend IL: {method} richiede 1 argomento (idx).");
        EmitExpression(expr.Args[0]);
        var t = NormalizeInferredType(InferType(expr.Args[0]));
        var clr = DataTypeMapper.Map(t, _types, _classTypes);
        if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
        _il.Emit(OpCodes.Call, RuntimeMethod(method, typeof(object)));
    }

    /// <summary>AI DISTILL APPLY ckpt$ [, {json|dict}] — ritorna report string.</summary>
    private void EmitAiDistillApplyCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException("Backend IL: AI DISTILL APPLY richiede 1 o 2 argomenti (ckpt [, optsDict]).");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length == 2)
        {
            EmitExpression(expr.Args[1]);
            var t = NormalizeInferredType(InferType(expr.Args[1]));
            var clr = DataTypeMapper.Map(t, _types, _classTypes);
            if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
        }
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiDistillApply), typeof(string), typeof(object)));
    }

    /// <summary>AI DISTILL TRAIN dataset$ [, {json|dict}] — ritorna report string.</summary>
    private void EmitAiDistillTrainCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException("Backend IL: AI DISTILL TRAIN richiede 1 o 2 argomenti (dataset [, optsDict]).");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length == 2)
        {
            EmitExpression(expr.Args[1]);
            var t = NormalizeInferredType(InferType(expr.Args[1]));
            var clr = DataTypeMapper.Map(t, _types, _classTypes);
            if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
        }
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiDistillTrain), typeof(string), typeof(object)));
    }

    /// <summary>AI DECODE alias [, max_tokens] — alias (string), max_tokens opzionale.</summary>
    private void EmitAiDecodeCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException("Backend IL: AI DECODE richiede 1 o 2 argomenti (alias [, max_tokens]).");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length == 2) EmitInt32Argument(expr.Args[1]);
        else _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiDecode), typeof(string), typeof(int)));
    }

    /// <summary>AI CACHE TRIM alias, <n> — n numerico (double), come gli altri builtin AI.</summary>
    private void EmitAiCacheTrimCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: AI CACHE TRIM richiede 2 argomenti (alias, n).");
        EmitStringArg(expr.Args[0]);
        EmitDoubleArgument(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiCacheTrim), typeof(string), typeof(double)));
    }

    /// <summary>AI SERVER START <dir> [port] — dir (string), porta opzionale (double, default 8020).</summary>
    private void EmitAiServerStartCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException("Backend IL: AI SERVER START richiede 1 o 2 argomenti (dir [, port]).");
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length == 2)
        {
            EmitDoubleArgument(expr.Args[1]);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiServerStartPort), typeof(string), typeof(double)));
        }
        else
        {
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiServerStart), typeof(string)));
        }
    }

    /// <summary>LLM MODEL LOAD nome, path [, RESIDENT] — nome/path string, flag opzionale (string).</summary>
    private void EmitAiModelLoadCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 2 or > 3)
            throw new NotSupportedException("Backend IL: LLM MODEL LOAD richiede 2 o 3 argomenti (nome, path [, RESIDENT]).");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        if (expr.Args.Length == 3) EmitStringArg(expr.Args[2]);
        else _il.Emit(OpCodes.Ldstr, "");
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiModelLoad), typeof(string), typeof(string), typeof(string)));
    }

    /// <summary>AI MODEL CONVERT src, dst — converte un checkpoint HF/MLX in un container `GGUF`.</summary>
    private void EmitAiModelConvertCall(BuiltinFunctionCallExpr expr)
    {
        // src, dst[, fmt] — fmt opzionale (GGUF|MLX); default "GGUF".
        if (expr.Args.Length < 2 || expr.Args.Length > 3)
            throw new NotSupportedException("Backend IL: LLM MODEL CONVERT richiede 2 o 3 argomenti (src, dst[, fmt]).");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        if (expr.Args.Length >= 3) EmitStringArg(expr.Args[2]); else _il.Emit(OpCodes.Ldstr, "GGUF");
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiModelConvert), typeof(string), typeof(string), typeof(string)));
    }

    /// <summary>MODEL COMPILER LLM: emette la chiamata a IlRuntime.Llm* (P/Invoke al runtime C
    /// CONDIVISO, dylib gmb_ai_llm). `sig`: 's'=stringa, 'd'=double. Argomenti mancanti → "" / 0
    /// (il runtime C valida e dà errore esplicito). La logica NON è ricopiata: è chiamata
    /// (AGENTS: C#→C ammesso; C→C# mai).</summary>
    private void EmitLlmCall(BuiltinFunctionCallExpr expr, System.Reflection.MethodInfo method, string sig)
    {
        for (var i = 0; i < sig.Length; i++)
        {
            var arg = i < expr.Args.Length ? expr.Args[i] : null;
            if (sig[i] == 'd')
            {
                if (arg != null) EmitDoubleArgument(arg); else _il.Emit(OpCodes.Ldc_R8, 0.0);
            }
            else
            {
                if (arg != null) EmitStringArg(arg); else _il.Emit(OpCodes.Ldstr, "");
            }
        }
        _il.Emit(OpCodes.Call, method);
    }

    /// <summary>AI MODEL QUANTIZE src, dst, bits[, group] — quantizza un modello HF/MLX (RTN affine).</summary>
    private void EmitAiModelQuantizeCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length < 3 || expr.Args.Length > 6)
            throw new NotSupportedException("Backend IL: AI MODEL QUANTIZE richiede 3..6 argomenti (src, dst, bits[, group][, imatrix][, expert_bits]).");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        EmitDoubleArgument(expr.Args[2]);
        if (expr.Args.Length >= 4) EmitDoubleArgument(expr.Args[3]); else _il.Emit(OpCodes.Ldc_R8, 64.0);
        if (expr.Args.Length >= 5) EmitStringArg(expr.Args[4]); else _il.Emit(OpCodes.Ldstr, "");
        if (expr.Args.Length >= 6) EmitDoubleArgument(expr.Args[5]); else _il.Emit(OpCodes.Ldc_R8, 0.0);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiModelQuantize), typeof(string), typeof(string), typeof(double), typeof(double), typeof(string), typeof(double)));
    }

    /// <summary>AI MODEL INSPECT dir — dump read-only del descriptor del modello (report su stdout).</summary>
    private void EmitAiModelInspectCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException("Backend IL: AI MODEL INSPECT richiede 1 argomento (dir).");
        EmitStringArg(expr.Args[0]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiModelInspect), typeof(string)));
    }

    /// <summary>LLM MODEL SAVE src, dst — EMITTER: round-trip/emissione dell'artefatto (gmb_llm_model_save).</summary>
    private void EmitAiModelSaveCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: LLM MODEL SAVE richiede 2 argomenti (src, dst).");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiModelSave), typeof(string), typeof(string)));
    }

    /// <summary>LLM MODEL ABLATE src, dst, pattern — MODIFICARE: azzera i tensori che combaciano.</summary>
    private void EmitAiModelAblateCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 3)
            throw new NotSupportedException("Backend IL: LLM MODEL ABLATE richiede 3 argomenti (src, dst, pattern).");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        EmitStringArg(expr.Args[2]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiModelAblate), typeof(string), typeof(string), typeof(string)));
    }

    /// <summary>LLM MODEL REMOVE src, dst, layer — RIMUOVERE: elimina un layer + rinumerazione.</summary>
    private void EmitAiModelRemoveCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 3)
            throw new NotSupportedException("Backend IL: LLM MODEL REMOVE richiede 3 argomenti (src, dst, layer).");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        EmitStringArg(expr.Args[2]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiModelRemove), typeof(string), typeof(string), typeof(string)));
    }

    /// <summary>LLM MODEL DIFF a, b, out — report strutturale (checkpoint/rollback + quality gate).</summary>
    private void EmitAiModelDiffCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 3)
            throw new NotSupportedException("Backend IL: LLM MODEL DIFF richiede 3 argomenti (a, b, out).");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        EmitStringArg(expr.Args[2]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiModelDiff), typeof(string), typeof(string), typeof(string)));
    }

    /// <summary>LLM MODEL CHECKPOINT src, store, name — snapshot nel checkpoint-store.</summary>
    private void EmitAiModelCheckpointCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 3)
            throw new NotSupportedException("Backend IL: LLM MODEL CHECKPOINT richiede 3 argomenti (src, store, name).");
        EmitStringArg(expr.Args[0]); EmitStringArg(expr.Args[1]); EmitStringArg(expr.Args[2]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiModelCheckpoint), typeof(string), typeof(string), typeof(string)));
    }

    /// <summary>LLM MODEL ROLLBACK store, name, dst — ripristina dal checkpoint-store.</summary>
    private void EmitAiModelRollbackCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 3)
            throw new NotSupportedException("Backend IL: LLM MODEL ROLLBACK richiede 3 argomenti (store, name, dst).");
        EmitStringArg(expr.Args[0]); EmitStringArg(expr.Args[1]); EmitStringArg(expr.Args[2]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiModelRollback), typeof(string), typeof(string), typeof(string)));
    }

    /// <summary>LLM MODEL ADD base, module, out — AGGIUNGERE: merge dei tensori di un modulo.</summary>
    private void EmitAiModelAddCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 3)
            throw new NotSupportedException("Backend IL: LLM MODEL ADD richiede 3 argomenti (base, module, out).");
        EmitStringArg(expr.Args[0]); EmitStringArg(expr.Args[1]); EmitStringArg(expr.Args[2]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiModelAdd), typeof(string), typeof(string), typeof(string)));
    }

    /// <summary>LLM MODEL ADAPTER base, adapter, out [, alpha] — AGGIUNGERE adapter LoRA (.safetensors).</summary>
    private void EmitAiModelAdapterCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length < 3 || expr.Args.Length > 4)
            throw new NotSupportedException("Backend IL: LLM MODEL ADAPTER richiede 3 o 4 argomenti (base, adapter, out[, alpha]).");
        EmitStringArg(expr.Args[0]); EmitStringArg(expr.Args[1]); EmitStringArg(expr.Args[2]);
        if (expr.Args.Length >= 4) EmitStringArg(expr.Args[3]); else _il.Emit(OpCodes.Ldstr, "1");
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiModelAdapter), typeof(string), typeof(string), typeof(string), typeof(string)));
    }

    /// <summary>LLM MODEL MODULE base, gmmod, module, out — AGGIUNGERE modulo dichiarato (.gmmod).</summary>
    private void EmitAiModelModuleCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 4)
            throw new NotSupportedException("Backend IL: LLM MODEL MODULE richiede 4 argomenti (base, gmmod, module, out).");
        EmitStringArg(expr.Args[0]); EmitStringArg(expr.Args[1]); EmitStringArg(expr.Args[2]); EmitStringArg(expr.Args[3]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiModelModule), typeof(string), typeof(string), typeof(string), typeof(string)));
    }

    /// <summary>LLM MODEL MODALITY base, mmproj, out — emit del .gmmod (modality dall'ABI).</summary>
    private void EmitAiModelModalityCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 3)
            throw new NotSupportedException("Backend IL: LLM MODEL MODALITY richiede 3 argomenti (base, mmproj, out).");
        EmitStringArg(expr.Args[0]); EmitStringArg(expr.Args[1]); EmitStringArg(expr.Args[2]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiModelModality), typeof(string), typeof(string), typeof(string)));
    }

    /// <summary>LLM MODEL VALIDATE path, out — verifica integrità strutturale.</summary>
    private void EmitAiModelValidateCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: LLM MODEL VALIDATE richiede 2 argomenti (path, out).");
        EmitStringArg(expr.Args[0]); EmitStringArg(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiModelValidate), typeof(string), typeof(string)));
    }

    /// <summary>LLM MODEL TRACE nome, path|OFF (K1): registra/ferma il trace esperti.</summary>
    private void EmitAiModelTraceCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: LLM MODEL TRACE richiede 2 argomenti (nome, path|OFF).");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiModelTrace), typeof(string), typeof(string)));
    }

    /// <summary>LLM MODEL CACHE nome, MB.</summary>
    private void EmitAiModelCacheCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: LLM MODEL CACHE richiede 2 argomenti (nome, MB).");
        EmitStringArg(expr.Args[0]);
        EmitDoubleArgument(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiModelCache), typeof(string), typeof(double)));
    }

    /// <summary>LLM MODEL PREFETCH nome, topn.</summary>
    private void EmitAiModelPrefetchCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: LLM MODEL PREFETCH richiede 2 argomenti (nome, topn).");
        EmitStringArg(expr.Args[0]);
        EmitDoubleArgument(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiModelPrefetch), typeof(string), typeof(double)));
    }

    /// <summary>LLM MODEL DEVICE nome, AUTO|CPU|METAL.</summary>
    private void EmitAiModelDeviceCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: LLM MODEL DEVICE richiede 2 argomenti (nome, modo).");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiModelDevice), typeof(string), typeof(string)));
    }

    /// <summary>AI BENCHMARK alias, PREFILL|DECODE|ALL [, {json}] — alias (string), mode</summary>
    /// (string), json opzionale (string, vuoto se assente).</summary>
    private void EmitAiBenchmarkCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 2 or > 3)
            throw new NotSupportedException("Backend IL: AI BENCHMARK richiede 2 o 3 argomenti (alias, mode [, json]).");
        EmitStringArg(expr.Args[0]);
        EmitStringArg(expr.Args[1]);
        if (expr.Args.Length == 3) EmitStringArg(expr.Args[2]);
        else _il.Emit(OpCodes.Ldstr, "");
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiBenchmark), typeof(string), typeof(string), typeof(string)));
    }

    private void EmitAiOcrCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 3)
            throw new NotSupportedException("Backend IL: AI.OCR richiede 1, 2 o 3 argomenti.");
        EmitExpression(expr.Args[0]);
        if (expr.Args.Length == 1)
        {
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiOcr), typeof(string)));
            return;
        }
        EmitExpression(expr.Args[1]);
        if (expr.Args.Length == 2)
        {
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiOcrFull), typeof(string), typeof(string)));
            return;
        }
        // 3 argomenti: path, prompt, redact (bool) → AiOcrFull(path, prompt, redact).
        EmitDoubleArgument(expr.Args[2]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiOcrFull),
            typeof(string), typeof(string), typeof(double)));
    }

    // F3 multimodale (2026-09-11): AI.IMAGEINFO/RESIZE/CROP/CONVERT → IlRuntime.AiImage*.
    /// <summary>Argomento-dimensione: numerici via EmitDoubleArgument; stringhe/Any via
    /// IlRuntime.SafeDouble (parità PCode: "x" → 0). Senza, IlConversion.EmitConvert lascia
    /// la stringa sullo stack e la call fallisce con InvalidProgramException.</summary>
    private void EmitImageDimArg(Expr arg)
    {
        var t = NormalizeInferredType(InferType(arg));
        if (t is StringType or AnyType or Basic.JsonType)
        {
            EmitExpression(arg);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.SafeDouble), typeof(object)));
            return;
        }
        EmitDoubleArgument(arg);
    }
    private void EmitAiImageResizeCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 4)
            throw new NotSupportedException("Backend IL: AI.IMAGERESIZE richiede 4 argomenti (input, output, width, height).");
        EmitExpression(expr.Args[0]);
        EmitExpression(expr.Args[1]);
        EmitImageDimArg(expr.Args[2]);
        EmitImageDimArg(expr.Args[3]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiImageResize),
            typeof(string), typeof(string), typeof(double), typeof(double)));
    }

    private void EmitAiImageCropCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 6)
            throw new NotSupportedException("Backend IL: AI.IMAGECROP richiede 6 argomenti (input, output, x, y, width, height).");
        EmitExpression(expr.Args[0]);
        EmitExpression(expr.Args[1]);
        EmitImageDimArg(expr.Args[2]);
        EmitImageDimArg(expr.Args[3]);
        EmitImageDimArg(expr.Args[4]);
        EmitImageDimArg(expr.Args[5]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiImageCrop),
            typeof(string), typeof(string), typeof(double), typeof(double), typeof(double), typeof(double)));
    }

    private void EmitAiImageConvertCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: AI.IMAGECONVERT richiede 2 argomenti (input, output).");
        EmitExpression(expr.Args[0]);
        EmitExpression(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiImageConvert),
            typeof(string), typeof(string)));
    }

    // F2 multimodale (2026-09-11): AI.DOCFIELDS/DOCEXTRACT → IlRuntime.AiDoc*.
    private void EmitAiDocExtractCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: AI.DOCEXTRACT richiede 2 argomenti (path, schema).");
        EmitExpression(expr.Args[0]);
        EmitExpression(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiDocExtract),
            typeof(string), typeof(string)));
    }

    // F4 multimodale (2026-09-11): AI.SYNTHESIZE(text, output) → IlRuntime.AiSynthesize.
    private void EmitAiSynthesizeCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: AI.SYNTHESIZE richiede 2 argomenti (text, output).");
        EmitExpression(expr.Args[0]);
        EmitExpression(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiSynthesize),
            typeof(string), typeof(string)));
    }

    // F5 multimodale (2026-09-11): embedding/compare/detect/classify.
    private void EmitAiImageEmbedCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException("Backend IL: AI.IMAGEEMBED richiede 1 argomento (path).");
        EmitExpression(expr.Args[0]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiImageEmbed), typeof(string)));
    }

    private void EmitAiImageCompareCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: AI.IMAGECOMPARE richiede 2 argomenti (path1, path2).");
        EmitExpression(expr.Args[0]);
        EmitExpression(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiImageCompare),
            typeof(string), typeof(string)));
    }

    private void EmitAiImageClassifyCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: AI.IMAGECLASSIFY richiede 2 argomenti (path, labels).");
        EmitExpression(expr.Args[0]);
        EmitExpression(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiImageClassify),
            typeof(string), typeof(string)));
    }

    // F6 multimodale (2026-09-11): AI.AGENTASKFILE(name, path [, question]).
    private void EmitAiAgentAskFileCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 2 or > 3)
            throw new NotSupportedException("Backend IL: AI.AGENTASKFILE richiede 2 o 3 argomenti (name, path [, question]).");
        EmitExpression(expr.Args[0]);
        EmitExpression(expr.Args[1]);
        if (expr.Args.Length == 3) EmitExpression(expr.Args[2]);
        else _il.Emit(OpCodes.Ldstr, "");
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiAgentAskFile),
            typeof(string), typeof(string), typeof(string)));
    }

    // F7 multimodale (2026-09-11): AI.VIDEOFRAME(path, index, output).
    private void EmitAiVideoFrameCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 3)
            throw new NotSupportedException("Backend IL: AI.VIDEOFRAME richiede 3 argomenti (path, index, output).");
        EmitExpression(expr.Args[0]);
        EmitDoubleArgument(expr.Args[1]);
        EmitExpression(expr.Args[2]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiVideoFrame),
            typeof(string), typeof(double), typeof(string)));
    }

    private void EmitAiVideoSnapshotCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 2)
            throw new NotSupportedException("Backend IL: AI.VIDEOSNAPSHOT richiede 2 argomenti (path, output).");
        EmitExpression(expr.Args[0]);
        EmitExpression(expr.Args[1]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiVideoSnapshot),
            typeof(string), typeof(string)));
    }

    /// <summary>AI.SETMODEL(name,model)/AI.GETMODEL(name): stesso TokenKind.AiModel condiviso
    /// per due overload di arità diversa (vedi StdLib.AI.cs) — il p-code VM disambigua per
    /// conteggio argomenti (2 → registra il modello, altrimenti "OK", GETMODEL non è realmente
    /// implementato nemmeno lì). Stessa logica qui.</summary>
    private void EmitAiModelCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length == 2)
        {
            EmitExpression(expr.Args[0]);
            EmitExpression(expr.Args[1]);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.AiSetModel)));
            return;
        }
        EmitStringUnaryBuiltinCall(expr, RuntimeMethod(nameof(IlRuntime.AiGetModel)));
    }

    /// <summary>Gruppo di funzioni AI.* non ancora realmente implementate nemmeno nel p-code VM
    /// (PCodeProcessor.Builtins.cs, gruppo AITEMP/AIBASE/AIINFO/AISAVE/AILOAD/AIDEF/AION/AIOFF)
    /// — restituiscono sempre "OK" a prescindere dagli argomenti. Gli argomenti vengono
    /// comunque valutati (mai saltati: un'espressione argomento con side-effect, es. una
    /// chiamata a funzione, deve eseguire sempre) e scartati.</summary>
    /// <summary>CHART.PLOT handle, x(), y(), type$ [, legend$] → IGraphicsService.ChartPlot.</summary>
    private void EmitChartPlotStatement(ChartPlotStmt stmt)
    {
        var args = stmt.Args;
        if (args.Length < 4)
        {
            foreach (var a in args) { EmitExpression(a); _il.Emit(OpCodes.Pop); }
            return;
        }
        EmitLoadGraphicsService();
        EmitInt64Argument(args[0]);
        EmitArrayAsDoubleArray(args[1]);
        EmitArrayAsDoubleArray(args[2]);
        EmitStringArg(args[3]);
        if (args.Length > 4 && args[4] is not null) EmitStringArg(args[4]); else _il.Emit(OpCodes.Ldstr, "");
        _il.Emit(OpCodes.Callvirt, GraphicsChartPlotMethod);
    }

    /// <summary>CHART.FINANCIAL handle, ohlc(), type$ [, legend$] — dati OHLC: delega al
    /// plot generico con le serie (il costruttore OhlcData[] è specifico CommonGraphics).</summary>
    private void EmitChartFinancialStatement(ChartFinancialStmt stmt)
    {
        var args = stmt.Args;
        if (args.Length < 3)
        {
            foreach (var a in args) { EmitExpression(a); _il.Emit(OpCodes.Pop); }
            return;
        }
        EmitLoadGraphicsService();
        EmitInt64Argument(args[0]);
        EmitArrayAsDoubleArray(args[1]);
        EmitStringArg(args[2]);
        if (args.Length > 3 && args[3] is not null) EmitStringArg(args[3]); else _il.Emit(OpCodes.Ldstr, "");
        _il.Emit(OpCodes.Callvirt, GraphicsChartFinancialMethod);
    }

    /// <summary>CANVAS NEW/USE/… → IlRuntime.CanvasOp (canvas offscreen, runtime C).
    /// ABI uniforme: op string + 4 double (0 se assenti) + path string ("" se assente).</summary>
    private void EmitCanvasStmt(CanvasStmt cs)
    {
        var a = cs.Args;
        Expr? PathExpr() => cs.Sub switch
        {
            "SAVE" => a.Length > 1 ? a[1] : null,
            "LOAD" => a.Length > 0 ? a[0] : null,
            _ => null
        };

        _il.Emit(OpCodes.Ldstr, cs.Sub);
        for (int i = 0; i < 4; i++)
        {
            Expr? e = cs.Sub switch
            {
                "SAVE" => (i == 0 && a.Length > 0) ? a[0] : null,
                "LOAD" => null,
                _ => i < a.Length ? a[i] : null
            };
            // Colori (CLEAR arg 1, SET arg 3): stessa via delle primitive (EmitInt32Arg),
            // così valgono le regole di PSET/LINE (indice palette o packed).
            bool isColor = (cs.Sub == "CLEAR" && i == 1) || (cs.Sub == "SET" && i == 3);
            if (e == null) _il.Emit(OpCodes.Ldc_R8, 0.0);
            else if (isColor) { EmitInt32Arg(e); _il.Emit(OpCodes.Conv_R8); }
            else EmitDoubleArgument(e);
        }
        var pe = PathExpr();
        if (pe != null) EmitStringArg(pe); else _il.Emit(OpCodes.Ldstr, "");
        _il.Emit(OpCodes.Call, IlRuntimeCanvasOpMethod);
        // Il risultato della call è un double: va coercizzato al tipo del target
        // (LONG/INTEGER) prima dello store, altrimenti IL invalido o valore perso.
        if (cs.Into is VariableTarget gv) EmitAssignTargetFromTop(gv, new DoubleType()); else _il.Emit(OpCodes.Pop);
    }

    /// <summary>CHART DRAW handle, "op", args[] → IlRuntime.ChartDraw (oggetti di disegno).</summary>
    private void EmitChartDraw(ChartDrawStmt cm)
    {
        EmitInt32Argument(cm.Handle);
        EmitStringArg(cm.Op);
        var n = cm.Args.Length;
        _il.Emit(OpCodes.Ldc_I4, n);
        _il.Emit(OpCodes.Newarr, typeof(double));
        for (int i = 0; i < n; i++)
        {
            _il.Emit(OpCodes.Dup); _il.Emit(OpCodes.Ldc_I4, i);
            EmitDoubleArgument(cm.Args[i]);
            _il.Emit(OpCodes.Stelem_R8);
        }
        _il.Emit(OpCodes.Call, IlRuntimeChartDrawMethod);
        if (cm.Into is VariableTarget gv) EmitAssignTargetFromTop(gv); else _il.Emit(OpCodes.Pop);
    }

    /// <summary>CHART SET/GET/GETS → IGraphicsService.ChartConfigSet/Get/GetStr (configurazione
    /// grafico da BASIC: rendering, view window zoom/pan, colori, titolo).</summary>
    private void EmitChartConfig(ChartConfigStmt cm)
    {
        // Via IlRuntime (statico) come CONTROL SET/GET: gestisce headless/CGfx in un solo punto.
        EmitInt32Argument(cm.Handle);
        EmitStringArg(cm.Prop);
        var sub = cm.Sub.ToUpperInvariant();
        var isSet = cm.SubKind == TokenKind.Set
            || (cm.SubKind == TokenKind.Identifier && sub == "SET");
        var isGet = cm.SubKind == TokenKind.Get
            || (cm.SubKind == TokenKind.Identifier && sub == "GET");
        if (isSet)
        {
            if (cm.Num != null) EmitDoubleArgument(cm.Num); else _il.Emit(OpCodes.Ldc_R8, 0.0);
            if (cm.Str != null) EmitStringArg(cm.Str); else _il.Emit(OpCodes.Ldstr, "");
            _il.Emit(OpCodes.Call, IlRuntimeChartConfigSetMethod);
        }
        else if (isGet)
        {
            _il.Emit(OpCodes.Call, IlRuntimeChartConfigGetMethod);
            if (cm.Into is VariableTarget gv) EmitAssignTargetFromTop(gv); else _il.Emit(OpCodes.Pop);
        }
        else
        {
            _il.Emit(OpCodes.Call, IlRuntimeChartConfigGetStrMethod);
            if (cm.Into is VariableTarget gs) EmitAssignTargetFromTop(gs, new StringType()); else _il.Emit(OpCodes.Pop);
        }
    }

    /// <summary>CHART.SHOW handle, x, y / CHART.SAVE handle, path$ → IGraphicsService.</summary>
    private void EmitChartRenderStatement(ChartRenderStmt stmt)
    {
        var args = stmt.Args;
        EmitLoadGraphicsService();
        if (args.Length >= 1) EmitInt64Argument(args[0]); else _il.Emit(OpCodes.Ldc_I8, 0L);
        if (stmt.IsSave)
        {
            if (args.Length >= 2) EmitStringArg(args[1]); else _il.Emit(OpCodes.Ldstr, "chart.bmp");
            _il.Emit(OpCodes.Callvirt, GraphicsChartSaveMethod);
        }
        else
        {
            if (args.Length >= 2) EmitInt32Arg(args[1]); else _il.Emit(OpCodes.Ldc_I4_0);
            if (args.Length >= 3) EmitInt32Arg(args[2]); else _il.Emit(OpCodes.Ldc_I4_0);
            _il.Emit(OpCodes.Callvirt, GraphicsChartShowMethod);
        }
    }

    private void EmitChartClearStatement(ChartClearStmt stmt)
    {
        EmitLoadGraphicsService();
        EmitInt64Argument(stmt.Handle);
        _il.Emit(OpCodes.Callvirt, GraphicsChartClearMethod);
    }

    private void EmitChartCloseStatement(ChartCloseStmt stmt)
    {
        EmitLoadGraphicsService();
        EmitInt64Argument(stmt.Handle);
        _il.Emit(OpCodes.Callvirt, GraphicsChartCloseMethod);
    }

    /// <summary>Array BASIC (int[]/double[]) → double[] per il runtime chart.</summary>
    private void EmitArrayAsDoubleArray(Expr expr)
    {
        EmitObjectBoxedArgument(expr);
        _il.Emit(OpCodes.Call, IlRuntimeArrayToDoubleArrayMethod);
    }

    private void EmitChartCreateCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length is < 1 or > 3)
            throw new NotSupportedException("Backend IL: CHART.CREATE richiede da 1 a 3 argomenti (titolo[, xLabel[, yLabel]]).");
        EmitLoadGraphicsService();
        EmitStringArg(expr.Args[0]);
        if (expr.Args.Length >= 2) EmitStringArg(expr.Args[1]); else _il.Emit(OpCodes.Ldstr, "");
        if (expr.Args.Length >= 3) EmitStringArg(expr.Args[2]); else _il.Emit(OpCodes.Ldstr, "");
        _il.Emit(OpCodes.Callvirt, GraphicsChartCreateMethod);
    }

    private void EmitSpriteCollideCall(BuiltinFunctionCallExpr expr, bool batch)
    {
        // COLLIDE(id1, id2) → -1/0 (GraphicsManager.Collide); COLLIDE(id) → int[]
        // (CollideAll); COLLIDEBATCH(X(), Y(), w, h) → int[] coppie AABB.
        // Sincrono: senza schermo/sprite restituisce false/array vuoto (come la PCodeVM).
        if (batch)
        {
            if (expr.Args.Length != 4)
                throw new NotSupportedException("Backend IL: COLLIDEBATCH richiede 4 argomenti (X(), Y(), w, h).");
            foreach (var a in expr.Args) EmitObjectBoxedArgument(a);
            _il.Emit(OpCodes.Call, IlRuntimeCollideBatchMethod);
            return;
        }
        if (expr.Args.Length == 2)
        {
            foreach (var a in expr.Args) EmitInt32Arg(a);
            _il.Emit(OpCodes.Call, IlRuntimeCollideMethod);
        }
        else if (expr.Args.Length == 1)
        {
            EmitInt32Arg(expr.Args[0]);
            _il.Emit(OpCodes.Call, IlRuntimeCollideAllMethod);
        }
        else
            throw new NotSupportedException("Backend IL: COLLIDE richiede 1 o 2 argomenti.");
    }

    private void EmitRndCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 0)
            throw new NotSupportedException("Backend IL: RND accetta zero argomenti.");
        _il.Emit(OpCodes.Call, IlRuntimeRndMethod);
    }

    private void EmitNativeCollectionBuiltinCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length == 0)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede una collection come primo argomento.");
        if (!TryGetNativeCollectionType(InferType(expr.Args[0]), out var collectionType))
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede una collection CLR nativa.");

        var method = collectionType.GetMethods(BindingFlags.Instance | BindingFlags.Public)
            .Where(candidate => candidate.Name.Equals(methodName, StringComparison.OrdinalIgnoreCase))
            .Where(candidate => candidate.GetParameters().Length == expr.Args.Length - 1)
            .SingleOrDefault()
            ?? throw new NotSupportedException($"Backend IL: {methodName} non disponibile per '{InferType(expr.Args[0])}'.");
        EmitExpression(expr.Args[0]);
        var parameters = method.GetParameters();
        for (var i = 0; i < parameters.Length; i++)
        {
            var arg = expr.Args[i + 1];
            EmitExpression(arg);
            var sourceType = NormalizeInferredType(InferType(arg));
            IlConversion.EmitConvert(_il, _types, sourceType, DataTypeMapper.Map(sourceType, _types, _classTypes), parameters[i].ParameterType);
        }
        _il.Emit(OpCodes.Callvirt, method);
    }

    private void EmitCollectionMinMaxCall(BuiltinFunctionCallExpr expr, bool min)
    {
        EmitExpression(expr.Args[0]);
        _il.Emit(OpCodes.Call, RuntimeMethod(min ? nameof(IlRuntime.CollectionMin) : nameof(IlRuntime.CollectionMax), typeof(object)));
    }

    private static bool IsCallbackFunctionReferenceCore(int symbolId, BasicProgram program)
    {
        var decl = symbolId >= 0 ? program.Symbols.GetSymbol(symbolId).DeclarationNode : null;
        if (decl is not ProcedureDefinition pd) return false;
        if (pd.Parameters.Length > 0) return true;
        // FunctionDefinition LOCALE a 0 parametri con CATTURE (heap promotion → è un
        // MethodMember di una classe __ClosureCtx): il riferimento (es. RETURN
        // ContatoreInterno2) è un DELEGATE, mai una chiamata — come le callback a
        // parametri. Le funzioni GLOBALI a 0-param (es. TIME) restano CHIAMATE.
        if (decl is FunctionDefinition)
        {
            var closureCount = program.Symbols.Count;
            for (var ci = 0; ci < closureCount; ci++)
            {
                var sym = program.Symbols.GetSymbol(ci);
                if (sym.DeclarationNode is ClassDefinition cd && sym.OriginalName.StartsWith("__ClosureCtx"))
                    foreach (var m in cd.Members)
                        if (m is MethodMember mm && mm.Procedure.SymbolId == symbolId)
                            return true;
            }
        }
        return false;
    }

    private void EmitFunctionReferenceAsCall(FunctionReferenceExpr fre)
    {
        // Riferimento bruto a funzione a 0 argomenti usato come VALORE (es.
        // `info("time") = TIME`): il P-Code valuta una referenza top-level a 0 argomenti
        // come CHIAMATA. Le referenze a callback reali (param) restano delegate.
        var decl = _program.Symbols.GetSymbol(fre.SymbolId).DeclarationNode;
        if (decl is FunctionDefinition fd && fd.Parameters.Length == 0)
        {
            EmitFunctionCall(new FunctionExpr(fre.SymbolId, fre.OriginalToken, ImmutableArray<Expr>.Empty, fd.ReturnType));
            return;
        }
        if (decl is NativeProcedureDefinition npd && npd.Parameters.Length == 0)
        {
            var nsToken = new Token(TokenKind.Identifier, "", null, fre.OriginalToken.Position);
            EmitBuiltinFunctionCall(new BuiltinFunctionCallExpr(nsToken, fre.OriginalToken, ImmutableArray<Expr>.Empty, npd.ReturnType));
            return;
        }
        throw new NotSupportedException($"Backend IL: FunctionReferenceExpr '{fre.OriginalToken.Lexeme}' (callback) non ancora supportata.");
    }

    /// <summary>True se <see cref="EmitFunctionReference"/> per questa FUNCTION emette il
    /// delegate `ref` di FfiRefCallbackTypes (e non il thunk by-value/<c>Func</c>). Serve ai
    /// call-site che applicano <see cref="EmitRefCallbackAdapter"/>: l'adattatore va emesso
    /// SOLO se sullo stack c'è davvero il delegate `ref`.</summary>
    private bool EmitsRefCallbackDelegate(FunctionDefinition fd)
    {
        if (!_procedureMethods.TryGetValue(fd.SymbolId, out var m)) return true;
        // Non statica (closure): CreateCallbackThunk non è applicabile → fallback al ref.
        if (!m.IsStatic) return true;
        if (!fd.Parameters.Any(p => p.Flags.HasFlag(SymbolFlags.IsByRef) || p.DataType is ArrayType))
            return false;
        // Thunk disponibile (TypeBuilder del programma) → EmitFunctionReference usa Func.
        return !_programTypeBuilders.TryGetValue(_program, out _);
    }

    private void EmitFunctionReference(FunctionReferenceExpr expr, bool ffiCallbackShape = false, bool refCallbackShape = false)
    {
        // Callback per AMAP/AFILTER/AREDUCE/ACOUNT/...: la FUNCTION è già un metodo CIL
        // statico (in _procedureMethods) — costruisce il delegate Func<> con la firma CLR
        // della FunctionDefinition (parametri + return), poi Ldftn + Newobj.
        /* Il simbolo è una LOCALE o un PARAMETRO (es. `next AS NEXTHANDLER` passato a
         * `GO SUB OperazioneLunga(req, res, next)`): il valore è già un oggetto/delegate sullo
         * stack, NON una procedura da compilare. Prima si cercava solo fra i metodi delle
         * procedure → NotSupportedException "callback 'next' non compilato", che rendeva
         * impossibile compilare qualunque middleware a 3 parametri sulla gamba IL
         * (bug trovato su samples/Http/HttpRoutes.bas). */
        if (_parameters.ContainsKey(expr.SymbolId) || _locals.ContainsKey(expr.SymbolId))
        {
            EmitLoadVariable(expr.SymbolId);
            return;
        }
        if (!_procedureMethods.TryGetValue(expr.SymbolId, out var fnMethod))
            throw new NotSupportedException($"Backend IL: callback '{expr.OriginalToken.Lexeme}' non compilato (bug interno).");
        var symbol = _program.Symbols.GetSymbol(expr.SymbolId);
        if (symbol.DeclarationNode is not FunctionDefinition fd)
            throw new NotSupportedException($"Backend IL: riferimento a funzione '{expr.OriginalToken.Lexeme}' non è una FUNCTION.");

        var paramClr = fd.Parameters
            .Select(p => DataTypeMapper.Map(NormalizeInferredType(p.DataType), _types, _classTypes))
            .ToArray();
        var retClr = DataTypeMapper.Map(NormalizeInferredType(fd.ReturnType), _types, _classTypes);
        // FUNCTION con parametri ARRAY:
        // - FFI (DECLARE NATIVE): il delegate deve combaciare con l'ABI C (array-first,
        //   `T[]&`): si usa FfiRefCallbackTypes.CbRef* (2026-11-02).
        // - Builtin (SolveIvp, LEAST SQUARES, FSOLVE, ...): il runtime aspetta Func/Action
        //   by-value. Il metodo BASIC ha gli array `T[]&` → serve un THUNK statico by-value
        //   che inoltra per indirizzo (vedi CreateCallbackThunk). Senza, il delegate
        //   CbRef1S1ArrD veniva passato dove il runtime si aspetta Func<...>: l'ordine dei
        //   parametri (array-first contro scalar-first) e il byref non combaciavano
        //   (IndexOutOfRange in NUMERICAL.SolveIvp).
        Type? refDel = (ffiCallbackShape || refCallbackShape) ? TryRefDelegateType(fd) : null;
        MethodBuilder fnTarget = fnMethod;
        if (refDel is null && fnMethod.IsStatic
            && fd.Parameters.Any(p => p.Flags.HasFlag(SymbolFlags.IsByRef) || p.DataType is ArrayType))
            fnTarget = CreateCallbackThunk(fd, fnMethod, retClr, paramClr) ?? fnMethod;
        refDel ??= ReferenceEquals(fnTarget, fnMethod) ? TryRefDelegateType(fd) : null;
        // SUB (VoidType): callback Action<T...> — Func con retClr=void è IL illegale.
        // Il tipo delegate viene da DataTypeMapper (Func/Action fino a 16 parametri, poi un
        // tipo generato a runtime): prima qui c'era uno switch scritto a mano che si fermava a
        // 3 parametri, quindi una FUNCTION nominata con 4+ parametri usata come callback
        // falliva SOLO sulla gamba IL.
        Type delegateType = refDel ?? DataTypeMapper.Map(
            new FunctionPointerType(
                fd.Parameters.Select(p => NormalizeInferredType(p.DataType)).ToImmutableArray(),
                NormalizeInferredType(fd.ReturnType)),
            _types, _classTypes);
        // Delegate generico chiuso che può contenere un TypeBuilder (es. Func<Punto, Punto>
        // con Punto = TYPE utente): GetConstructor diretto fallisce — serve il ponte
        // TypeBuilder.GetConstructor(closedType, openCtor). MA il ponte richiede che il
        // tipo chiuso CONTENGA davvero un TypeBuilder (List<double> → ArgumentException):
        // percorso diretto per i generici chiusi "normali" (vedi EmitNewNativeCollection).
        ConstructorInfo? ctor;
        if (!delegateType.IsGenericType)
        {
            // Delegate dedicato con parametri `ref` (FfiRefCallbackTypes): non generico.
            ctor = delegateType.GetConstructor([_types.Object, typeof(IntPtr)])
                ?? throw new NotSupportedException($"Backend IL: costruttore del delegate '{delegateType.Name}' non trovato.");
        }
        else
        {
            var openCtor = delegateType.GetGenericTypeDefinition().GetConstructor([_types.Object, typeof(IntPtr)])
                ?? throw new NotSupportedException($"Backend IL: costruttore del delegate '{delegateType.Name}' non trovato.");
            ctor = ClosedGenericContainsTypeBuilder(delegateType)
                ? TypeBuilder.GetConstructor(delegateType, openCtor)
                : delegateType.GetConstructor([_types.Object, typeof(IntPtr)])
                  ?? throw new NotSupportedException($"Backend IL: costruttore del delegate '{delegateType.Name}' non trovato.");
        }
        // Metodo d'ISTANZA della closure (__context heap-promotato): il delegate va legato
        // all'istanza della closure nel corpo corrente (local __context), mai null this.
        if (fnMethod.IsStatic)
        {
            _il.Emit(OpCodes.Ldnull);
        }
        else
        {
            var closureLocal = FindClosureContextLocal(fnMethod.DeclaringType!)
                ?? throw new NotSupportedException(
                    $"Backend IL: delegate a '{expr.OriginalToken.Lexeme}' (metodo di closure) senza istanza __context nel corpo corrente.");
            _il.Emit(OpCodes.Ldloc, closureLocal);
        }
        _il.Emit(OpCodes.Ldftn, fnTarget);
        _il.Emit(OpCodes.Newobj, ctor);
    }

    /// <summary>
    /// Crea (una volta per riferimento) un metodo statico con la firma BY-VALUE del delegate
    /// (double[], double, ...) che inoltra gli argomenti per INDIRIZZO al metodo BASIC reale
    /// (che dichiara gli array `T[]&` per il BYREF implicito del parser). È il ponte usato
    /// quando una FUNCTION nominata con parametri array viene passata come callback a un
    /// builtin che aspetta Func/Action: il CLR non può creare un delegate by-value su un
    /// metodo byref (verrebbe un thunk con ABI sbagliata). Restituisce null se il TypeBuilder
    /// del programma non è disponibile (fallback al delegate FfiRef dedicato).
    /// </summary>
    private MethodBuilder? CreateCallbackThunk(FunctionDefinition fd, MethodBuilder target,
        Type retClr, Type[] paramClr)
    {
        if (!_programTypeBuilders.TryGetValue(_program, out var programType)) return null;
        var seq = System.Threading.Interlocked.Increment(ref _callbackThunkSeq);
        var thunk = programType.DefineMethod(
            $"__cb_thunk_{fd.SymbolId}_{seq}",
            MethodAttributes.Private | MethodAttributes.Static | MethodAttributes.HideBySig,
            retClr, paramClr);
        var thunkIl = thunk.GetILGenerator();
        for (var i = 0; i < paramClr.Length; i++)
        {
            var byRef = MapParamClrTypeConsideringBox(
                fd.Parameters[i], i, fd.SymbolId, _program, _types, _classTypes).IsByRef;
            thunkIl.Emit(byRef ? OpCodes.Ldarga_S : OpCodes.Ldarg_S, (byte)i);
        }
        thunkIl.Emit(OpCodes.Call, target);
        thunkIl.Emit(OpCodes.Ret);
        return thunk;
    }

    private void EmitHttpFetchExpr(HttpFetchExpr expr)
    {
        // FETCH come ESPRESSIONE (DIM res AS FETCHRESULT = FETCH(url[, opts])): il parser
        // produce HttpFetchExpr (non BuiltinFunctionCallExpr).
        if (expr.CallbackName is not null)
            throw new NotSupportedException("Backend IL: FETCH asincrono con callback non ancora supportato.");
        EmitObjectBoxedArgument(expr.Url);
        if (expr.Options is not null) EmitObjectBoxedArgument(expr.Options);
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, IlRuntimeFetchFunctionMethod);
    }

    private void EmitSafeCast(SafeCastExpr expr)
    {
        // AS? (SafeCast): converte il valore al tipo target, NIL (null) se non convertibile
        // — come la VM (BuiltInFunction.SAFECAST: stringa numerica → numero, altrimenti null).
        var sourceType = NormalizeInferredType(InferType(expr.Expression));
        var targetType = NormalizeInferredType(expr.TargetType);

        // Una classe può dichiarare OPERATOR CTYPE(value) AS Target. Il fallback runtime
        // non può invocare un MethodBuilder del tipo generato, quindi il dispatch va
        // risolto staticamente qui mentre il TypeBuilder è ancora disponibile.
        if (sourceType is ClassType sourceClass
            && FindOperatorInHierarchy(sourceClass.Definition.SymbolId, TokenKind.CType, 1) is { } conversionOperator
            && NormalizeInferredType(sourceClass.Definition.Members
                    .OfType<OperatorMember>()
                    .First(member => member.Operator.OperatorToken.Kind == TokenKind.CType
                        && member.Operator.Parameters.Length == 1)
                    .Operator.ReturnType).GetType() == targetType.GetType())
        {
            EmitExpression(expr.Expression);
            _il.Emit(OpCodes.Call, conversionOperator);
            var resultClr = DataTypeMapper.Map(targetType, _types, _classTypes);
            if (resultClr.IsValueType) _il.Emit(OpCodes.Box, resultClr);
            return;
        }

        EmitExpression(expr.Expression);
        var valClr = DataTypeMapper.Map(sourceType, _types, _classTypes);
        if (valClr.IsValueType) _il.Emit(OpCodes.Box, valClr);
        _il.Emit(OpCodes.Ldstr, targetType.ToString() ?? "DOUBLE");
        _il.Emit(OpCodes.Call, IlRuntimeSafeCastMethod);
    }

    private static bool SameLinqKeyAsOutput(Expr key, Expr output)
    {
        if (key is VariableExpr kv && output is VariableExpr ov)
            return kv.SymbolId == ov.SymbolId || kv.NameCleaned.Lexeme == ov.NameCleaned.Lexeme;
        return key.ToString() == output.ToString();
    }

    private void EmitArrayLengthCall(BuiltinFunctionCallExpr expr)
    {
        // Count() su un array (DIM x = []): emette la lunghezza dell'array.
        EmitExpression(expr.Args[0]);
        var arrClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Args[0])), _types, _classTypes);
        var lenProp = arrClr.GetProperty("Length")?.GetGetMethod()
            ?? throw new NotSupportedException("Backend IL: proprietà Length non trovata.");
        _il.Emit(OpCodes.Callvirt, lenProp);
        _il.Emit(OpCodes.Conv_R8);
    }

    private void EmitDynamicCountCall(BuiltinFunctionCallExpr expr)
    {
        // Count() su un oggetto dinamico (Tree/Bag/Cache/... boxed): CollectionCount runtime.
        EmitExpression(expr.Args[0]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionCount), typeof(object)));
    }

    /// <summary>Call a metodo su collection boxed con argomenti: CollectionInvoke(istanza,
    /// nome, object[] args) — istanza = expr.Args[0], args = expr.Args[1..].</summary>
    private void EmitObjectArgCollectionInvokeCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        EmitExpression(expr.Args[0]);
        _il.Emit(OpCodes.Ldstr, methodName);
        _il.Emit(OpCodes.Ldc_I4, expr.Args.Length - 1);
        _il.Emit(OpCodes.Newarr, _types.Object);
        for (var i = 1; i < expr.Args.Length; i++)
        {
            _il.Emit(OpCodes.Dup);
            _il.Emit(OpCodes.Ldc_I4, i - 1);
            EmitExpression(expr.Args[i]);
            var aClr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Args[i])), _types, _classTypes);
            if (aClr.IsValueType) _il.Emit(OpCodes.Box, aClr);
            _il.Emit(OpCodes.Stelem_Ref);
        }
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionInvoke), typeof(object), typeof(string), typeof(object[])));
    }

    private void EmitNativeCollectionCountCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 1 || !TryGetNativeCollectionType(InferType(expr.Args[0]), out var collectionType))
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede una collection nativa.");
        // ATTENZIONE: emit UNA sola volta l'argomento. Prima l'EmitExpression era qui, comune,
        // e il ramo ClosedOverTypeBuilder lo RIEMETTEVA → due copie della collezione sullo
        // stack e solo una consumata da CollectionCount(object): una resta al `ret` → IL
        // non verificabile (InvalidProgramException in StampaMembri di
        // ClassesSimpleInheritance: `PRINT ...; SELF.Animali.Count()`).
        // ARRAY: Count() = Length (double[] non ha la property Count — la VM COUNT su
        // ICollection usa Count che per un array è Length).
        if (collectionType.IsArray)
        {
            EmitExpression(expr.Args[0]);
            _il.Emit(OpCodes.Ldlen);
            _il.Emit(OpCodes.Conv_R8);
            return;
        }
        if (ClosedOverTypeBuilder(collectionType))
        {
            // List<Udt> / Graph<Udt> (elementi TypeBuilder non finalizzati): il GetProperty
            // lancia o produce IL non verificabile → dispatch runtime CollectionCount(object).
            EmitExpression(expr.Args[0]);
            if (collectionType.IsValueType) _il.Emit(OpCodes.Box, collectionType);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionCount), typeof(object)));
            return;
        }
        EmitExpression(expr.Args[0]);
        var getter = GetCollectionCountGetter(collectionType);
        _il.Emit(OpCodes.Callvirt, getter);
        _il.Emit(OpCodes.Conv_R8);
    }

    private void EmitNativeCollectionToArrayCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 1) throw new NotSupportedException("Backend IL: TOARRAY richiede una collection.");
        // Strutture avanzate boxed (RedBlack/Tree/AVL/Bag/... materializzate come object/
        // SortedDictionary): Enumerable.ToArray<ElementType> è INVALIDO (il dict non è
        // IEnumerable<ElementType>, es. IEnumerable<bool> su un SortedDictionary<int,bool>)
        // → dispatch runtime CollectionInvoke("ToArray") che ritorna object[] di coppie.
        var argType = NormalizeInferredType(InferType(expr.Args[0]));
        if (argType is RedBlackTreeType or TreeType or AVLTreeType or BinaryIndexedTreeType
            or TrieType or BPlusTreeType or GraphType or HashRingType or SparseSetType
            or BinaryHeapType or BagType or CacheType or TimelineType or RingBufferType)
        {
            EmitExpression(expr.Args[0]);
            _il.Emit(OpCodes.Ldstr, "ToArray");
            _il.Emit(OpCodes.Ldc_I4_0);
            _il.Emit(OpCodes.Newarr, _types.Object);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionInvoke), typeof(object), typeof(string), typeof(object[])));
            return;
        }
        var elementType = CollectionElementType(InferType(expr.Args[0]));
        var elementClr = DataTypeMapper.Map(elementType, _types, _classTypes);
        EmitExpression(expr.Args[0]);
        _il.Emit(OpCodes.Call, EnumerableToArrayMethod.MakeGenericMethod(elementClr));
    }

    private void EmitNativeCollectionToListCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 1) throw new NotSupportedException("Backend IL: TOLIST richiede una collection.");
        var elementType = CollectionElementType(InferType(expr.Args[0]));
        var elementClr = DataTypeMapper.Map(elementType, _types, _classTypes);
        EmitExpression(expr.Args[0]);
        _il.Emit(OpCodes.Call, EnumerableToListMethod.MakeGenericMethod(elementClr));
    }

    private void EmitToDictCall(BuiltinFunctionCallExpr expr)
    {
        // ToDict(dict): copia in un nuovo Dictionary (come il VM) — `DIM c = d.ToDict()`.
        if (expr.Args.Length != 1)
            throw new NotSupportedException("Backend IL: TODICT richiede un DICT.");
        EmitExpression(expr.Args[0]);
        _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.ToDict), typeof(object)));
    }

    private void EmitNativeDictionaryKeysCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException("Backend IL: KEYS richiede un DICT o SORTED nativo.");
        var dictType = InferType(expr.Args[0]);
        // Keys() su oggetti dinamici (Tree/Bag/Cache/... boxed): estrae le chiavi dal tipo
        // reale via CollectionInvoke("Keys") → array.
        if (NormalizeInferredType(dictType) is TreeType or AVLTreeType or RedBlackTreeType
            or BinaryIndexedTreeType or TrieType or BPlusTreeType or GraphType or HashRingType
            or SparseSetType or BinaryHeapType or BagType or CacheType or TimelineType or RingBufferType)
        {
            EmitExpression(expr.Args[0]);
            _il.Emit(OpCodes.Ldstr, "Keys");
            _il.Emit(OpCodes.Ldc_I4_0);
            _il.Emit(OpCodes.Newarr, _types.Object);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.CollectionInvoke), typeof(object), typeof(string), typeof(object[])));
            return;
        }
        // KEYS() su un valore JSON/AnyType (JSON.KEYS(DatiJson) — il JSON parsed è un
        // Dictionary<string,object?> boxed): dispatch runtime JsonKeys (come la VM).
        if (NormalizeInferredType(dictType) is Basic.JsonType or AnyType)
        {
            EmitExpression(expr.Args[0]);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.JsonKeys), typeof(object)));
            return;
        }
        // Fallback RUNTIME per qualunque altro tipo statico (es. il Linker che inferisce
        // DoubleType per JSON.KEYS — vedi InferGenericPolymorphicReturnType Caso 1): il
        // valore reale è boxed → JsonKeys (IDictionary → chiavi; altrimenti null come la VM).
        if (dictType is not DictType and not SortedDictType)
        {
            EmitExpression(expr.Args[0]);
            var dClr = DataTypeMapper.Map(dictType, _types, _classTypes);
            if (dClr.IsValueType) _il.Emit(OpCodes.Box, dClr);
            _il.Emit(OpCodes.Call, RuntimeMethod(nameof(IlRuntime.JsonKeys), typeof(object)));
            return;
        }
        var keyType = dictType switch
        {
            DictType dictionary => dictionary.KeyType,
            SortedDictType dictionary => dictionary.KeyType,
            _ => throw new NotSupportedException("Backend IL: KEYS richiede un DICT o SORTED nativo.")
        };
        var concreteType = DataTypeMapper.Map(dictType, _types, _classTypes);
        var keyClr = DataTypeMapper.Map(keyType, _types, _classTypes);
        var getter = concreteType.GetProperty("Keys")?.GetGetMethod()
            ?? throw new NotSupportedException("Backend IL: proprietà Keys non disponibile.");
        EmitExpression(expr.Args[0]);
        _il.Emit(OpCodes.Callvirt, getter);
        _il.Emit(OpCodes.Call, EnumerableToArrayMethod.MakeGenericMethod(keyClr));
    }

    private void EmitPiCall(BuiltinFunctionCallExpr expr)
    {
        if (expr.Args.Length != 0)
            throw new NotSupportedException("Backend IL: PI non accetta argomenti.");
        _il.Emit(OpCodes.Call, IlRuntimePiMethod);
    }

    private void EmitDoubleBuiltinCall(BuiltinFunctionCallExpr expr, MethodInfo method)
    {
        if (expr.Args.Length != 1)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede esattamente 1 argomento.");
        var sourceType = NormalizeInferredType(InferType(expr.Args[0]));
        if (sourceType is BitType)
        {
            // BIT/BOOL → numero con la convenzione VMBASIC TRUE = -1.
            // Il valore sullo stack NON è sempre un bool CLR: una VARIABILE BIT vive in un
            // local int32 (GetOrDeclareLocal) e vale già -1/0 (o il numero assegnato, es. 5).
            // Solo letterali/confronti/chiamate BIT sono bool 0/1 da moltiplicare per -1:
            // senza la distinzione `STR$(b)` stampava " 1" su IL contro "-1" delle gambe C.
            EmitExpression(expr.Args[0]);
            _il.Emit(OpCodes.Conv_R8);
            if (EmitsClrBoolValue(expr.Args[0]))
            {
                _il.Emit(OpCodes.Ldc_R8, -1.0);
                _il.Emit(OpCodes.Mul);
            }
        }
        else
        {
            EmitExpression(expr.Args[0]);
            var sourceClr = DataTypeMapper.Map(sourceType, _types, _classTypes);
            IlConversion.EmitConvert(_il, _types, sourceType, sourceClr, _types.Double);
        }
        _il.Emit(OpCodes.Call, method);
    }

    /// <summary>Chiamata numerica variadica (0..N argomenti double → double): usata dalle
    /// funzioni SPECIAL (BETA 2 arg, BETAINC 3 arg, BESSELJ 2 arg, ...) che EmitDoubleBuiltinCall
    /// non può coprire (richiede esattamente 1 argomento).</summary>
    private void EmitNumericCall(BuiltinFunctionCallExpr expr, MethodInfo method)
    {
        foreach (var arg in expr.Args)
        {
            EmitExpression(arg);
            var st = NormalizeInferredType(InferType(arg));
            var sc = DataTypeMapper.Map(st, _types, _classTypes);
            IlConversion.EmitConvert(_il, _types, st, sc, _types.Double);
        }
        _il.Emit(OpCodes.Call, method);
    }

    private static readonly MethodInfo IlRuntimeRoundSafeMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.RoundSafe))!;
    private static readonly MethodInfo IlRuntimeTruncSafeMethod = typeof(IlRuntime).GetMethod(nameof(IlRuntime.TruncSafe))!;

    private void EmitDecimalSafeCall(BuiltinFunctionCallExpr expr, MethodInfo method)
    {
        if (expr.Args.Length is < 1 or > 2)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede uno o due argomenti.");
        // Un builtin BitType (es. TP.ISLONGONLY) è dichiarato bool nel tipo BASIC ma il
        // metodo IlRuntime restituisce già double (-1/0): la conversione BitType→double
        // normalizzava il valore a 1 prima di ROUND/TRUNC. Per le chiamate builtin usa
        // direttamente il double emesso; per variabili BIT resta la conversione normale.
        if (expr.Args[0] is BuiltinFunctionCallExpr bitCall && InferType(bitCall) is BitType)
            EmitExpression(bitCall);
        else
            EmitDoubleArgument(expr.Args[0]);
        if (expr.Args.Length == 2) EmitInt32Arg(expr.Args[1]);
        else _il.Emit(OpCodes.Ldc_I4_0);
        _il.Emit(OpCodes.Call, method);
    }

    /// <summary>Builtin DBF con 0 o 1 argomento (alias opzionale): emette l'argomento (o
    /// null) e chiama il metodo runtime Dbf* (porting GMBasic/GMBasicAI).</summary>
    private void EmitDbfAliasCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length > 1)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 0 o 1 argomento (alias).");
        if (expr.Args.Length == 1)
        {
            // Work-area alias: un identificatore nudo si riferisce all'ALIAS per NOME
            // (pattern ISAM.SELECTED(Clienti) / DBF.RECNO(alias)) — NON al valore di una
            // variabile (che sarebbe 0 per gli alias non dichiarati). Un'espressione
            // diversa (numero/stringa) passa il valore.
            if (expr.Args[0] is VariableExpr aliasRef)
                _il.Emit(OpCodes.Ldstr, aliasRef.NameCleaned.Lexeme);
            else EmitStringArg(expr.Args[0]);
        }
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName, typeof(string)));
    }

    /// <summary>Nome tipo per la definizione campi di DBF CREATE ... FROM TYPE (formato Nome=Tipo=Len).</summary>
    private void EmitDbfAliasCallNum(BuiltinFunctionCallExpr expr, string methodName)
    {
        EmitDbfAliasCall(expr, methodName);
        _il.Emit(OpCodes.Conv_R8);
    }

    /// <summary>RDD RLOCK(area)/FLOCK(area) come FUNZIONE (ritorna bool). Il runtime
    /// RddRLock/RddFLock esiste già; qui si emette l'area (boxed object, o null per
    /// l'area corrente) e si converte il bool a double (1/0) come da convenzione.</summary>
    private void EmitRddLockCall(BuiltinFunctionCallExpr expr, string methodName)
    {
        if (expr.Args.Length > 1)
            throw new NotSupportedException($"Backend IL: {expr.OriginalToken.Lexeme} richiede 0 o 1 argomento (area).");
        if (expr.Args.Length == 1)
        {
            EmitExpression(expr.Args[0]);
            var clr = DataTypeMapper.Map(NormalizeInferredType(InferType(expr.Args[0])), _types, _classTypes);
            if (clr.IsValueType) _il.Emit(OpCodes.Box, clr);
        }
        else _il.Emit(OpCodes.Ldnull);
        _il.Emit(OpCodes.Call, RuntimeMethod(methodName, typeof(object)));
        _il.Emit(OpCodes.Conv_I4);
        _il.Emit(OpCodes.Conv_R8);
    }

    /// <summary>Emette un argomento boxato a object (per i runtime RDD che accettano object?).</summary>
    private void EmitBoxedArg(Expr e)
    {
        EmitExpression(e);
        var c = DataTypeMapper.Map(NormalizeInferredType(InferType(e)), _types, _classTypes);
        if (c.IsValueType) _il.Emit(OpCodes.Box, c);
    }

    /// <summary>Chiamata che il linker NON ha risolto (es. builtin DBF/SCREEN/POINT
    /// risolti per nome dal P-Code): dispatch builtin col TokenKind del nome (porting
    /// GMBasic — EmitUnresolvedCall).</summary>
    private void EmitUnresolvedCall(UnresolvedCallExpr uce)
    {
        // Diagnostico DISTINTO per esito (mai un NotSupported generico): simbolo non trovato /
        // ambigua / namespace non importato. Stesso messaggio del bytecode-compiler.
        if (uce.Name.Kind is TokenKind.Identifier)
            throw new InvalidOperationException(UnresolvedCallDiagnostics.Build(uce));
        EmitBuiltinFunctionCall(new BuiltinFunctionCallExpr(
            new Token(TokenKind.Identifier, "", null, uce.Name.Position), uce.Name, uce.Arguments, uce.ReturnType));
    }

    /// <summary>USE <file> [ALIAS a] — DBF_USE → IlRuntime.DbfUse (delega a IDatabaseService,
    /// l'handle è tracciato per alias nel runtime).</summary>
}
