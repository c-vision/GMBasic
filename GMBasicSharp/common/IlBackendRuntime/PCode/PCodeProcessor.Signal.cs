// PCodeProcessor.Signal.cs — SIGNAL (namespace SIGNAL) lato PCodeVM.
//
// Segnali con FILTRO STRUTTURATO su bus locale in-process; l'handler è una SUB del
// programma, dispatchata con DispatchEventHandler (task GPM corrente). Il filtro replica
// esattamente gmb_signal.c / IlRuntime.Signal.cs: AND dei filtri specificati, CSV
// case-insensitive per TYPES/SYMBOLS, uguaglianza case-insensitive per ASSET/MARKET/TIMEFRAME.
using System;
using System.Collections.Generic;
using Basic.Runtime.Gpm;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    private sealed class PSignal
    {
        public string Id = "", Types = "", Asset = "", Market = "", Symbols = "", Timeframe = "";
        public string Handler = "";
    }

    private readonly Dictionary<string, PSignal> _sigSub = new(StringComparer.OrdinalIgnoreCase);

    /// <summary>SIGNAL SUBSCRIBE: stack (top) = timeframe, symbols, market, asset, types, id.</summary>
    private void HandleSignalSubscribe(PCodeTask task)
    {
        var timeframe = PopString(task) ?? "";
        var symbols = PopString(task) ?? "";
        var market = PopString(task) ?? "";
        var asset = PopString(task) ?? "";
        var types = PopString(task) ?? "";
        var id = PopString(task) ?? "";
        if (id.Length == 0) return;
        _sigSub[id] = new PSignal
        {
            Id = id, Types = types, Asset = asset, Market = market, Symbols = symbols, Timeframe = timeframe,
            Handler = _sigSub.TryGetValue(id, out var old) ? old.Handler : "",
        };
    }

    /// <summary>ON SIGNAL id CALL handler: stack (top) = handler, id.</summary>
    private void HandleOnSignal(PCodeTask task)
    {
        var id = PopString(task) ?? "";
        var handler = PopString(task) ?? "";
        if (id.Length == 0) return;
        if (!_sigSub.TryGetValue(id, out var s)) return;
        s.Handler = handler;
    }

    /// <summary>SIGNAL EMIT: stack (top) = timeframe, market, asset, value, symbol, type.</summary>
    private void HandleSignalEmit(PCodeTask task)
    {
        var timeframe = PopString(task) ?? "";
        var market = PopString(task) ?? "";
        var asset = PopString(task) ?? "";
        var value = PopNum(task);
        var symbol = PopString(task) ?? "";
        var type = PopString(task) ?? "";
        foreach (var kv in new List<KeyValuePair<string, PSignal>>(_sigSub))
        {
            var s = kv.Value;
            if (s.Handler.Length == 0) continue;
            if (!CsvHas(s.Types, type)) continue;
            if (!CsvHas(s.Symbols, symbol)) continue;
            if (!EqOrAny(s.Asset, asset)) continue;
            if (!EqOrAny(s.Market, market)) continue;
            if (!EqOrAny(s.Timeframe, timeframe)) continue;
            var arity = _procedureAddresses.TryGetValue(s.Handler, out var addr)
                        && _procedureArities.TryGetValue(addr, out var ar) ? ar : 4;
            var args = arity >= 5
                ? new object?[] { s.Id, type, symbol, value, timeframe }
                : new object?[] { s.Id, type, symbol, value };
            DispatchEventHandler(task, s.Handler, args);
        }
    }

    private void HandleSignalUnsubscribe(PCodeTask task)
    {
        var id = PopString(task) ?? "";
        if (id.Length == 0) _sigSub.Clear(); else _sigSub.Remove(id);
    }

    private static bool EqOrAny(string filter, string? value)
        => string.IsNullOrEmpty(filter) || string.Equals(filter, value ?? "", StringComparison.OrdinalIgnoreCase);

    private static bool CsvHas(string csv, string? value)
    {
        if (string.IsNullOrEmpty(csv)) return true;
        foreach (var raw in csv.Split(','))
            if (string.Equals(raw.Trim(), value ?? "", StringComparison.OrdinalIgnoreCase)) return true;
        return false;
    }
}
