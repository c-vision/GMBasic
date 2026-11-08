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
        // Flatten ricorsivo (come il ToDoubleArr di GMBasic): un System.Array multidimensionale
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
