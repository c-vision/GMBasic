// FILE: Basic.IlBackend.DbExprPCodeEvaluator.cs
// Compila ed esegue un'espressione di condizione/chiave database (SET FILTER, SORT TO, JOIN,
// COPY TO/APPEND FROM FOR/WHILE, chiavi SEEK/INDEX ON) con la stessa pipeline unificata usata
// per &/EVAL (Compiler -> P-Code -> PCodeVm interno, vedi IlRuntime.MacroEvalPCode) — piena
// sintassi BASIC (funzioni utente incluse), non una whitelist ristretta come i vecchi
// RuntimeExprEvaluator/RuntimeExpressionEvaluator che sostituisce.
//
// Differenza rispetto alle macro: li' ogni identificatore catturato e' una variabile GLOBALE
// del programma ospite (letta/scritta nel bag di IlRuntime, per SymbolId); qui ogni
// identificatore e' assunto essere un CAMPO del record corrente — il probe e' sempre vero, il
// valore arriva dal contesto per-record passato a Evaluate (un Dictionary), non dal programma.

using System.Collections.Immutable;
using Basic.IlBackend.PCode;
using Basic.Runtime.Gpm;

namespace Basic.IlBackend;

public sealed class DbExprPCodeEvaluator
{
    private static BasicProgram? _program;
    // DefaultNamespaces esplicito (stessa lista di Resources/system.settings.json): senza, il
    // Parser non riconosce nessun builtin namespaced (UPPER$, LEN, VAL, ecc.) in un'espressione
    // di filtro/chiave — li tratta come nodo ponte ArrayAccessExpr, che risolve a NULL silenzioso
    // a runtime invece di un errore. Stesso bug diagnosticato dal vivo per le pagine BSP (vedi
    // SourcePipeline.CompileFromStringWithCompiler/IlRuntime.MinimalMacroProgram, stesso fix).
    private static BasicProgram Program => _program ??= new BasicProgram(new CommonLib.EffectiveSettings
    {
        DefaultNamespaces = ["SYSTEM", "CONSOLE", "GRAPHICS", "MATH", "STRINGS", "DATETIME", "CONVERSIONS", "IO", "CONCURRENCY", "ARRAYS"]
    }, null, "");

    private readonly global::Compiler.CompiledChunk _chunk;
    private readonly IReadOnlyDictionary<string, int> _fieldSlots;
    // Riuso della VM: un filtro database viene valutato per OGNI record (scan di migliaia).
    // Creare un nuovo PCodeVm a ogni Evaluate alloca scheduler+task+processor per record —
    // il C (gmb_expr, tree-walk) non ha questo overhead. Parità: riusiamo la stessa VM e
    // resettiamo il task tra le valutazioni (entry point + globals + stack).
    private readonly object _vmLock = new();
    private PCodeVm? _vm;
    private readonly int _entryPoint;

    private DbExprPCodeEvaluator(global::Compiler.CompiledChunk chunk, IReadOnlyDictionary<string, int> fieldSlots)
    {
        _chunk = chunk;
        _fieldSlots = fieldSlots;
        _entryPoint = chunk.EntryPoint > 0 ? chunk.EntryPoint : 0;
    }

    private static readonly System.Collections.Concurrent.ConcurrentDictionary<string, DbExprPCodeEvaluator?> Cache = new();

    /// <summary>Come <see cref="TryCompile(string?)"/>, ma con cache per testo espressione —
    /// usato dai contesti dove lo STESSO evaluator viene richiesto ripetutamente per la stessa
    /// stringa (es. il callback KeyExpressionEvaluatorCallback di ISAM, invocato per OGNI record
    /// con la stessa espressione chiave): evita di ricompilare da zero a ogni chiamata.</summary>
    public static DbExprPCodeEvaluator? GetOrCompileCached(string? expressionText)
    {
        if (string.IsNullOrWhiteSpace(expressionText)) return null;
        return Cache.GetOrAdd(expressionText, TryCompile);
    }

    /// <summary>Compila l'espressione UNA SOLA VOLTA — l'istanza va riusata per ogni record
    /// (SET FILTER/SORT TO valutano potenzialmente su migliaia di record), mai ricompilata per
    /// singola valutazione.</summary>
    public static DbExprPCodeEvaluator? TryCompile(string? expressionText)
    {
        if (string.IsNullOrWhiteSpace(expressionText)) return null;
        try
        {
            var sourceLine = new SourceLine(-1, "", 0, expressionText);
            var expr = Parser.ParseSingleExpression(expressionText, Program, sourceLine, 1, StdLib.Instance);
            return TryCompile(expr);
        }
        catch (Exception ex)
        {
            // Parità col C (gmb_expr `fail("unknown function")`): l'espressione non compila
            // NON è un no-op muto — va segnalato. Il C# torna null (nessun filtro applicabile),
            // ma logga l'errore perché un SET FILTER rotto non deve passare inosservato.
            System.Console.Error.WriteLine(
                $"[DbExprPCodeEvaluator] filtro/chiave non compilabile: '{expressionText}' — {ex.Message}");
            return null;
        }
    }

    /// <summary>Come <see cref="TryCompile(string?)"/>, ma a partire da un Expr già parsato
    /// altrove (evita un doppio parse quando il chiamante ha già l'AST, es.
    /// RuntimeFilterParser.CreateEvaluator(Expr?)).</summary>
    public static DbExprPCodeEvaluator? TryCompile(Expr expr)
    {
        try
        {
            var compiler = new global::Compiler.Compiler { ExternalGlobalProbe = _ => true };
            var chunk = compiler.CompileExpressionBody(ImmutableArray<Parameter>.Empty, expr);
            chunk.GlobalCount = compiler.GlobalSlots.Count;
            return new DbExprPCodeEvaluator(chunk, compiler.GlobalSlots);
        }
        catch
        {
            return null;
        }
    }

    /// <summary>Valuta l'espressione contro il contesto campi del record corrente. Sola
    /// lettura: nessun write-back dei campi (a differenza delle macro, un filtro non deve poter
    /// modificare il record che sta valutando). Riusa la stessa PCodeVm (reset del task tra
    /// record) — parità di performance col tree-walk C (gmb_expr).</summary>
    public object? Evaluate(IReadOnlyDictionary<string, object?>? ctx)
    {
        try
        {
            var task = GetOrCreateTask();
            // Reset del task per una nuova valutazione: riporta Ip all'entry point, stack e
            // frame puliti, globals riassegnati dai campi del record corrente. Il FRAME va
            // azzerato: OP_RETURN del run precedente lo popola (task.Frame) e senza reset il
            // run successivo riparte con stato spurio → risultato errato.
            var t = task;
            t.Ip = _entryPoint;
            t.State = Basic.Runtime.Gpm.PCodeTaskState.Runnable;
            t.Sp = 0;
            t.InQueue = false;
            t.Frame = null;
            t.IpStack = null;
            t.ErrorHandlerIp = null;
            t.PendingException = null;
            if (ctx != null)
            {
                foreach (var (name, slot) in _fieldSlots)
                {
                    if (TryGetFieldCaseInsensitive(ctx, name, out var value))
                        t.EnsureGlobals(slot + 1);
                }
                foreach (var (name, slot) in _fieldSlots)
                {
                    if (TryGetFieldCaseInsensitive(ctx, name, out var value))
                        t.Globals![slot] = value;
                }
            }
            RunSync();
            return t.PendingException != null ? null : t.PeekTop();
        }
        catch
        {
            return null;
        }
    }

    private PCodeTask GetOrCreateTask()
    {
        lock (_vmLock)
        {
            if (_vm == null)
            {
                _vm = new PCodeVm(_chunk, IlRuntime.Current);
            }
            return _vm.MainTask;
        }
    }

    private void RunSync() => _vm!.RunSync();

    public bool EvaluateBool(IReadOnlyDictionary<string, object?>? ctx) => ToBool(Evaluate(ctx));

    private static bool TryGetFieldCaseInsensitive(IReadOnlyDictionary<string, object?> ctx, string name, out object? value)
    {
        if (ctx.TryGetValue(name, out value)) return true;
        foreach (var kv in ctx)
        {
            if (string.Equals(kv.Key, name, StringComparison.OrdinalIgnoreCase))
            {
                value = kv.Value;
                return true;
            }
        }
        value = null;
        return false;
    }

    private static bool ToBool(object? v) => v switch
    {
        null => false,
        bool b => b,
        double d => d != 0,
        float f => f != 0,
        int i => i != 0,
        long l => l != 0,
        string s => !string.IsNullOrEmpty(s) && s != "0" && !s.Equals("FALSE", StringComparison.OrdinalIgnoreCase),
        _ => true
    };
}
