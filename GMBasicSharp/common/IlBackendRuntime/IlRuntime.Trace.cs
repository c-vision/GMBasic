// FILE: IlRuntime.Trace.cs
//
// STACK TRACE BASIC della gamba IL — parità con C99Native/VM-C/PCode.
//
// Sulle gambe native la catena si legge dai frame della VM. Qui le procedure BASIC sono
// metodi CLR `Proc_<symbolId>`, ma lo StackTrace .NET NON è utilizzabile come fonte: ILOpt
// fa INLINING dei metodi piccoli, quindi un errore sollevato in `C` chiamata da `B` può
// riportare il solo frame del chiamante. Serve quindi una catena esplicita, tenuta dal
// runtime e aggiornata dall'emitter:
//
//   TraceEnter(symbolId)      prologo della procedura → ritorna la profondità precedente
//   TraceLine(symbolId, riga) ogni statement          → riga corrente del frame in cima
//   TraceLeaveTo(depth)       ogni uscita normale     → toglie il frame della procedura
//   TraceSnapshot()           all'ingresso di un CATCH → fotografia PRIMA di scartare i
//                             frame interni (i loro TraceLeaveTo non sono passati: il
//                             longjmp non esiste, l'eccezione .NET ha scavalcato i ret)
//   TraceRestoreTo(depth)     subito dopo lo snapshot  → riporta la catena al frame corrente
//
// `ErrTrace()` restituisce la fotografia: è ciò che finisce in `ERR.StackTrace` /
// `e.StackTrace`, con lo stesso testo delle altre gambe:
//
//     in C (riga 3)
//     in B (riga 7)
//     in Main (riga 11)

using System.Text;

namespace Basic.IlBackend;

public static partial class IlRuntime
{
    private const int TraceMax = 64;

    [ThreadStatic] private static (int SymbolId, int Line)[]? _traceStack;
    [ThreadStatic] private static int _traceN;
    [ThreadStatic] private static string? _traceSnapshot;
    /// <summary>Frame di un GENERATORE da anteporre alla catena. Il corpo del generatore gira
    /// su un ALTRO thread (con la sua catena): quando l'errore viene ri-lanciato dal
    /// consumatore, i suoi frame vanno preposti a quelli del chiamante — è ciò che sulle gambe
    /// native accade da sé, perché la catena è condivisa tra corpo e chiamante.</summary>
    [ThreadStatic] private static string? _tracePrefix;

    /// <summary>Entra nella procedura: pusha il frame e ritorna la profondità PRECEDENTE
    /// (da passare a <see cref="TraceLeaveTo"/> / <see cref="TraceRestoreTo"/>).</summary>
    public static int TraceEnter(int symbolId)
    {
        _traceStack ??= new (int, int)[TraceMax];
        var prev = _traceN;
        if (_traceN < _traceStack.Length)
        {
            _traceStack[_traceN] = (symbolId, 0);
            _traceN++;
        }
        return prev;
    }

    /// <summary>Aggiorna la riga corrente del frame in cima (chiamata a ogni statement emesso).</summary>
    public static void TraceLine(int symbolId, int line)
    {
        if (line <= 0 || _traceN <= 0 || _traceStack is null) return;
        _traceStack[_traceN - 1] = (_traceStack[_traceN - 1].SymbolId, line);
    }

    /// <summary>Uscita NORMALE della procedura: toglie il frame (e quelli rimasti appesi).</summary>
    public static void TraceLeaveTo(int depth)
    {
        if (depth >= 0 && depth < _traceN) _traceN = depth;
    }

    /// <summary>Ripresa dopo un errore: tiene il frame della procedura che ha catturato.</summary>
    public static void TraceRestoreTo(int depth)
    {
        if (depth >= 0 && depth + 1 < _traceN) _traceN = depth + 1;
    }

    /// <summary>Fotografia della catena: va presa all'ingresso del CATCH, prima di scartare i
    /// frame delle procedure attraversate dall'errore.</summary>
    public static void TraceSnapshot()
    {
        _traceSnapshot = BuildTraceText();
        _tracePrefix = null;   /* consumato: la fotografia lo contiene già */
    }

    /// <summary>Antepone alla prossima fotografia i frame di un generatore il cui errore sta
    /// per essere ri-lanciato su questo thread (vedi IlGeneratorInstance.Next).</summary>
    internal static void SetTracePrefix(string? prefix)
        => _tracePrefix = string.IsNullOrEmpty(prefix) ? null : prefix;

    /// <summary>Catena fotografata all'ultimo errore ("" se nessuno).</summary>
    public static string ErrTrace() => _traceSnapshot ?? string.Empty;

    /// <summary>Azzera lo stato della catena (fine programma in un processo CONDIVISO). Senza,
    /// i frame e la fotografia del programma precedente restano vivi e un nuovo programma li
    /// eredita (nello sweep per-file la catena mostrava procedure di ALTRI file).</summary>
    public static void TraceReset()
    {
        _traceN = 0;
        _traceStack = null;
        _traceSnapshot = null;
        _tracePrefix = null;
    }

    /// <summary>Catena corrente come testo (usata per il messaggio fatale: nessun CATCH ha
    /// ancora scartato i frame).</summary>
    public static string TraceText() => BuildTraceText();

    /// <summary>Nome BASIC della procedura registrata con questo SymbolId ("" se ignota).</summary>
    private static string TraceProcName(int symbolId)
    {
        foreach (var kv in _basicProcedures)
            if (kv.Value.SymbolId == symbolId) return kv.Key;
        return string.Empty;
    }

    private static string BuildTraceText()
    {
        if (_tracePrefix is null && (_traceN <= 0 || _traceStack is null)) return string.Empty;
        var sb = new StringBuilder();
        if (_tracePrefix is not null) sb.Append(_tracePrefix);
        for (var i = _traceN - 1; i >= 0; i--)
        {
            var (sym, line) = _traceStack[i];
            var name = TraceProcName(sym);
            if (name.Length == 0) continue;
            sb.Append("  in ").Append(name).Append(" (riga ").Append(line).Append(")\n");
        }
        return sb.ToString();
    }
}
