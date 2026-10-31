// FILE: Basic.DBF.DbfThreadCursor

namespace Basic.DBF;

/// <summary>
/// Stato navigazione DBF per thread (cursore record, BOF/EOF/Found, filtro, posizione indice).
/// </summary>
internal sealed class DbfThreadCursor
{
    public int CurrentRecordNumber { get; set; }

    public uint CurrentLeafPageNum { get; set; }

    public int CurrentIndexInLeaf { get; set; }

    public long LastReadRecordVersion { get; set; } = -1;

    public bool Eof { get; set; }

    public bool Bof { get; set; }

    public bool Found { get; set; }

    public Expr? ActiveFilterExpression { get; set; }

    public Func<Dictionary<string, object?>, bool>? ActiveFilterEvaluator { get; set; }
}
