// FILE: Basic.Databases.Rdd.DataSession.cs
// DataSession: gestisce le work area (1-255), come XSharp DataSession / Clipper.
// Centralizza SELECT, USE, CLOSE e la risoluzione degli alias.

using System.Collections.Generic;

namespace Basic.Databases.Rdd;

/// <summary>Gestisce il selezionatore di aree di lavoro (stile Clipper SELECT 1, SELECT 2, ...).
/// Fino a 255 aree, indice 0 riservato (mai usato).</summary>
public class DataSession
{
    public const int MaxWorkareas = 255;

    private readonly DatabaseWorkArea[] _areas = new DatabaseWorkArea[MaxWorkareas + 1];
    private readonly Dictionary<string, int> _aliases = new(StringComparer.OrdinalIgnoreCase);
    private int _currentArea;
    // Lock di mutua-esclusione E visibilità di memoria per _areas/_aliases/_currentArea (bug
    // AiWiki SQL-Parquet-CoarsePark-Race-Cross-Test): SQL OPEN/CLOSE (sul thread worker GPM
    // che esegue lo statement) e le scritture pesanti coarse-park (ParkBlocking -> Task.Run,
    // un thread DIVERSO del pool .NET per ogni chiamata) leggono/scrivono questo stato da
    // thread diversi. Dictionary<TKey,TValue>/array NON sono thread-safe: senza un lock (che
    // garantisce anche una barriera di memoria, non solo mutua esclusione), una SCRITTURA
    // (RegisterDirect/Open registra l'alias) su un thread non è garantita visibile a una
    // LETTURA (FindAlias) su un thread diverso — osservato con una traccia dedicata:
    // FindAlias("M") ritornava intermittentemente null PUR ESSENDO "M" già registrata
    // (chiamata pochi istanti prima, nello stesso script), facendo cadere la risoluzione sul
    // fallback FindAnySqlDriver() — che restituisce il driver della PRIMA area occupata, non
    // necessariamente quella giusta — instradando una scrittura (CREATE/INSERT/UPDATE/DELETE)
    // sul driver SBAGLIATO: il SELECT successivo, risolto correttamente dal path alternativo
    // (FindAreaByAlias, scan lineare su _areas[i].Alias, stesso problema di visibilità ma
    // "per fortuna" quasi sempre coerente), non trovava la tabella mai scritta sulla
    // connessione vera ("no such table: <nome>").
    private readonly object _stateLock = new();

    public int CurrentArea
    {
        get { lock (_stateLock) return _currentArea; }
        set { lock (_stateLock) _currentArea = value; }
    }

    /// <summary>Driver predefinito per nuove USE senza VIA (es. "ISAM", "DBF", "SPEED").</summary>
    public string DefaultDriver { get; set; } = "ISAM";

    /// <summary>Gestore lock a livello record.</summary>
    public LockManager Locks { get; } = new();

    /// <summary>Gestore eventi/trigger database.</summary>
    public DbEventManager Events { get; } = new();

    /// <summary>Table buffering per area.</summary>
    internal readonly Dictionary<int, RowsBuffer> _buffers = new();

    public IDatabaseDriver? CurrentDriver
    {
        get
        {
            lock (_stateLock)
                return _currentArea > 0 && _currentArea <= MaxWorkareas ? _areas[_currentArea]?.Driver : null;
        }
    }

    public DataSession()
    {
        for (int i = 0; i <= MaxWorkareas; i++)
            _areas[i] = new DatabaseWorkArea { Area = i };
    }

    /// <summary>Seleziona un'area di lavoro e restituisce il driver (se presente).</summary>
    public IDatabaseDriver? Select(int area)
    {
        lock (_stateLock)
        {
            if (area < 0) area = FindEmptyAreaLocked(first: false);
            if (area == 0) area = FindEmptyAreaLocked(first: true);
            if (area < 1 || area > MaxWorkareas)
                throw new ArgumentOutOfRangeException(nameof(area), $"Area {area} non valida (1-{MaxWorkareas}).");

            _currentArea = area;
            return _currentArea > 0 && _currentArea <= MaxWorkareas ? _areas[_currentArea]?.Driver : null;
        }
    }

    /// <summary>Alloca la prima area libera (1-based).</summary>
    public int AllocateArea() { lock (_stateLock) return FindEmptyAreaLocked(true); }

    /// <summary>Cerca la prima area libera. Se first=true, parte da area 1. Se first=false, parte dopo l'area corrente.</summary>
    public int FindEmptyArea(bool first) { lock (_stateLock) return FindEmptyAreaLocked(first); }

    private int FindEmptyAreaLocked(bool first)
    {
        int start = first ? 1 : _currentArea + 1;
        for (int i = start; i <= MaxWorkareas; i++)
            if (_areas[i].IsEmpty) return i;
        for (int i = 1; i < start; i++)
            if (_areas[i].IsEmpty) return i;
        return 0; // nessuna area libera
    }

    /// <summary>Cerca un driver per alias (case-insensitive).</summary>
    public IDatabaseDriver? FindAlias(string alias)
    {
        lock (_stateLock)
        {
            if (_aliases.TryGetValue(alias, out var area))
                return _areas[area].Driver;
            return null;
        }
    }

    /// <summary>Apre un database in una work area e restituisce il driver.</summary>
    public IDatabaseDriver Open(IDatabaseDriver driver, DbOpenInfo info)
    {
        lock (_stateLock)
        {
            int area = info.Area > 0 ? info.Area : FindEmptyAreaLocked(first: true);
            if (area == 0)
                throw new InvalidOperationException("Nessuna work area disponibile.");

            if (!string.IsNullOrEmpty(info.Alias) && _aliases.TryGetValue(info.Alias, out _))
                throw new InvalidOperationException($"Alias '{info.Alias}' già in uso.");

            driver.Open(info);
            _areas[area].Driver = driver;
            _areas[area].Alias = info.Alias;
            if (!string.IsNullOrEmpty(info.Alias)) _aliases[info.Alias] = area;
            info.Area = area;
            _currentArea = area;
            return driver;
        }
    }

    /// <summary>Restituisce l'oggetto work area per l'area specificata (accesso diretto per VM).</summary>
    public DatabaseWorkArea? GetWorkArea(int area)
    {
        lock (_stateLock)
        {
            if (area < 1 || area > MaxWorkareas) return null;
            return _areas[area];
        }
    }

    /// <summary>Registra direttamente un driver a un'area specifica (per uso interno VM).</summary>
    public void RegisterDirect(int area, IDatabaseDriver driver, string alias)
    {
        lock (_stateLock)
        {
            if (area < 1 || area > MaxWorkareas) return;
            _areas[area].Driver = driver;
            _areas[area].Alias = alias;
            if (!string.IsNullOrEmpty(alias)) _aliases[alias] = area;
            _currentArea = area;
        }
    }

    /// <summary>Cerca l'area number per alias. Restituisce 0 se non trovato.</summary>
    public int FindAreaByAlias(string alias)
    {
        if (string.IsNullOrWhiteSpace(alias)) return 0;
        lock (_stateLock)
        {
            for (int i = 1; i <= MaxWorkareas; i++)
                if (_areas[i].Alias?.Equals(alias, StringComparison.OrdinalIgnoreCase) == true)
                    return i;
            return 0;
        }
    }

    /// <summary>Rimuove un driver da un'area specifica (per uso interno VM).</summary>
    public void Unregister(int area)
    {
        lock (_stateLock)
        {
            if (area < 1 || area > MaxWorkareas) return;
            var alias = _areas[area].Alias;
            _areas[area].Driver?.Close();
            _areas[area].Driver = null;
            _areas[area].Alias = "";
            _areas[area].Meta.Clear();
            if (!string.IsNullOrEmpty(alias)) _aliases.Remove(alias);
            // Chiudendo l'area CORRENTE, CurrentDriver resterebbe null: i comandi area-correnti
            // successivi (es. `DBF AGGREGATE` dopo aver chiuso un'area temporanea) non
            // troverebbero più l'area principale. Torna alla prima area ancora aperta (o 0).
            // Parità con il runtime C (VM-C/C99/C99Native).
            if (_currentArea == area)
            {
                _currentArea = 0;
                for (int i = 1; i <= MaxWorkareas; i++)
                    if (_areas[i].Driver is not null) { _currentArea = i; break; }
            }
        }
    }

    /// <summary>Chiude un database nell'area corrente e libera la work area.</summary>
    public void Close()
    {
        lock (_stateLock)
        {
            if (_currentArea > 0)
            {
                var alias = _areas[_currentArea].Alias;
                _areas[_currentArea].Driver?.Close();
                _areas[_currentArea].Driver = null;
                _areas[_currentArea].Alias = "";
                if (!string.IsNullOrEmpty(alias)) _aliases.Remove(alias);
            }
        }
    }

    /// <summary>Chiude tutti i database aperti.</summary>
    public void CloseAll()
    {
        lock (_stateLock)
        {
            for (int i = 1; i <= MaxWorkareas; i++)
            {
                _areas[i].Driver?.Close();
                _areas[i].Driver = null;
                _areas[i].Alias = "";
            }
            _aliases.Clear();
            _currentArea = 0;
        }
    }
}
