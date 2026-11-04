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
using CommonLib.Trading;

namespace Basic.IlBackend.PCode;

internal sealed partial class PCodeProcessor
            {
    /// <summary>DICT: delegazione dominio — case estratti da
    /// PCodeProcessor.Builtins.cs (refactor B, de-mescolamento per dominio).</summary>
    private static readonly HashSet<BuiltInFunction> DICTBuiltInFunctions = new HashSet<BuiltInFunction>
            {
        BuiltInFunction.DICT_KEYS,
        BuiltInFunction.DICT_VALUES,
        BuiltInFunction.DICT_ADD,
    };

    /// <summary>DICT: dispatch per BuiltInFunction a TABELLA (Dictionary), non switch (refactor §18).</summary>
    private static readonly Dictionary<BuiltInFunction, Func<PCodeProcessor, PCodeTask, object?[], object?>> DICTDispatch = new()
    {
        [BuiltInFunction.DICT_KEYS] = (self, task, args) =>
        {
            _ = task;
            if (args.Length >= 1 && args[0] is System.Collections.IDictionary dkk) { return dkk.Keys.Cast<object>().ToArray(); }
            // Collection chiave→valore (Cache/RedBlack/BPlusTree, non IDictionary).
            // Ordine CANONICO per nome chiave: Cache usa un ConcurrentDictionary (ordine di
            // hash instabile) mentre C99/VM/C restituiscono le chiavi ordinate.
            if (args.Length >= 1 && args[0] is CommonLib.Collections.IKeyValueIndexable kvi)
                return SortedEntries(kvi).Select(kv => kv.Key).ToArray();
            return null;
        },
        [BuiltInFunction.DICT_VALUES] = (self, task, args) =>
        {
            _ = task;
            if (args.Length >= 1 && args[0] is System.Collections.IDictionary ddv) { return ddv.Values.Cast<object>().ToArray(); }
            // Collection chiave→valore (Cache/RedBlack/BPlusTree).
            if (args.Length >= 1 && args[0] is CommonLib.Collections.IKeyValueIndexable kvi2)
                return SortedEntries(kvi2).Select(kv => kv.Value).ToArray();
            return null;
        },
        [BuiltInFunction.DICT_ADD] = (self, task, args) =>
        {
            _ = task;
            if (args.Length >= 2) {
                if (args[0] is System.Collections.IList al) { al.Add(args[1]); return null; }
                if (args[0] is System.Collections.IDictionary ad) { ad.Add(args[1]!, args.Length > 2 ? args[2] : null); return null; }
            }
            return null;
        },
    };

    /// <summary>Voci di una collezione chiave→valore ordinate per nome chiave (ordine
    /// canonico delle gambe C: Keys/Values deterministici).</summary>
    private static List<KeyValuePair<object?, object?>> SortedEntries(CommonLib.Collections.IKeyValueIndexable kvi)
    {
        var list = kvi.Entries().ToList();
        list.Sort((a, b) => string.CompareOrdinal(
            Convert.ToString(a.Key, System.Globalization.CultureInfo.InvariantCulture),
            Convert.ToString(b.Key, System.Globalization.CultureInfo.InvariantCulture)));
        return list;
    }

    private object? ExecuteBuiltInDict(PCodeTask task, BuiltInFunction fn, object?[] args)
        => DICTDispatch.TryGetValue(fn, out var handler)
            ? handler(this, task, args)
            : throw new InvalidOperationException(
                $"OP_CALL_BUILTIN: funcCode {(int)fn} non gestito da ExecuteBuiltInDict (fn={fn}).");
}
