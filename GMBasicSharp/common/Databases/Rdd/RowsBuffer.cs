// FILE: Basic.Databases.Rdd.RowsBuffer.cs
// Table Buffering universale (Clipper/VFP) — funziona con qualsiasi IDatabaseDriver.
// Modalità: 1=None, 2=Pessimistico (lock su edit), 3=Ottimistico (lock su save).
// OLDVAL() / GETFLDSTATE() / CURVAL() per accedere a valori originali e modificati.

using System;
using System.Collections.Generic;

namespace Basic.Databases.Rdd;

/// <summary>Modalità di buffering.</summary>
public enum BufferMode
{
    None = 1,
    Pessimistic = 2,   // lock record al primo edit
    Optimistic = 3,    // lock solo al save, verifica modifiche concorrenti
}

/// <summary>Stato di modifica campo.</summary>
public enum FieldState
{
    Unchanged = 1,
    Modified = 2,
    Appended = 3,
    Deleted = 4,
}

/// <summary>Buffer per un'area di lavoro. Memorizza valori originali e modificati.</summary>
public sealed class RowsBuffer
{
    public BufferMode Mode { get; set; } = BufferMode.None;
    public bool IsActive => Mode != BufferMode.None;

    private Dictionary<int, object?>? _original;
    private Dictionary<int, object?>? _current;
    private Dictionary<int, FieldState>? _states;
    private long _originalRecNo;
    private bool _isAppended;

    /// <summary>Attiva il buffer per il record corrente.</summary>
    public void Activate(IDatabaseDriver driver, BufferMode mode)
    {
        Mode = mode;
        var record = driver.GetCurrentRecord();
        if (record != null)
        {
            _original = new Dictionary<int, object?>(record);
            _current = new Dictionary<int, object?>(record);
            _states = new Dictionary<int, FieldState>();
            _originalRecNo = driver.RecNo;
            _isAppended = false;

            if (mode == BufferMode.Pessimistic)
                driver.Lock(_originalRecNo);
        }
        else
        {
            // Nuovo record (append buffer)
            _original = new Dictionary<int, object?>();
            _current = new Dictionary<int, object?>();
            _states = new Dictionary<int, FieldState>();
            _originalRecNo = 0;
            _isAppended = true;
            foreach (var kv in _current) _states[kv.Key] = FieldState.Appended;
        }
    }

    /// <summary>Scrive un valore nel buffer.</summary>
    public void SetField(int symbolId, object? value)
    {
        if (!IsActive || _current == null || _states == null) return;
        _current[symbolId] = value;
        _states[symbolId] = _isAppended ? FieldState.Appended : FieldState.Modified;
    }

    /// <summary>Legge il valore corrente dal buffer.</summary>
    public object? GetField(int symbolId)
    {
        if (!IsActive || _current == null) return null;
        return _current.TryGetValue(symbolId, out var v) ? v : null;
    }

    /// <summary>Legge il valore originale (prima delle modifiche).</summary>
    public object? GetOldValue(int symbolId)
    {
        if (!IsActive || _original == null) return null;
        return _original.TryGetValue(symbolId, out var v) ? v : null;
    }

    /// <summary>Stato di modifica di un campo.</summary>
    public FieldState GetFieldState(int symbolId)
    {
        if (!IsActive || _states == null) return FieldState.Unchanged;
        return _states.TryGetValue(symbolId, out var s) ? s : FieldState.Unchanged;
    }

    /// <summary>Commit: scrive le modifiche nel driver.</summary>
    public bool TableUpdate(IDatabaseDriver driver)
    {
        if (!IsActive || _current == null) return false;

        if (Mode == BufferMode.Optimistic)
        {
            // Verifica che il record non sia stato modificato da altri
            if (!_isAppended && _originalRecNo > 0)
            {
                // Il lock qui serve solo per leggere il record corrente senza corse durante il
                // controllo di conflitto: va rilasciato PRIMA di proseguire a PutCurrentRecord,
                // che acquisisce il proprio write lock sullo STESSO record tramite
                // Engine.AcquireRecordWriteLock — tenerlo ancora acquisito causava sempre
                // LockRecursionException (ReaderWriterLockSlim non è rientrante sullo stesso
                // thread), facendo fallire OGNI TABLEUPDATE in modalità ottimistica.
                if (!driver.Lock(_originalRecNo)) return false;
                try
                {
                    var current = driver.GetCurrentRecord();
                    if (current != null)
                    {
                        foreach (var kv in _original!)
                        {
                            if (current!.TryGetValue(kv.Key, out var cv) && !Equals(cv, kv.Value))
                                return false; // conflitto: modificato da altri
                        }
                    }
                }
                finally
                {
                    driver.Unlock(_originalRecNo);
                }
            }
        }

        // Il lock pessimistico tenuto da Activate va RILASCIATO prima del put: lo stesso
        // motore acquisisce la propria write-lock su questo record (Engine.AcquireRecordWriteLock)
        // e tenerla già causava LockRecursionException — classe di bug già fissata per il ramo
        // ottimistico (vedi commento sopra); qui mancava il ramo pessimistico: TABLEUPDATE in
        // mode 2 falliva sempre silenziosamente (catch del chiamante → false).
        if (Mode == BufferMode.Pessimistic && _originalRecNo > 0)
        {
            try { driver.Unlock(_originalRecNo); } catch { }
        }

        if (_isAppended)
        {
            driver.Append(_current);
        }
        else if (_originalRecNo > 0)
        {
            // Riposiziona SOLO se il cursore si è davvero mosso: su DbfDriver GoTo è
            // SEEK per CHIAVE (engine.Seek), non numero di record — chiamarlo sempre
            // declassava il cursore e PutCurrentRecord throwava "Cursore non posizionato"
            // (RDD.TABLEUPDATE falliva su aree DBF via RDD USE; su IsamDriver GoTo è
            // per numero, quindi il problema non si vedeva).
            if (driver.RecNo != _originalRecNo) driver.GoTo(_originalRecNo);
            driver.PutCurrentRecord(_current);
        }

        // Deactivate
        Deactivate(driver);
        return true;
    }

    /// <summary>Revert: scarta tutte le modifiche.</summary>
    public void TableRevert(IDatabaseDriver driver)
    {
        if (_isAppended && _originalRecNo > 0)
        {
            driver.Delete();
        }
        Deactivate(driver);
    }

    private void Deactivate(IDatabaseDriver driver)
    {
        // Unlock idempotente: TableUpdate l'ha già rilasciato prima del put.
        if (Mode == BufferMode.Pessimistic && _originalRecNo > 0)
        {
            try { driver.Unlock(_originalRecNo); } catch { }
        }
        Mode = BufferMode.None;
        _original = null;
        _current = null;
        _states = null;
    }
}
