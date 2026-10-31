// FILE: Basic.Databases.Rdd.SqlResultSet.cs
// Result set SQL materializzato: tutte le righe caricate in memoria a QueryRaw time.
// Consente l'accesso per indice di colonna, il conteggio righe e il cursore FETCH
// indipendente dal DataReader (portabile su qualunque provider).

using System;
using System.Collections.Generic;

namespace Basic.Databases.Rdd;

/// <summary>Snapshot materializzato di un risultato SQL QUERY (portabile).</summary>
public sealed class SqlResultSet
{
    private readonly IReadOnlyList<IReadOnlyDictionary<string, object?>> _rows;
    private int _position = -1;

    public SqlResultSet(IReadOnlyList<string> columns, IReadOnlyList<IReadOnlyDictionary<string, object?>> rows)
    {
        Columns = columns;
        _rows = rows;
    }

    /// <summary>Nomi delle colonne, in ordine (SELECT * → ordine della query).</summary>
    public IReadOnlyList<string> Columns { get; }

    /// <summary>Numero totale di righe (materializzate a QUERY time).</summary>
    public int Count => _rows.Count;

    public int ColumnCount => Columns.Count;

    /// <summary>Avanza il cursore: true se c'è una riga corrente.</summary>
    public bool Fetch()
    {
        if (_position + 1 < _rows.Count) { _position++; return true; }
        return false;
    }

    /// <summary>Riga corrente (null prima del primo FETCH o a fine cursore).</summary>
    public IReadOnlyDictionary<string, object?>? CurrentRow =>
        _position >= 0 && _position < _rows.Count ? _rows[_position] : null;

    /// <summary>Valore della colonna per indice nella riga corrente.</summary>
    public object? CurrentField(int columnIndex)
    {
        var row = CurrentRow;
        if (row is null || columnIndex < 0 || columnIndex >= Columns.Count) return null;
        return row.TryGetValue(Columns[columnIndex], out var v) ? v : null;
    }

    /// <summary>Valore della colonna per nome nella riga corrente.</summary>
    public object? CurrentField(string column)
    {
        var row = CurrentRow;
        return row is not null && row.TryGetValue(column, out var v) ? v : null;
    }

    /// <summary>Riga per indice (senza muovere il cursore) — per DESCRIBE/COL/TOCSV.</summary>
    public IReadOnlyDictionary<string, object?>? RowAt(int row) =>
        row >= 0 && row < _rows.Count ? _rows[row] : null;

    /// <summary>Valore (row,col) per indice (senza cursore).</summary>
    public object? ValueAt(int row, int col)
    {
        var r = RowAt(row);
        if (r is null || col < 0 || col >= Columns.Count) return null;
        return r.TryGetValue(Columns[col], out var v) ? v : null;
    }
}
