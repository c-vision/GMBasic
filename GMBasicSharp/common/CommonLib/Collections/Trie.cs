// FILE: Interpreter.Gpm.Collections.Trie.Core.cs
// ReSharper disable CheckNamespace

/* 
 * Trie.cs: albero di prefissi (Trie) per ricerca e inserimento efficiente di stringhe
 * 
 * FUNZIONI:
 * - Add(word): aggiunge stringa
 * - Contains(word): verifica presenza
 * - Remove(word): rimuove stringa
 * - StartsWith(prefix): verifica prefisso
 * - GetAllWords(): tutte le stringhe
 * - FindAllWordsWithPrefix(prefix): tutte le parole con prefisso
 * - Clear(): svuota trie
 */

namespace CommonLib.Collections;

/// <summary>
/// Implementazione "stato dell'arte" di una struttura dati Trie (Albero di Prefissi).
///
/// Teoria e Algoritmi:
/// Un Trie è una struttura dati ad albero specializzata per la memorizzazione e la ricerca efficiente di un
/// insieme dinamico di stringhe. A differenza di un albero binario, la posizione di un nodo nell'albero
/// definisce la chiave a cui è associato. Ogni percorso dalla radice a un nodo rappresenta un prefisso comune
/// a tutte le stringhe che discendono da quel nodo. Un nodo può essere marcato come "fine di parola" per
/// indicare che il percorso dalla radice a quel nodo forma una stringa completa presente nel set.
/// Le operazioni principali sono implementate come segue:
/// - Inserimento (`Add`): Si attraversa l'albero dalla radice, un carattere alla volta. Se un nodo per un
///   carattere lungo il percorso non esiste, viene creato. L'ultimo nodo corrispondente all'ultimo carattere
///   della stringa viene marcato come `IsEndOfWord`.
/// - Ricerca (`Contains`): Si attraversa l'albero seguendo i caratteri della stringa cercata. La stringa
///   è considerata presente solo se il percorso completo esiste e il nodo finale è marcato come `IsEndOfWord`.
/// - Ricerca per Prefisso (`StartsWith`): Simile alla ricerca, ma si ferma all'ultimo carattere del prefisso.
///   È sufficiente che il percorso esista.
///
/// Performance:
/// Il vantaggio principale di un Trie è la sua eccezionale performance temporale per le operazioni di ricerca.
/// La complessità temporale per l'inserimento, la cancellazione e la ricerca di una stringa è O(k), dove 'k' è
/// la lunghezza della stringa. Questo è asintoticamente più veloce di un albero binario di ricerca bilanciato (O(k * log n))
/// o di una tabella hash (O(k) in media, ma con overhead per il calcolo dell'hash). Il Trie eccelle in applicazioni
/// come i dizionari, il completamento automatico e l'analisi lessicale. La complessità spaziale può essere
/// significativa, dipendendo dal numero di nodi, che è legato ai prefissi condivisi tra le stringhe.
/// </summary>
public class Trie
{
    private sealed class TrieNode
    {
        public readonly Dictionary<char, TrieNode> Children = new();
        public bool IsEndOfWord;
    }

    private readonly TrieNode _root;
    private int _count;

    /// <summary>
    /// Inizializza una nuova istanza vuota della classe <see cref="Trie"/>.
    /// </summary>
    public Trie()
    {
        _root = new TrieNode();
    }

    /// <summary>
    /// Ottiene il numero di stringhe uniche contenute nel Trie.
    /// </summary>
    public int Count => _count;

    /// <summary>Parole nell'ordine di INSERIMENTO (autorità per FOR EACH/ToArray, come la
    /// lista companion del runtime C: VM/C e C99Native iterano in quest'ordine). L'ordine
    /// alfabetico/DFS resta quello di <see cref="GetAllWords"/> (FindAll/StartsWith).</summary>
    private readonly List<string> _insertion = new();
    public IReadOnlyList<string> Words => _insertion;

    /// <summary>
    /// Aggiunge una stringa al Trie. Se la stringa è già presente, il metodo non ha alcun effetto.
    /// L'operazione attraversa l'albero carattere per carattere, creando nuovi nodi se necessario,
    /// e infine marca il nodo finale come "fine di parola".
    /// </summary>
    /// <param name="word">La stringa da aggiungere. Se nulla o vuota, il metodo non esegue alcuna azione.</param>
    /// <returns><c>true</c> se la stringa è stata aggiunta con successo; <c>false</c> se era già presente.</returns>
    public bool Add(string word)
    {
        if (string.IsNullOrEmpty(word))
        {
            return false;
        }

        var current = _root;
        foreach (var ch in word)
        {
            if (!current.Children.TryGetValue(ch, out var child))
            {
                child = new TrieNode();
                current.Children[ch] = child;
            }
            current = child;
        }

        if (current.IsEndOfWord)
        {
            return false; // La parola esisteva già
        }

        current.IsEndOfWord = true;
        _count++;
        _insertion.Add(word);
        return true;
    }

    /// <summary>
    /// Determina se una specifica stringa è contenuta nel Trie.
    /// Una stringa è considerata contenuta solo se il percorso completo dei suoi caratteri esiste
    /// e il nodo finale è esplicitamente marcato come la fine di una parola.
    /// </summary>
    /// <param name="word">La stringa da cercare.</param>
    /// <returns><c>true</c> se la stringa è presente nel Trie; altrimenti, <c>false</c>.</returns>
    public bool Contains(string word)
    {
        if (string.IsNullOrEmpty(word))
        {
            return false;
        }

        var current = _root;
        foreach (var ch in word)
        {
            if (!current.Children.TryGetValue(ch, out var child))
            {
                return false;
            }
            current = child;
        }
        return current.IsEndOfWord;
    }

    /// <summary>
    /// Rimuove una stringa dal Trie.
    /// Per rimuovere una stringa, viene prima verificata la sua esistenza. Se esiste, il flag `IsEndOfWord`
    /// del nodo finale viene impostato su `false`. Successivamente, l'algoritmo risale il percorso dal nodo
    /// finale verso la radice, eliminando i nodi che non sono più necessari (cioè, nodi che non sono
    /// la fine di un'altra parola e non hanno altri figli).
    /// </summary>
    /// <param name="word">La stringa da rimuovere.</param>
    /// <returns><c>true</c> se la stringa è stata trovata e rimossa con successo; altrimenti, <c>false</c>.</returns>
    public bool Remove(string word)
    {
        if (string.IsNullOrEmpty(word) || !Contains(word))
        {
            return false;
        }

        /* BUG REALE (2026-11-26): il valore di ritorno era quello di RemoveRecursive, cioè
         * "il nodo radice può essere eliminato" → FALSE per ogni parola tranne quando il
         * trie resta VUOTO (con "car","cat","dog", Remove("dog") rendeva false pur avendo
         * rimosso la parola). Ora il predicato dice se la parola esisteva (Contains sopra)
         * e la lista delle parole (Words/ToArray/JSON) viene aggiornata. */
        RemoveRecursive(_root, word, 0);
        _insertion.Remove(word);
        return true;
    }

    /// <summary>
    /// Esegue la logica ricorsiva per la rimozione di una stringa.
    /// </summary>
    private bool RemoveRecursive(TrieNode current, string word, int index)
    {
        if (index == word.Length)
        {
            if (!current.IsEndOfWord)
            {
                return false; // La parola non esiste.
            }

            current.IsEndOfWord = false;
            _count--;
            // Ritorna true se il nodo corrente può essere cancellato (non ha figli).
            return current.Children.Count == 0;
        }

        var ch = word[index];
        if (!current.Children.TryGetValue(ch, out var child))
        {
            return false; // La parola non esiste.
        }

        var shouldDeleteChild = RemoveRecursive(child, word, index + 1);

        if (shouldDeleteChild)
        {
            current.Children.Remove(ch);
            // Ritorna true se anche il nodo corrente può essere cancellato
            // (non è la fine di un'altra parola e non ha altri figli).
            return current is { IsEndOfWord: false, Children.Count: 0 };
        }

        return false;
    }

    /// <summary>
    /// Determina se esiste almeno una stringa nel Trie che inizia con il prefisso specificato.
    /// </summary>
    /// <param name="prefix">Il prefisso da cercare.</param>
    /// <returns><c>true</c> se il prefisso esiste; altrimenti, <c>false</c>.</returns>
    public bool StartsWith(string prefix)
    {
        if (string.IsNullOrEmpty(prefix))
        {
            return true; // Un prefisso vuoto è condiviso da tutte le stringhe.
        }

        var current = _root;
        foreach (var ch in prefix)
        {
            if (!current.Children.TryGetValue(ch, out var child))
            {
                return false;
            }
            current = child;
        }
        return true;
    }
    
    /// <summary>
    /// Restituisce un enumeratore che itera attraverso tutte le stringhe contenute nel Trie.
    /// L'ordine non è garantito.
    /// </summary>
    /// <returns>Una collezione enumerabile di stringhe.</returns>
    public IEnumerable<string> GetAllWords()
    {
        var words = new List<string>();
        FindAllWordsRecursive(_root, "", words);
        return words;
    }

    /// <summary>
    /// Metodo helper ricorsivo per trovare tutte le parole a partire da un nodo.
    /// </summary>
    private void FindAllWordsRecursive(TrieNode node, string prefix, List<string> words)
    {
        if (node.IsEndOfWord)
        {
            words.Add(prefix);
        }

        foreach (var (ch, childNode) in node.Children)
        {
            FindAllWordsRecursive(childNode, prefix + ch, words);
        }
    }
    
    // Aggiungere i seguenti metodi alla classe Trie
    /// <summary>
    /// Trova tutte le stringhe nel Trie che iniziano con il prefisso specificato.
    /// L'operazione ha una complessità di O(p + k), dove 'p' è la lunghezza del prefisso e 'k'
    /// è il numero totale di caratteri in tutte le parole trovate.
    /// </summary>
    /// <param name="prefix">Il prefisso da cercare.</param>
    /// <returns>Una collezione enumerabile contenente tutte le parole che iniziano con il prefisso.</returns>
    public IEnumerable<string> FindAllWordsWithPrefix(string prefix)
    {
        var current = _root;
        foreach (var ch in prefix)
        {
            if (!current.Children.TryGetValue(ch, out var child))
            {
                return [];
            }
            current = child;
        }

        var words = new List<string>();
        FindAllWordsRecursive(current, prefix, words);
        return words;
    }
    
    /// <summary>
    /// Rimuove tutte le stringhe dal Trie. L'operazione ha una complessità temporale di O(1).
    /// </summary>
    public void Clear()
    {
        _insertion.Clear();
        _root.Children.Clear();
        _count = 0;
    }
}
