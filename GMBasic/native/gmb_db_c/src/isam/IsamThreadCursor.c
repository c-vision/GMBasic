/* FILE: native/gmb_db_c/src/ISAM/IsamThreadCursor.c */
/* Port 1:1 di modules/Database/ISAM/IsamThreadCursor.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.IsamThreadCursor
//
//using Basic.ISAM;
//
//namespace Basic.ISAM;
//
///// <summary>
///// Stato navigazione ISAM per thread (cursore record, EOF/BOF, versione letta per MVCC, filtro).
///// </summary>
//internal sealed class IsamThreadCursor
//{
//    public long CurrentRecordOffset { get; set; } = -1;
//
//    public bool EofFlag { get; set; }
//
//    public bool BofFlag { get; set; }
//
//    public long LastReadRecordVersion { get; set; } = -1;
//
//    public Expr? ActiveFilterExpression { get; set; }
//
//    public Engine.IsamExpressionEvaluator? ActiveFilterEvaluator { get; set; }
//
//    /// <summary>Chiave seek quando SET FILTER abilita pushdown su indice attivo.</summary>
//    internal byte[]? FilterPushdownSeekKey { get; set; }
//
//    /// <summary>Definizione indice usata dal pushdown filtro.</summary>
//    internal Config.IndexDefinition? FilterPushdownIndexDef { get; set; }
//
//    /// <summary>Byte da confrontare sulla chiave (-1 = chiave serializzata intera).</summary>
//    internal int FilterPushdownMatchLength { get; set; } = -1;
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

void IsamThreadCursor_Init(IsamThreadCursor* c)
{
    if (!c) return;
    memset(c, 0, sizeof(*c));
    c->CurrentRecordOffset = -1;
    c->LastReadRecordVersion = -1;
    c->FilterPushdownMatchLength = -1;
}

void IsamThreadCursor_Dispose(IsamThreadCursor* c)
{
    if (!c) return;
    free(c->FilterPushdownSeekKey);
    c->FilterPushdownSeekKey = NULL;
    c->FilterPushdownSeekKeyLen = 0;
}

int IsamThreadCursor_SelfCheck(void)
{
    IsamThreadCursor c;
    IsamThreadCursor_Init(&c);
    if (c.CurrentRecordOffset != -1 || c.LastReadRecordVersion != -1) return -270;
    if (c.EofFlag || c.BofFlag) return -271;
    if (c.FilterPushdownMatchLength != -1) return -272;
    c.FilterPushdownSeekKey = (uint8_t*)malloc(4);
    if (!c.FilterPushdownSeekKey) return -273;
    c.FilterPushdownSeekKeyLen = 4;
    IsamThreadCursor_Dispose(&c);
    if (c.FilterPushdownSeekKey) return -274;
    return GMB_DB_OK;
}

