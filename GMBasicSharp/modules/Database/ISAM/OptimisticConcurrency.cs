// FILE: Basic.ISAM.OptimisticConcurrency.cs

using System.Runtime.InteropServices;

// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace
// ReSharper disable RedundantNameQualifier

namespace Basic.ISAM;

/// <summary>
/// Rappresenta i metadati prepesi a ogni versione di un record di dati utente.
/// Questa struttura è fondamentale per gestire la concorrenza (sia ottimistica che multi-versione)
/// e lo stato del record (attivo/cancellato). Il suo layout è fisso per la persistenza su disco.
/// </summary>
[StructLayout(LayoutKind.Sequential, Pack = 1)]
public struct RecordMetadata
{
    /// <summary>
    /// 1 byte - Stato del record (Attivo o Cancellato). Utilizza le costanti `RecordStatusActive` (0x20)
    /// e `RecordStatusDeleted` (0x2A) definite in `Basic.ISAM.Constants.cs`.
    /// </summary>
    public byte Status;

    /// <summary>
    /// 8 byte - Versione del record. Questo valore viene usato per la concorrenza ottimistica
    /// per rilevare i conflitti di scrittura ("lost update problem"). Tipicamente è un timestamp (Ticks UTC).
    /// </summary>
    public long Version;

    /// <summary>
    /// 8 byte - L'identificatore univoco della transazione che ha creato questa versione del record.
    /// Questo campo è al centro della logica di visibilità del MVCC. Un valore di 0 indica che il record
    /// è stato creato o modificato in modalità auto-commit (o prima dell'introduzione del versioning).
    /// </summary>
    public long TransactionId;

    /// <summary>
    /// 8 byte - Puntatore all'offset fisico della versione precedente di questo record.
    /// Un valore di -1 indica che non ci sono versioni precedenti (è la versione originale o la prima in una catena).
    /// Questo crea una catena di "undo" che permette alle transazioni di trovare le versioni dei record a loro visibili.
    /// </summary>
    public long PreviousVersionOffset;
}
