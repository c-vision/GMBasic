// FILE: IlBackendRuntime/PCode/PCodeProcessor.Builtins.Helpers.cs
// PARTIAL — dominio BUILTIN/HELPERS: ArgOr, ToDoubleArray, MinCvar/Cdar, portfolio.
// Migrato da PCodeProcessor.Builtins.cs (2026-08-30 — split).

using System;
using System.Collections;
using System.Collections.Generic;
using System.Linq;
using Basic.Runtime;
using Basic.Runtime.Gpm;
using Compiler;
using Basic.IlBackend.PCode.Http;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
{
    private static double ArgOr(object?[] args, int idx, double def = 0) => idx < args.Length ? SafeDouble(args[idx]) : def;
    private static double[] ToDoubleArray(object? val) {
        // Flatten ricorsivo (come il ToDoubleArr di GM-Basic): un System.Array multidimensionale
        // (es. object[,] della FFT) implementa IList, ma list[i] lancia "Array was not a
        // one-dimensional array" (crash AnalysisTests — IFFT(FftRes)). Appiattisce invece
        // qualunque IEnumerable (row-major), esattamente come il repo gemello.
        if (val is double[] da2) return da2;
        var flat2 = new System.Collections.Generic.List<double>();
        void Flatten(object? item)
        {
            if (item is double[] dad2) { foreach (var d in dad2) flat2.Add(d); return; }
            if (item is System.Collections.IEnumerable en2 && item is not string && item is not System.Collections.IDictionary)
            {
                foreach (var sub in en2) Flatten(sub);
                return;
            }
            flat2.Add(SafeDouble(item ?? 0));
        }
        Flatten(val);
        return flat2.ToArray();
    }

    private static double[,]? ToDoubleMatrix2D(object? value)
    {
        if (value is not System.Collections.IList outer || outer.Count == 0) return null;
        int rows = outer.Count, cols = 0;
        foreach (var r in outer) if (r is System.Collections.IList rr) cols = Math.Max(cols, rr.Count);
        if (cols == 0) return null;
        var m = new double[rows, cols];
        for (int i = 0; i < rows; i++)
            if (outer[i] is System.Collections.IList rr2)
                for (int j = 0; j < rr2.Count; j++) m[i, j] = SafeDouble(rr2[j]);
        return m;
    }

    private static object? OptimizePortfolioToDict(CommonLib.Trading.PortfolioResult r) =>
        new System.Collections.Generic.Dictionary<string, object?>
        {
            ["ExpectedReturn"] = r.Return,
            ["Volatility"] = r.StDev,
            ["SharpeRatio"] = r.SharpeRatio,
            ["Weights"] = r.Weights
        };

    // MINCVAR: media delle perdite oltre il quantile (CVaR / Expected Shortfall). Porting da VM/C.
    private static double MinCvar(double[] r, double conf)
    {
        if (r.Length == 0) return 0;
        var sorted = r.OrderBy(x => x).ToArray();
        int tail = Math.Max(1, (int)((1 - conf) * r.Length));
        double sum = 0;
        for (int i = 0; i < tail; i++) sum += sorted[i];
        return -sum / tail;
    }

    // MINCDAR: CVaR sul drawdown cumulato. Porting da VM/C.
    private static double MinCdar(double[] r, double conf)
    {
        if (r.Length == 0) return 0;
        var dd = new double[r.Length];
        double cum = 0, peak = 0;
        for (int i = 0; i < r.Length; i++) { cum += r[i]; if (cum > peak) peak = cum; dd[i] = peak - cum; }
        var sorted = dd.OrderBy(x => x).ToArray();
        int tail = Math.Max(1, (int)((1 - conf) * r.Length));
        double sum = 0;
        for (int i = r.Length - tail; i < r.Length; i++) sum += sorted[i];
        return tail > 0 ? sum / tail : 0;
    }

    // Converte una matrice (zigzag IList oppure System.Array multidimensionale Rank=2)
    // in una double[,]. Restituisce null se il valore NON è una matrice 2D.
    // ---- MEMORY namespace: store key-value persistente di processo (SQLite in-memory) ----
    private static Basic.AgentMemoryStore? _memoryStore;
    private static Basic.AgentMemoryStore MemoryStore()
    {
        if (_memoryStore is null)
        {
            var conn = new Microsoft.Data.Sqlite.SqliteConnection("Data Source=:memory:");
            conn.Open();
            _memoryStore = new Basic.AgentMemoryStore(conn);
        }
        return _memoryStore;
    }

    // ---- TEAM/WORKFLOW namespace: registry di processo (popolato dai comandi TEAM/WORKFLOW) ----
}
