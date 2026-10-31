/* FILE: native/gmb_db_c/src/DBF/DbfThreadCursor.c */
/* Port 1:1 di modules/Database/DBF/DbfThreadCursor.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.DBF.DbfThreadCursor
//
//namespace Basic.DBF;
//
///// <summary>
///// Stato navigazione DBF per thread (cursore record, BOF/EOF/Found, filtro, posizione indice).
///// </summary>
//internal sealed class DbfThreadCursor
//{
//    public int CurrentRecordNumber { get; set; }
//
//    public uint CurrentLeafPageNum { get; set; }
//
//    public int CurrentIndexInLeaf { get; set; }
//
//    public long LastReadRecordVersion { get; set; } = -1;
//
//    public bool Eof { get; set; }
//
//    public bool Bof { get; set; }
//
//    public bool Found { get; set; }
//
//    public Expr? ActiveFilterExpression { get; set; }
//
//    public Func<Dictionary<string, object?>, bool>? ActiveFilterEvaluator { get; set; }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

/* C#: internal sealed class DbfThreadCursor */
typedef struct DbfThreadCursor {
    int CurrentRecordNumber;
    unsigned int CurrentLeafPageNum;
    int CurrentIndexInLeaf;
    int64_t LastReadRecordVersion;
    int Eof;
    int Bof;
    int Found;
    void* ActiveFilterExpression;
    void* ActiveFilterEvaluator;
} DbfThreadCursor;

void DbfThreadCursor_Init(DbfThreadCursor* c)
{
    memset(c, 0, sizeof(*c));
    c->LastReadRecordVersion = -1;
}

