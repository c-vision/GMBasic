// FILE: Basic.ISAM.IsamThreadCursor

using Basic.ISAM;

namespace Basic.ISAM;

/// <summary>
/// Stato navigazione ISAM per thread (cursore record, EOF/BOF, versione letta per MVCC, filtro).
/// </summary>
internal sealed class IsamThreadCursor
{
    public long CurrentRecordOffset { get; set; } = -1;

    public bool EofFlag { get; set; }

    public bool BofFlag { get; set; }

    public long LastReadRecordVersion { get; set; } = -1;

    public Expr? ActiveFilterExpression { get; set; }

    public Engine.IsamExpressionEvaluator? ActiveFilterEvaluator { get; set; }

    /// <summary>Chiave seek quando SET FILTER abilita pushdown su indice attivo.</summary>
    internal byte[]? FilterPushdownSeekKey { get; set; }

    /// <summary>Definizione indice usata dal pushdown filtro.</summary>
    internal Config.IndexDefinition? FilterPushdownIndexDef { get; set; }

    /// <summary>Byte da confrontare sulla chiave (-1 = chiave serializzata intera).</summary>
    internal int FilterPushdownMatchLength { get; set; } = -1;
}
