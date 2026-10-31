/* FILE: native/gmb_db_c/src/SPEED/SpeedDurabilityMode.c */
/* Port 1:1 di modules/Database/SPEED/SpeedDurabilityMode.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Speed.SpeedDurabilityMode
//
//using Basic.Databases.Speed;
//
//namespace Basic.SPEED;
//
///// <summary>
///// Controlla quando i file WAL SPEED vengono sincronizzati su disco.
///// </summary>
//public enum SpeedDurabilityMode
//{
//    /// <summary>
//    /// Ogni commit esegue fsync su WAL (comportamento legacy).
//    /// </summary>
//    Full,
//
//    /// <summary>
//    /// Scrive su disco senza fsync ad ogni operazione; usare <see cref="Database.FlushDurability"/> o dispose.
//    /// </summary>
//    Buffered,
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

/* C#: public enum SpeedDurabilityMode */
typedef enum SpeedDurabilityMode {
    /* Ogni commit esegue fsync su WAL (comportamento legacy). */
    SpeedDurabilityMode_Full = 0,
    /* Scrive su disco senza fsync ad ogni operazione; usare Database.FlushDurability o dispose. */
    SpeedDurabilityMode_Buffered = 1
} SpeedDurabilityMode;

