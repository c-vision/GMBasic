/* FILE: native/gmb_db_c/src/ISAM/IsamFormatPolicy.c */
/* Port 1:1 di modules/Database/ISAM/IsamFormatPolicy.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.IsamFormatPolicy
//
//using Basic.ISAM;
//
//namespace Basic.ISAM;
//
///// <summary>
///// Policy di versionamento del file <c>.isdb</c> (header <see cref="Config.FileHeader.Version"/>).
///// </summary>
///// <remarks>
///// <para><b>Regola:</b> non incrementare <see cref="CurrentFormatVersion"/> a 2 salvo necessità
///// strutturale (layout header, page size, checksum). Lo schema logico usa un versionamento
///// separato nel blocco ISCH (<c>SchemaFormatVersion</c> in <c>SchemaManager</c>).</para>
///// <para><b>Scrittura:</b> i nuovi file usano sempre <see cref="CurrentFormatVersion"/>.</para>
///// <para><b>Lettura N-1:</b> il motore legge file con
///// <c>MinimumReadableVersion &lt;= Version &lt;= CurrentFormatVersion</c>.
///// Quando <see cref="CurrentFormatVersion"/> passerà a 2, impostare
///// <see cref="MinimumReadableVersion"/> a 1 per mantenere la policy N-1.</para>
///// <para>Migrazioni in-place legacy (pre-ISCH, <c>LobHeapStartOffset</c>) sono gestite da
///// <see cref="IsamOfflineMigrator"/> senza bump di formato file.</para>
///// </remarks>
//public static class IsamFormatPolicy
//{
//    /// <summary>Versione scritta su nuovi database.</summary>
//    public const int CurrentFormatVersion = 1;
//
//    /// <summary>Versione minima del campo header <c>Version</c> ancora apribile in lettura.</summary>
//    public const int MinimumReadableVersion = 1;
//
//    /// <summary>Restituisce <see langword="true"/> se la versione header è supportata in lettura.</summary>
//    public static bool IsReadable(int headerVersion) =>
//        headerVersion >= MinimumReadableVersion && headerVersion <= CurrentFormatVersion;
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */

/* C#: public static class IsamFormatPolicy */
enum {
    /* Versione scritta su nuovi database. */
    IsamFormatPolicy_CurrentFormatVersion = 1,
    /* Versione minima del campo header Version ancora apribile in lettura. */
    IsamFormatPolicy_MinimumReadableVersion = 1
};

/* Restituisce true se la versione header è supportata in lettura. */
int IsamFormatPolicy_IsReadable(int headerVersion)
{
    return headerVersion >= IsamFormatPolicy_MinimumReadableVersion
        && headerVersion <= IsamFormatPolicy_CurrentFormatVersion;
}

