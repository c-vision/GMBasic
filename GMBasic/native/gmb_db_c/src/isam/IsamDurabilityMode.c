/* FILE: native/gmb_db_c/src/ISAM/IsamDurabilityMode.c */
/* Port 1:1 di modules/Database/ISAM/IsamDurabilityMode.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.IsamDurabilityMode
//
//using Basic.ISAM;
//
//namespace Basic.ISAM;
//
///// <summary>
///// Controlla quando header e stream vengono sincronizzati su disco.
///// </summary>
//public enum IsamDurabilityMode
//{
//    /// <summary>
//    /// Ogni <see cref="IsamFileHandle.Commit"/> esegue fsync (comportamento legacy).
//    /// </summary>
//    Full,
//
//    /// <summary>
//    /// Scrive header e dati su disco senza fsync ad ogni commit; usare <see cref="IsamFileHandle.FlushDurability"/> o dispose.
//    /// </summary>
//    Buffered,
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

/* C#: public enum IsamDurabilityMode */
typedef enum IsamDurabilityMode {
    /* Ogni IsamFileHandle.Commit esegue fsync (comportamento legacy). */
    IsamDurabilityMode_Full = 0,
    /* Scrive header e dati su disco senza fsync ad ogni commit; usare FlushDurability o dispose. */
    IsamDurabilityMode_Buffered = 1
} IsamDurabilityMode;

