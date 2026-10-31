// FILE: Basic.IlBackend.RuntimeFilterParser.cs
// Parser JIT delle condizioni database a runtime (SET FILTER, FOR/WHILE/WHERE,
// COPY TO, SORT TO, JOIN). Usa il Parser BASIC completo (ParseSingleExpression)
// così le condizioni supportano la piena sintassi BASIC, incluse le macro
// `&(expr)` già espanse a compile-time (il cui valore stringa arriva qui).

using System;
using System.Collections.Generic;
using System.Collections.Immutable;
using CommonLib;

namespace Basic.IlBackend;

/// <summary>Parsa e valuta a runtime le condizioni di filtro database.</summary>
public static class RuntimeFilterParser
{
    private static BasicProgram? _program;

    /// <summary>Programma minimo riusato per il parse JIT delle espressioni.
    /// DefaultNamespaces esplicito (stessa lista di Resources/system.settings.json): senza, il
    /// Parser non riconosce nessun builtin namespaced (UPPER$, LEN, VAL, ecc.) in una condizione
    /// di filtro — li tratta come nodo ponte ArrayAccessExpr, NULL silenzioso a runtime invece
    /// di un errore. Stesso bug diagnosticato dal vivo per le pagine BSP (vedi
    /// SourcePipeline.CompileFromStringWithCompiler/IlRuntime.MinimalMacroProgram/
    /// DbExprPCodeEvaluator, stesso fix).</summary>
    private static BasicProgram Program => _program ??= new BasicProgram(
        new EffectiveSettings
        {
            DefaultNamespaces = ["SYSTEM", "CONSOLE", "GRAPHICS", "MATH", "STRINGS", "DATETIME", "CONVERSIONS", "IO", "CONCURRENCY", "ARRAYS"]
        }, null, "");

    /// <summary>Parsa una condizione stringa in un <see cref="Expr"/> AST (null se vuota o non valida).</summary>
    public static Expr? Parse(string? condition)
    {
        if (string.IsNullOrWhiteSpace(condition)) return null;
        try
        {
            var line = new SourceLine(0, condition, 1, condition);
            return Parser.ParseSingleExpression(condition, Program, line, 1, null);
        }
        catch (Exception ex)
        {
            // Prima: `catch { return null; }` — un errore di sintassi nella stringa condizione
            // (es. SET FILTER TO "espressione scritta male") spariva silenziosamente e
            // CreateEvaluator(string) tratta un Ast null come "nessun filtro" (sempre true) —
            // quindi un typo nella condizione mostrava TUTTI i record invece di fallire in modo
            // rumoroso o di filtrare correttamente. Loggato per essere distinguibile da una
            // condizione genuinamente vuota (vedi Fase 1 del piano dinamico).
            Console.Error.WriteLine(
                $"[RuntimeFilterParser] condizione non parsabile, trattata come 'nessun filtro': " +
                $"'{condition}' — {ex.GetType().Name}: {ex.Message}");
            return null;
        }
    }

    /// <summary>Costruisce l'evaluator per-record da un'Expr (null → nessun filtro, sempre true).
    /// Compila UNA SOLA VOLTA con la pipeline unificata Compiler → P-Code → PCodeVm interno
    /// (stessa di &amp;/EVAL, vedi DbExprPCodeEvaluator) — piena sintassi BASIC (funzioni utente
    /// incluse), non la whitelist ristretta del vecchio RuntimeExprEvaluator che sostituisce.
    /// L'istanza compilata viene riusata per OGNI record dal delegate ritornato (mai
    /// ricompilata per singola valutazione).</summary>
    public static System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? CreateEvaluator(Expr? expr)
    {
        if (expr is null) return null;
        var compiled = DbExprPCodeEvaluator.TryCompile(expr);
        if (compiled is null)
        {
            // Expr già parsata con successo da Parse() — un fallimento di compilazione qui è
            // inatteso (bug), non una condizione vuota: loggato invece di trattarlo in
            // silenzio come "nessun filtro" (che mostrerebbe tutti i record).
            Console.Error.WriteLine($"[RuntimeFilterParser] compilazione P-Code fallita per l'espressione già parsata: '{expr}'");
            return null;
        }
        return compiled.EvaluateBool;
    }

    /// <summary>Costruisce l'evaluator per-record da una condizione stringa (null se non parsabile).</summary>
    public static (Expr? Ast, System.Func<System.Collections.Generic.IReadOnlyDictionary<string, object?>, bool>? Evaluator) CreateEvaluator(string? condition)
    {
        var ast = Parse(condition);
        return (ast, CreateEvaluator(ast));
    }
}
