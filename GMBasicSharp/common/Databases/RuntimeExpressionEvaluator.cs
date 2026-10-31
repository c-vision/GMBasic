// FILE: Basic.Databases.Rdd.RuntimeExpressionEvaluator.cs
// Valutatore runtime di espressioni di filtro per ISAM SETFILTER/SPEED/scope ISAM.
// Delega interamente alla pipeline unificata Compiler -> P-Code -> PCodeVm interno
// (DbExprPCodeEvaluator, common/IlBackend/) — piena sintassi BASIC, non una whitelist ristretta.

namespace Basic.Databases.Rdd;

/// <summary>Valutatore runtime di espressioni filtro (SET FILTER/SORT TO/JOIN/COPY TO su
/// DBF/ISAM/SPEED, scope ISAM CopyTo/AppendFrom).</summary>
public static class RuntimeExpressionEvaluator
{
    /// <summary>Delegato per valutazione runtime di espressioni filtro.</summary>
    public delegate bool ExpressionEvaluator(string? expr, System.Collections.Generic.IReadOnlyDictionary<string, object?>? ctx);

    /// <summary>
    /// Hook di iniezione verso la pipeline unificata Compiler → P-Code → PCodeVm interno
    /// (DbExprPCodeEvaluator, common/IlBackend/) — questo progetto (Databases) è sotto IlBackend
    /// nel grafo delle dipendenze (IlBackend -> Databases, mai il contrario), quindi non può
    /// referenziare PCodeVm direttamente: serve un punto di iniezione, stesso pattern già usato
    /// per BSP (HttpService.GlobalBspPageHandler). IlBackend imposta questo delegate una volta
    /// (vedi DbFilterHookInit, [ModuleInitializer]) — sempre impostato quando questo backend è in
    /// uso, dato che il ModuleInitializer scatta al primo caricamento dell'assembly IlBackend.
    ///
    /// Non esiste (e non deve esistere) un fallback verso un parser C# ristretto: VM/C e C99 non
    /// possono MAI eseguire codice C# — nessun ponte fra i due mondi, per principio
    /// architetturale (vedi Architecture-Three-Backends.md). Se questo hook non è impostato, la
    /// causa è che l'IL Backend non è ancora caricato in questo processo (non che serva un
    /// fallback più debole): CreateEvaluator ritorna null (nessun filtro) e logga, non inventa
    /// un secondo motore di valutazione da mantenere in parallelo.
    /// </summary>
    public static Func<string, ExpressionEvaluator?>? PCodeEvaluatorFactory { get; set; }

    /// <summary>Crea un evaluator da una stringa di espressione filtro tramite la pipeline
    /// P-Code unificata. Null (nessun filtro, loggato) se l'hook non è impostato o l'espressione
    /// non è compilabile.</summary>
    public static ExpressionEvaluator? CreateEvaluator(string? expression)
    {
        if (string.IsNullOrWhiteSpace(expression))
            return null;

        if (PCodeEvaluatorFactory == null)
        {
            Console.Error.WriteLine(
                $"[RuntimeExpressionEvaluator] PCodeEvaluatorFactory non impostato (IL Backend non caricato in questo processo) — filtro ignorato: '{expression}'");
            return null;
        }

        return PCodeEvaluatorFactory(expression);
    }
}
