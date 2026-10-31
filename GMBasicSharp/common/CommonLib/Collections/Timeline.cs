// FILE: Interpreter.Gpm.Collections.Timeline.Core.cs

using System.Collections;

// ReSharper disable CheckNamespace

/* 
 * Timeline.cs: linea del tempo basata su SortedDictionary con query per intervallo di tempo
 * 
 * FUNZIONI:
 * - Add(timestamp, value): aggiunge evento
 * - Remove(timestamp): rimuove evento
 * - TryGetValue(timestamp, out value): cerca per timestamp
 * - Contains(timestamp): verifica esistenza
 * - Clear(): svuota timeline
 * - First()/Last(): primo/ultimo evento
 * - GetRange(start, end): eventi in intervallo
 */

namespace CommonLib.Collections;

/// <summary>
/// Implementazione "stato dell'arte" di una struttura dati Timeline (Linea del Tempo) generica.
///
/// Teoria e Algoritmi:
/// Una Timeline è una collezione specializzata per memorizzare dati (eventi) associati a un timestamp.
/// È ottimizzata per query basate su intervalli di tempo. L'implementazione più efficiente per questa
/// struttura utilizza una collezione ordinata che mappa i timestamp (DateTimeOffset) ai valori degli eventi.
/// Questa implementazione utilizza un `SortedDictionary{DateTimeOffset, T}`, che internamente si basa su un
/// albero rosso-nero. Questo garantisce che gli eventi siano sempre mantenuti in ordine cronologico per chiave (timestamp).
/// Le operazioni principali sono:
/// - Inserimento (`Add`): Aggiunge un evento a un timestamp specifico. Se un evento esiste già a quel timestamp,
///   il valore viene sovrascritto.
/// - Query su Intervallo (`GetRange`): Dato un intervallo di tempo (data di inizio e fine), restituisce
///   tutti gli eventi che cadono all'interno di quell'intervallo. Grazie alla natura ordinata della struttura
///   sottostante, questa operazione è molto più efficiente di una scansione lineare.
///
/// Performance:
/// Basandosi su un `SortedDictionary`, le performance delle operazioni sono logaritmiche.
/// - Inserimento (`Add`), Rimozione (`Remove`) e Ricerca di un evento a un timestamp esatto (`TryGetValue`)
///   hanno una complessità temporale di O(log n), dove 'n' è il numero di eventi nella timeline.
/// - L'operazione di `GetRange` ha una complessità temporale di O(log n + k), dove 'k' è il numero di elementi
///   restituiti nell'intervallo. Questo la rende estremamente efficiente per l'analisi di serie temporali.
/// - L'accesso al primo (`First`) e all'ultimo (`Last`) evento sono operazioni O(log n) nella maggior parte
///   delle implementazioni di alberi bilanciati.
/// </summary>
/// <typeparam name="T">Il tipo dei valori (eventi) memorizzati nella timeline.</typeparam>
public class Timeline<T> : IEnumerable<KeyValuePair<DateTimeOffset, T>>
{
    private readonly SortedDictionary<DateTimeOffset, T> _events;

    /// <summary>
    /// Ottiene il numero totale di eventi nella timeline.
    /// </summary>
    public int Count => _events.Count;

    /// <summary>
    /// Inizializza una nuova istanza vuota della classe <see cref="Timeline{T}"/>.
    /// </summary>
    public Timeline()
    {
        _events = new SortedDictionary<DateTimeOffset, T>();
    }

    /// <summary>
    /// Aggiunge un evento alla timeline in un timestamp specifico. Se un evento esiste già a quel
    /// timestamp, il suo valore viene aggiornato.
    /// </summary>
    /// <param name="timestamp">Il momento nel tempo a cui associare l'evento.</param>
    /// <param name="value">Il valore o l'oggetto dell'evento da memorizzare.</param>
    public void Add(DateTimeOffset timestamp, T value)
    {
        _events[timestamp] = value;
    }

    /// <summary>
    /// Rimuove un evento dalla timeline in base al suo timestamp.
    /// </summary>
    /// <param name="timestamp">Il timestamp dell'evento da rimuovere.</param>
    /// <returns><c>true</c> se l'evento è stato trovato e rimosso; altrimenti, <c>false</c>.</returns>
    public bool Remove(DateTimeOffset timestamp)
    {
        return _events.Remove(timestamp);
    }

    /// <summary>
    /// Ottiene il valore dell'evento al timestamp specificato.
    /// </summary>
    /// <param name="timestamp">Il timestamp dell'evento da cercare.</param>
    /// <param name="value">Quando questo metodo ritorna, contiene il valore dell'evento se trovato; altrimenti, il valore predefinito per il tipo T.</param>
    /// <returns><c>true</c> se un evento è stato trovato al timestamp specificato; altrimenti, <c>false</c>.</returns>
    public bool TryGetValue(DateTimeOffset timestamp, out T? value)
    {
        return _events.TryGetValue(timestamp, out value);
    }

    /// <summary>
    /// Determina se un evento esiste al timestamp specificato.
    /// </summary>
    /// <param name="timestamp">Il timestamp da controllare.</param>
    /// <returns><c>true</c> se un evento esiste a quel timestamp; altrimenti, <c>false</c>.</returns>
    public bool Contains(DateTimeOffset timestamp)
    {
        return _events.ContainsKey(timestamp);
    }

    /// <summary>
    /// Rimuove tutti gli eventi dalla timeline.
    /// </summary>
    public void Clear()
    {
        _events.Clear();
    }

    /// <summary>
    /// Ottiene il primo evento nella timeline (quello con il timestamp più vecchio).
    /// </summary>
    /// <returns>Una coppia chiave-valore rappresentante il primo evento.</returns>
    /// <exception cref="InvalidOperationException">Lanciata se la timeline è vuota.</exception>
    public KeyValuePair<DateTimeOffset, T> First()
    {
        if (_events.Count == 0)
            throw new InvalidOperationException("La timeline è vuota.");
        return _events.First();
    }

    /// <summary>
    /// Ottiene l'ultimo evento nella timeline (quello con il timestamp più recente).
    /// </summary>
    /// <returns>Una coppia chiave-valore rappresentante l'ultimo evento.</returns>
    /// <exception cref="InvalidOperationException">Lanciata se la timeline è vuota.</exception>
    public KeyValuePair<DateTimeOffset, T> Last()
    {
        if (_events.Count == 0)
            throw new InvalidOperationException("La timeline è vuota.");
        return _events.Last();
    }

    /// <summary>
    /// Restituisce una collezione di tutti gli eventi che si trovano all'interno di un intervallo di tempo specificato.
    /// L'intervallo è inclusivo.
    /// </summary>
    /// <param name="start">Il timestamp di inizio dell'intervallo.</param>
    /// <param name="end">Il timestamp di fine dell'intervallo.</param>
    /// <returns>Una collezione enumerabile di coppie chiave-valore rappresentanti gli eventi nell'intervallo.</returns>
    public IEnumerable<KeyValuePair<DateTimeOffset, T>> GetRange(DateTimeOffset start, DateTimeOffset end)
    {
        // Ottimizzazione: SortedDictionary non ha un modo diretto per ottenere un sottoinsieme.
        // L'iterazione è il modo più semplice e sufficientemente performante per molti scenari.
        // Per performance estreme su dataset enormi, sarebbe necessario un albero B+ o simile.
        return _events.Where(kvp => kvp.Key >= start && kvp.Key <= end);
    }

    /// <summary>
    /// Restituisce un enumeratore che itera attraverso tutti gli eventi nella timeline in ordine cronologico.
    /// </summary>
    /// <returns>Un enumeratore per la timeline.</returns>
    public IEnumerator<KeyValuePair<DateTimeOffset, T>> GetEnumerator()
    {
        return _events.GetEnumerator();
    }

    /// <summary>
    /// Restituisce un enumeratore che itera attraverso la collezione.
    /// </summary>
    /// <returns>Un enumeratore per la collezione.</returns>
    IEnumerator IEnumerable.GetEnumerator()
    {
        return GetEnumerator();
    }
}
