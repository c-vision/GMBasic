/* FILE: native/gmb_db_c/src/SPEED/Config.c */
/* Port 1:1 di modules/Database/SPEED/Config.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.Databases.Speed.Config.cs
//// ReSharper disable InconsistentNaming
//// ReSharper disable CheckNamespace
//
//namespace Basic.Databases.Speed;
//
///// <summary>
///// Contiene le costanti e le enumerazioni per il motore LSM-Tree Speed.
///// </summary>
//public static class Config
//{
//    /// <summary>
//    /// Nome del file manifesto che contiene i metadati del database.
//    /// </summary>
//    public const string ManifestFileName = "manifest.json";
//
//    /// <summary>
//    /// Nome della sottodirectory per i file Write-Ahead Log.
//    /// </summary>
//    public const string WalDirectoryName = "wal";
//
//    /// <summary>
//    /// Nome della sottodirectory per i file SSTable.
//    /// </summary>
//    public const string SstDirectoryName = "tables";
//
//    #region Schema Structures (per il motore Speed)
//
//    /// <summary>
//    /// Rappresenta la definizione logica di un singolo campo all'interno dello schema Speed.
//    /// </summary>
//    [Serializable]
//    public record SpeedFieldSchema(
//        string Name,
//        DataType DataType,
//        int Length // Per le stringhe a lunghezza fissa, altrimenti 0
//    );
//
//    /// <summary>
//    /// Rappresenta la definizione logica e le proprietà di un singolo indice per il motore Speed.
//    /// </summary>
//    [Serializable]
//    public record SpeedIndexDefinition(
//        string Name,
//        string KeyExpression,
//        DataType DataType,
//        int KeyLength,
//        bool IsUnique,
//        bool IsUnsafe
//    );
//
//    /// <summary>
//    /// Rappresenta la definizione logica completa dello schema di una tabella o vista Speed.
//    /// </summary>
//    [Serializable]
//    public record SpeedSchema(
//        string TypeName,
//        List<SpeedFieldSchema> Fields,
//        SpeedIndexDefinition PrimaryKey,
//        List<SpeedIndexDefinition> SecondaryKeys,
//        // NUOVO: Contiene la stringa della query SELECT se questa tabella è una vista.
//        string? ViewDefinitionQuery = null 
//    );
//
//    #endregion
//}
//
///// <summary>
///// Definisce i tipi di operazione che possono essere registrati nel Write-Ahead Log (WAL) del motore Speed.
///// </summary>
//internal enum WalOperationType : byte
//{
//    /// <summary>
//    /// Rappresenta un'operazione di inserimento o aggiornamento di una coppia chiave-valore.
//    /// </summary>
//    Set = 1,
//
//    /// <summary>
//    /// Rappresenta un'operazione di cancellazione di una chiave (scrittura di una "tombstone").
//    /// </summary>
//    Delete = 2,
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

/* C#: public static class Config — ManifestFileName/WalDirectoryName/SstDirectoryName e WalOperationType in gmb_db_port.h */
