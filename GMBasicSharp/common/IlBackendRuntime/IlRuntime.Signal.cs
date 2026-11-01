// FILE: IlRuntime.Signal.cs — namespace SIGNAL lato runtime IL.
//
// Segnali con FILTRO STRUTTURATO su un bus locale in-process (in live i segnali arrivano
// dal signal oracle via i transport remoti di MESSAGE). Nessun thread esterno: EMIT gira
// nel task GPM chiamante e l'handler è dispatchato con DispatchHandlerAsGpmOrDirect
// (stesso contratto di MESSAGE, §0ter). Il filtro è identico a quello della leg C
// (gmb_signal.c): AND dei filtri specificati, CSV case-insensitive per TYPES/SYMBOLS.
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Reflection;

namespace Basic.IlBackend;

public static partial class IlRuntime
{
    private sealed class SSub
    {
        public string Id = "";
        public string Types = "";
        public string Asset = "";
        public string Market = "";
        public string Symbols = "";
        public string Timeframe = "";
        public int HandlerSymbolId = -1;
        public MethodInfo? Handler;
    }

    private static readonly Dictionary<string, SSub> _ssub = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>SIGNAL SUBSCRIBE … AS "id".</summary>
    public static void SignalSubscribe(string id, string types, string asset, string market,
        string symbols, string timeframe)
    {
        if (string.IsNullOrEmpty(id)) return;
        _ssub[id] = new SSub
        {
            Id = id,
            Types = types ?? "",
            Asset = asset ?? "",
            Market = market ?? "",
            Symbols = symbols ?? "",
            Timeframe = timeframe ?? "",
            HandlerSymbolId = _ssub.TryGetValue(id, out var old) ? old.HandlerSymbolId : -1,
            Handler = _ssub.TryGetValue(id, out var old2) ? old2.Handler : null,
        };
    }

    /// <summary>ON SIGNAL "id" CALL &lt;handler&gt;: risolve symbolId/metodo (arità 4 o 5).</summary>
    public static void RegisterSignalHandlerBySymbol(string id, int symbolId)
    {
        if (string.IsNullOrEmpty(id) || !_ssub.TryGetValue(id, out var s)) return;
        s.HandlerSymbolId = symbolId;
        s.Handler = symbolId >= 0 ? FindProcedureMethodIn(Assembly.GetCallingAssembly(), symbolId, null) : null;
    }

    /// <summary>ON SIGNAL "id" STOP: disattiva l'handler mantenendo la sottoscrizione.</summary>
    public static void UnregisterSignalHandler(string id)
    {
        if (string.IsNullOrEmpty(id) || !_ssub.TryGetValue(id, out var s)) return;
        s.HandlerSymbolId = -1;
        s.Handler = null;
    }

    /// <summary>SIGNAL EMIT …: consegna sincrona a chi matcha; ritorna il numero di handler.</summary>
    public static long SignalEmit(string type, string symbol, double value, string asset,
        string market, string timeframe)
    {
        var hits = 0L;
        foreach (var kv in new List<KeyValuePair<string, SSub>>(_ssub))
        {
            var s = kv.Value;
            if (s.HandlerSymbolId < 0 || s.Handler is null) continue;
            if (!CsvHas(s.Types, type)) continue;
            if (!CsvHas(s.Symbols, symbol)) continue;
            if (!EqOrAny(s.Asset, asset)) continue;
            if (!EqOrAny(s.Market, market)) continue;
            if (!EqOrAny(s.Timeframe, timeframe)) continue;
            var npar = s.Handler.GetParameters().Length;
            var args = npar >= 5
                ? new object?[] { s.Id, type ?? "", symbol ?? "", value, timeframe ?? "" }
                : new object?[] { s.Id, type ?? "", symbol ?? "", value };
            DispatchHandlerAsGpmOrDirect(s.HandlerSymbolId, s.Handler, args);
            hits++;
        }
        return hits;
    }

    /// <summary>SIGNAL UNSUBSCRIBE ["id"]: senza id rimuove tutte le sottoscrizioni.</summary>
    public static void SignalUnsubscribe(string id)
    {
        if (string.IsNullOrEmpty(id)) { _ssub.Clear(); return; }
        _ssub.Remove(id);
    }

    /// <summary>SIGNAL PING: keepalive (no-op sul bus locale).</summary>
    public static void SignalPing() { }

    /// <summary>Sottoscrizioni attive (diagnostica).</summary>
    public static long SignalCount() => _ssub.Count;

    private static bool EqOrAny(string filter, string? value)
    {
        if (string.IsNullOrEmpty(filter)) return true;
        return string.Equals(filter, value ?? "", StringComparison.OrdinalIgnoreCase);
    }

    private static bool CsvHas(string csv, string? value)
    {
        if (string.IsNullOrEmpty(csv)) return true;
        foreach (var raw in csv.Split(','))
        {
            if (string.Equals(raw.Trim(), value ?? "", StringComparison.OrdinalIgnoreCase)) return true;
        }
        return false;
    }
}
