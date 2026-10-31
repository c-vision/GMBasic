/* FILE: native/gmb_db_c/src/DBF/DbfDurabilityMode.c */
/* Port 1:1 di modules/Database/DBF/DbfDurabilityMode.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.DBF.DbfDurabilityMode
//
//namespace Basic.DBF;
//
///// <summary>
///// Controlla quando header e stream DBF vengono sincronizzati su disco.
///// </summary>
//public enum DbfDurabilityMode
//{
//    /// <summary>
//    /// Ogni auto-commit esegue fsync (comportamento legacy).
//    /// </summary>
//    Full,
//
//    /// <summary>
//    /// Scrive su disco senza fsync ad ogni commit; usare <see cref="DbfEngine.FlushDurability"/> o dispose.
//    /// </summary>
//    Buffered,
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

/* C#: public enum DbfDurabilityMode — dichiarato in gmb_db_port.h */

