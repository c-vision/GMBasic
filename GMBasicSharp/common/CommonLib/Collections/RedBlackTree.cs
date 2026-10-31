// FILE: Interpreter.Gpm.Collections.RedBlackTree.Core.cs
using System.Diagnostics.CodeAnalysis;
// ReSharper disable MemberCanBePrivate.Global
// ReSharper disable CheckNamespace
namespace CommonLib.Collections;
/// <summary>
/// Implementazione "stato dell'arte" di un Albero Rosso-Nero (Red-Black Tree) generico.
///
/// Teoria e Algoritmi:
/// Un albero rosso-nero è un albero binario di ricerca auto-bilanciato che garantisce performance logaritmiche
/// mantenendo un bilanciamento approssimativo. Ogni nodo ha un colore (rosso o nero) e l'albero deve
/// soddisfare le seguenti cinque proprietà in ogni momento:
/// 1. Ogni nodo è rosso o nero.
/// 2. La radice è sempre nera.
/// 3. Ogni foglia (rappresentata da un nodo sentinella NIL) è nera.
/// 4. Se un nodo è rosso, entrambi i suoi figli devono essere neri. (Nessun nodo rosso adiacente).
/// 5. Per ogni nodo, tutti i percorsi semplici da quel nodo alle foglie discendenti contengono lo stesso
///    numero di nodi neri (la "black-height" o "altezza nera").
/// Queste proprietà assicurano che il percorso più lungo dalla radice a una foglia non sia mai più del doppio
/// del percorso più breve, mantenendo l'altezza dell'albero O(log n).
/// Il bilanciamento viene mantenuto dopo inserimenti e cancellazioni attraverso due operazioni fondamentali:
/// la ricolorazione dei nodi e le rotazioni (sinistra e destra). Gli algoritmi di "fixup" dopo un'operazione
/// di modifica sono complessi ma efficienti, e ripristinano le proprietà dell'albero in tempo logaritmico.
/// Questa implementazione utilizza i classici algoritmi di fixup descritti in "Introduction to Algorithms" di Cormen et al.
///
/// Performance:
/// Grazie alle sue proprietà di bilanciamento, un albero rosso-nero garantisce una complessità temporale
/// nel caso peggiore di O(log n) per le operazioni fondamentali: ricerca, inserimento e cancellazione.
/// A differenza degli alberi AVL, gli alberi rosso-neri sono meno strettamente bilanciati, il che può
/// risultare in un'altezza media leggermente maggiore. Tuttavia, richiedono meno rotazioni in media
/// durante le operazioni di inserimento e cancellazione, rendendoli spesso più performanti in scenari
/// con un alto volume di scritture.
/// </summary>
/// <typeparam name="TKey">Il tipo delle chiavi, che deve essere comparabile.</typeparam>
/// <typeparam name="TValue">Il tipo dei valori.</typeparam>
public class RedBlackTree<TKey, TValue> : IKeyValueIndexable where TKey : IComparable<TKey>
{
    /// <summary>
    /// Definisce i due colori possibili per un nodo in un albero Rosso-Nero.
    /// Il colore è la proprietà fondamentale utilizzata dall'algoritmo per mantenere il bilanciamento
    /// dell'albero e garantire che il percorso più lungo dalla radice a una foglia non sia mai più del
    /// doppio del percorso più breve.
    /// </summary>
    private enum NodeColor { Red, Black }

    /// <summary>
    /// Rappresenta un singolo nodo all'interno dell'albero Rosso-Nero. Oltre alla chiave, al valore e ai puntatori
    /// ai figli, questo nodo include un puntatore al genitore e un colore (Rosso o Nero).
    /// Il puntatore al genitore è essenziale per gli algoritmi di "fixup", in quanto permette di risalire l'albero
    /// e di eseguire le rotazioni in modo efficiente senza l'uso della ricorsione o di uno stack esterno.
    /// Il colore è la proprietà centrale utilizzata per applicare le regole di bilanciamento dell'albero Rosso-Nero.
    /// Un nuovo nodo inserito è inizialmente sempre di colore Rosso, per preservare la proprietà dell'altezza nera.
    /// </summary>
    private sealed class Node(TKey key, TValue value)
    {
        public readonly TKey Key = key;
        public TValue Value = value;
        public NodeColor Color;
        public Node? Parent;
        public Node? Left;
        public Node? Right;
    }

    /// <summary>
    /// Rappresenta un nodo "sentinella" unico per l'intero albero, utilizzato per rappresentare tutte le foglie
    /// logiche (NIL). Secondo la definizione formale di un albero Rosso-Nero, tutte le foglie sono nodi neri.
    /// L'uso di un singolo oggetto sentinella invece di puntatori `null` semplifica notevolmente gli algoritmi
    /// di inserimento, cancellazione e rotazione, eliminando la necessità di continui controlli `if (node != null)`.
    /// Questo nodo è sempre di colore Nero e i suoi puntatori (parent, left, right) puntano a se stesso o sono nulli,
    /// agendo da terminatore per ogni percorso nell'albero.
    /// </summary>
    private readonly Node _nil; // Nodo sentinella per rappresentare le foglie

    /// <summary>
    /// Il campo privato che funge da radice dell'albero Rosso-Nero. È il punto di ingresso per tutte le operazioni
    /// e l'unico nodo (oltre al sentinella) che può avere un genitore nullo (impostato su `_nil`). Secondo le proprietà
    /// degli alberi Rosso-Neri, la radice è sempre Nera. Questo campo viene inizializzato puntando al nodo sentinella `_nil`,
    /// rappresentando un albero inizialmente vuoto.
    /// </summary>
    private Node _root;

    /// <summary>
    /// Inizializza una nuova istanza vuota della classe <see cref="RedBlackTree{TKey, TValue}"/>.
    /// Il costruttore crea il nodo sentinella (`_nil`) che sarà condiviso da tutto l'albero per rappresentare
    /// le foglie. Imposta la radice iniziale (`_root`) a puntare al nodo sentinella, indicando che
    /// l'albero è vuoto.
    /// </summary>
    public RedBlackTree()
    {
        // Il nodo sentinella è sempre nero e non contiene dati.
        _nil = new Node(default!, default!) { Color = NodeColor.Black, Parent = null, Left = null, Right = null };
        _root = _nil;
    }
    
    /// <summary>
    /// Ottiene il numero di coppie chiave-valore contenute nell'albero Rosso-Nero.
    /// Questa proprietà viene aggiornata in tempo costante (O(1)) durante le operazioni di inserimento e cancellazione.
    /// </summary>
    public int Count { get; private set; }

    /// <summary>
    /// Aggiunge un elemento con la chiave e il valore specificati all'albero, mantenendo le proprietà rosso-nere.
    /// L'operazione inizia con un inserimento standard da albero binario di ricerca, aggiungendo il nuovo nodo come
    /// una foglia di colore Rosso. Successivamente, invoca un algoritmo di "fixup" (InsertFixup) che risale l'albero
    /// dal nuovo nodo verso la radice, eseguendo una serie di ricolorazioni e rotazioni per correggere eventuali
    /// violazioni delle proprietà rosso-nere (in particolare, la violazione "due nodi rossi adiacenti").
    /// Se la chiave specificata esiste già, il valore del nodo esistente viene semplicemente aggiornato, senza alterare la struttura dell'albero.
    /// L'intera operazione ha una complessità temporale garantita di O(log n).
    /// </summary>
    /// <param name="key">La chiave dell'elemento da aggiungere. Non può essere nulla.</param>
    /// <param name="value">Il valore da associare alla chiave.</param>
    public void Add(TKey key, TValue value)
    {
        var y = _nil;
        var x = _root;
        while (x != _nil)
        {
            y = x;
            var cmp = key.CompareTo(x!.Key);
            switch (cmp)
            {
                case < 0:
                    x = x.Left;
                    break;
                case > 0:
                    x = x.Right;
                    break;
                default:
                    // La chiave esiste già, aggiorna il valore e termina.
                    x.Value = value;
                    return;
            }
        }
        var z = new Node(key, value)
        {
            Parent = y,
            Left = _nil,
            Right = _nil,
            Color = NodeColor.Red
        };
        if (y == _nil)
        {
            _root = z;
        }
        else if (key.CompareTo(y!.Key) < 0)
        {
            y.Left = z;
        }
        else
        {
            y.Right = z;
        }
        Count++;
        InsertFixup(z);
    }
    
    /// <summary>
    /// Ottiene il valore associato alla chiave specificata.
    /// </summary>
    /// <param name="key">La chiave del valore da ottenere.</param>
    /// <param name="value">Quando questo metodo ritorna, contiene il valore associato alla chiave, se la chiave viene
    /// trovata; altrimenti, il valore predefinito per il tipo TValue.</param>
    /// <returns><c>true</c> se l'albero contiene un elemento con la chiave specificata; altrimenti, <c>false</c>.</returns>
    public bool TryGetValue(TKey key, [MaybeNullWhen(false)] out TValue value)
    {
        var node = FindNode(key);
        if (node != _nil)
        {
            value = node!.Value;
            return true;
        }

        value = default;
        return false;
    }

    /// <summary>
    /// Determina se l'albero contiene un elemento con la chiave specificata.
    /// </summary>
    /// <param name="key">La chiave da cercare nell'albero.</param>
    /// <returns><c>true</c> se l'albero contiene un elemento con la chiave; altrimenti, <c>false</c>.</returns>
    public bool ContainsKey(TKey key) => FindNode(key) != _nil;
    
    /// <summary>
    /// Rimuove l'elemento con la chiave specificata dall'albero, mantenendo le proprietà rosso-nere.
    /// </summary>
    /// <param name="key">La chiave dell'elemento da rimuovere.</param>
    /// <returns><c>true</c> se l'elemento è stato trovato e rimosso con successo; altrimenti, <c>false</c>.</returns>
    public bool Remove(TKey key)
    {
        var z = FindNode(key);
        if (z == _nil)
        {
            return false;
        }
        Node? x;
        var y = z;
        var yOriginalColor = y!.Color;
        if (z!.Left == _nil)
        {
            x = z.Right!;
            Transplant(z, z.Right!);
        }
        else if (z.Right == _nil)
        {
            x = z.Left!;
            Transplant(z, z.Left!);
        }
        else
        {
            y = Minimum(z.Right!);
            yOriginalColor = y!.Color;
            x = y.Right!;
            if (y.Parent == z)
            {
                x.Parent = y;
            }
            else
            {
                Transplant(y, y.Right!);
                y.Right = z.Right;
                y.Right!.Parent = y;
            }
            Transplant(z, y);
            y.Left = z.Left;
            y.Left!.Parent = y;
            y.Color = z.Color;
        }
        if (yOriginalColor == NodeColor.Black)
        {
            DeleteFixup(x);
        }
        Count--;
        return true;
    }

    /// <summary>
    /// Rimuove tutti gli elementi dall'albero.
    /// </summary>
    public void Clear()
    {
        _root = _nil;
        Count = 0;
    }

    /// <summary>
    /// Restituisce un enumeratore che itera attraverso l'albero in ordine di chiave (in-order traversal).
    /// </summary>
    /// <returns>Un enumeratore per le coppie chiave-valore dell'albero.</returns>
    public IEnumerable<KeyValuePair<TKey, TValue>> InOrderTraversal()
    {
        if (_root == _nil) yield break;
        var stack = new Stack<Node>();
        var current = _root;
        while (current != _nil || stack.Count > 0)
        {
            while (current != _nil)
            {
                stack.Push(current!);
                current = current!.Left;
            }
            current = stack.Pop();
            yield return new KeyValuePair<TKey, TValue>(current.Key, current.Value);
            current = current.Right;
        }
    }
    
    /// <summary>
    /// Ripristina le proprietà dell'albero Rosso-Nero dopo un'operazione di inserimento.
    /// L'inserimento di un nuovo nodo (sempre di colore Rosso) può violare due proprietà: 1) la radice potrebbe diventare Rossa, o
    /// 2) potrebbero crearsi due nodi Rossi adiacenti. Questo metodo itera risalendo l'albero dal nuovo nodo z
    /// per correggere la violazione "Rosso-Rosso". In ogni iterazione, analizza il colore dello "zio" di z (il fratello del genitore).
    /// Caso 1 (Zio Rosso): Esegue una ricolorazione del padre, dello zio e del nonno, e sposta il problema verso l'alto (sul nonno).
    /// Caso 2 (Zio Nero, caso "triangolo"): Esegue una rotazione per trasformarlo nel caso 3.
    /// Caso 3 (Zio Nero, caso "linea"): Esegue una rotazione e una ricolorazione per risolvere la violazione.
    /// Il loop termina quando la proprietà Rosso-Rosso è soddisfatta. Infine, la radice viene sempre colorata di Nero per garantire la proprietà 2.
    /// </summary>
    /// <param name="z">Il nodo appena inserito, che è di colore Rosso e potrebbe aver causato la violazione.</param>
    private void InsertFixup(Node z)
    {
        while (z.Parent != _nil && z.Parent?.Color == NodeColor.Red)
        {
            if (z.Parent == z.Parent.Parent!.Left)
            {
                var y = z.Parent.Parent.Right; // Lo "zio"
                if (y!.Color == NodeColor.Red)
                {
                    z.Parent.Color = NodeColor.Black;
                    y.Color = NodeColor.Black;
                    z.Parent.Parent.Color = NodeColor.Red;
                    z = z.Parent.Parent;
                }
                else
                {
                    if (z == z.Parent.Right)
                    {
                        z = z.Parent;
                        RotateLeft(z);
                    }
                    z.Parent!.Color = NodeColor.Black;
                    z.Parent.Parent!.Color = NodeColor.Red;
                    RotateRight(z.Parent.Parent);
                }
            }
            else
            {
                var y = z.Parent.Parent!.Left; // Lo "zio"
                if (y!.Color == NodeColor.Red)
                {
                    z.Parent.Color = NodeColor.Black;
                    y.Color = NodeColor.Black;
                    z.Parent.Parent.Color = NodeColor.Red;
                    z = z.Parent.Parent;
                }
                else
                {
                    if (z == z.Parent.Left)
                    {
                        z = z.Parent;
                        RotateRight(z);
                    }
                    z.Parent!.Color = NodeColor.Black;
                    z.Parent.Parent!.Color = NodeColor.Red;
                    RotateLeft(z.Parent.Parent);
                }
            }
        }
        _root.Color = NodeColor.Black;
    }

    /// <summary>
    /// Ripristina le proprietà dell'albero Rosso-Nero dopo la rimozione di un nodo. La rimozione di un nodo Nero
    /// può violare la proprietà dell'altezza nera (numero 5), poiché alcuni percorsi potrebbero contenere un nodo Nero in meno.
    /// Questo metodo gestisce un nodo x che ora ha un "nero extra". Il suo obiettivo è spostare questo "nero extra"
    /// verso l'alto nell'albero fino a quando non viene assorbito (ad esempio, incontrando un nodo Rosso) o la radice viene raggiunta.
    /// Il metodo itera finché x non è la radice e ha ancora il "nero extra". In ogni iterazione, analizza il "fratello" di x,
    /// w, e gestisce quattro casi complessi basati sui colori di w e dei suoi figli, applicando rotazioni e ricolorazioni
    /// per ripristinare l'altezza nera senza violare le altre proprietà dell'albero.
    /// </summary>
    /// <param name="x">Il nodo che occupa la posizione del nodo rimosso e che ha un "nero extra" da risolvere.</param>
    private void DeleteFixup(Node x)
    {
        while (x != _root && x.Color == NodeColor.Black)
        {
            if (x == x.Parent!.Left)
            {
                var w = x.Parent.Right!;
                if (w.Color == NodeColor.Red)
                {
                    w.Color = NodeColor.Black;
                    x.Parent.Color = NodeColor.Red;
                    RotateLeft(x.Parent);
                    w = x.Parent.Right!;
                }
                if (w.Left!.Color == NodeColor.Black && w.Right!.Color == NodeColor.Black)
                {
                    w.Color = NodeColor.Red;
                    x = x.Parent;
                }
                else
                {
                    if (w.Right!.Color == NodeColor.Black)
                    {
                        w.Left!.Color = NodeColor.Black;
                        w.Color = NodeColor.Red;
                        RotateRight(w);
                        w = x.Parent.Right!;
                    }
                    w.Color = x.Parent.Color;
                    x.Parent.Color = NodeColor.Black;
                    w.Right!.Color = NodeColor.Black;
                    RotateLeft(x.Parent);
                    x = _root;
                }
            }
            else
            {
                var w = x.Parent.Left!;
                if (w.Color == NodeColor.Red)
                {
                    w.Color = NodeColor.Black;
                    x.Parent.Color = NodeColor.Red;
                    RotateRight(x.Parent);
                    w = x.Parent.Left!;
                }
                if (w.Right!.Color == NodeColor.Black && w.Left!.Color == NodeColor.Black)
                {
                    w.Color = NodeColor.Red;
                    x = x.Parent;
                }
                else
                {
                    if (w.Left!.Color == NodeColor.Black)
                    {
                        w.Right!.Color = NodeColor.Black;
                        w.Color = NodeColor.Red;
                        RotateLeft(w);
                        w = x.Parent.Left!;
                    }
                    w.Color = x.Parent.Color;
                    x.Parent.Color = NodeColor.Black;
                    w.Left!.Color = NodeColor.Black;
                    RotateRight(x.Parent);
                    x = _root;
                }
            }
        }
        x.Color = NodeColor.Black;
    }

    /// <summary>
    /// Esegue una rotazione a sinistra attorno al nodo x. Questa è una delle operazioni fondamentali per il ribilanciamento
    /// dell'albero Rosso-Nero. Una rotazione a sinistra sposta il figlio destro di x (chiamato y) verso l'alto per diventare
    /// la nuova radice del sottoalbero, mentre x diventa il figlio sinistro di y. Il sottoalbero sinistro originale di y
    /// viene riagganciato come nuovo sottoalbero destro di x. Questa operazione preserva l'ordine dell'albero binario di
    /// ricerca ed è cruciale per correggere gli sbilanciamenti "destra-pesanti".
    /// </summary>
    /// <param name="x">Il nodo che funge da perno per la rotazione. Questo nodo verrà spostato verso il basso e a sinistra.</param>
    private void RotateLeft(Node x)
    {
        var y = x.Right!;
        x.Right = y.Left;
        if (y.Left != _nil)
        {
            y.Left!.Parent = x;
        }
        y.Parent = x.Parent;
        if (x.Parent == _nil)
        {
            _root = y;
        }
        else if (x == x.Parent!.Left)
        {
            x.Parent.Left = y;
        }
        else
        {
            x.Parent.Right = y;
        }
        y.Left = x;
        x.Parent = y;
    }

    /// <summary>
    /// Esegue una rotazione a destra attorno al nodo y. Questa operazione è l'immagine speculare della rotazione a sinistra
    /// ed è utilizzata per correggere gli sbilanciamenti "sinistra-pesanti". Una rotazione a destra promuove il figlio
    /// sinistro di y (chiamato x) a nuova radice del sottoalbero, mentre y diventa il figlio destro di x.
    /// Il sottoalbero destro originale di x viene riagganciato come nuovo sottoalbero sinistro di y. L'operazione
    /// preserva l'ordine di ricerca binaria e, insieme alla rotazione a sinistra, costituisce la base per tutti gli
    /// algoritmi di ribilanciamento dell'albero.
    /// </summary>
    /// <param name="y">Il nodo che funge da perno per la rotazione. Questo nodo verrà spostato verso il basso e a destra.</param>
    private void RotateRight(Node y)
    {
        var x = y.Left!;
        y.Left = x.Right;
        if (x.Right != _nil)
        {
            x.Right!.Parent = y;
        }
        x.Parent = y.Parent;
        if (y.Parent == _nil)
        {
            _root = x;
        }
        else if (y == y.Parent!.Right)
        {
            y.Parent.Right = x;
        }
        else
        {
            y.Parent.Left = x;
        }
        x.Right = y;
        y.Parent = x;
    }

    /// <summary>
    /// Sostituisce un sottoalbero radicato nel nodo u con un sottoalbero radicato nel nodo v.
    /// Questo è un metodo helper fondamentale utilizzato durante la cancellazione di un nodo.
    /// Aggiorna i puntatori del genitore di u per puntare a v, gestendo correttamente i casi in cui u
    /// è la radice dell'albero, un figlio sinistro o un figlio destro. È importante notare che Transplant
    /// non aggiorna i puntatori dei figli di v; questa responsabilità è lasciata al metodo chiamante
    /// (Remove), poiché varia a seconda del caso specifico di cancellazione.
    /// </summary>
    /// <param name="u">Il nodo che deve essere sostituito.</param>
    /// <param name="v">Il nodo che prenderà il posto di u. Può essere il nodo sentinella _nil.</param>
    private void Transplant(Node u, Node v)
    {
        if (u.Parent == _nil)
        {
            _root = v;
        }
        else if (u == u.Parent!.Left)
        {
            u.Parent.Left = v;
        }
        else
        {
            u.Parent.Right = v;
        }
        v.Parent = u.Parent;
    }

    /// <summary>
    /// Cerca un nodo con la chiave specificata all'interno dell'albero Rosso-Nero.
    /// L'operazione segue il percorso standard di un albero binario di ricerca, confrontando la chiave
    /// cercata con la chiave del nodo corrente e decidendo se procedere nel sottoalbero sinistro o destro.
    /// La ricerca termina quando viene trovato il nodo o quando si raggiunge una foglia (il nodo sentinella _nil).
    /// Grazie alle proprietà di bilanciamento dell'albero, questa operazione ha una complessità temporale
    /// garantita di O(log n).
    /// </summary>
    /// <param name="key">La chiave dell'elemento da cercare.</param>
    /// <returns>Il nodo Node corrispondente alla chiave, se trovato; altrimenti, il nodo sentinella _nil.</returns>
    // ReSharper disable once ReturnTypeCanBeNotNullable
    private Node? FindNode(TKey key)
    {
        var current = _root;
        while (current != _nil)
        {
            var cmp = key.CompareTo(current!.Key);
            if (cmp == 0) return current;
            current = cmp < 0 ? current.Left : current.Right;
        }
        return _nil;
    }

    /// <summary>
    /// Trova il nodo con la chiave di valore minimo all'interno del sottoalbero radicato nel nodo specificato.
    /// In un albero binario di ricerca, il nodo con il valore minimo si trova sempre percorrendo il più a sinistra
    /// possibile a partire dalla radice del sottoalbero. Questo metodo implementa tale logica in modo iterativo,
    /// seguendo i puntatori Left fino a raggiungere un nodo che non ha un figlio sinistro (il cui figlio sinistro è il
    /// nodo sentinella _nil). Questo metodo è un componente essenziale dell'algoritmo di cancellazione, in quanto viene
    /// utilizzato per trovare il successore in-order di un nodo che ha due figli.
    /// </summary>
    /// <param name="node">La radice del sottoalbero in cui cercare il valore minimo. Questo nodo non deve essere il
    /// nodo sentinella _nil.</param>
    /// <returns>Il nodo contenente la chiave più piccola nel sottoalbero specificato.</returns>
    // ReSharper disable once ReturnTypeCanBeNotNullable
    private Node? Minimum(Node node)
    {
        while (node.Left != _nil)
        {
            node = node.Left!;
        }
        return node;
    }

    // ── IKeyValueIndexable (dispatch non riflessivo per la PCodeVM) ──
    bool IKeyValueIndexable.TryGetValue(object? key, out object? value)
    {
        if (key is TKey k && TryGetValue(k, out var v))
        {
            value = v;
            return true;
        }
        value = null;
        return false;
    }

    void IKeyValueIndexable.Set(object? key, object? value)
    {
        if (key is TKey k)
        {
            if (value is TValue v) Add(k, v);
            else if (value is null && default(TValue) is null) Add(k, default!);
        }
    }

    bool IKeyValueIndexable.TryGetMin(out object? key, out object? value)
    {
        key = null;
        value = null;
        if (_root == _nil) return false;
        var n = _root;
        while (n.Left != _nil) n = n.Left!;
        key = n.Key;
        value = n.Value;
        return true;
    }

    bool IKeyValueIndexable.TryGetMax(out object? key, out object? value)
    {
        key = null;
        value = null;
        if (_root == _nil) return false;
        var n = _root;
        while (n.Right != _nil) n = n.Right!;
        key = n.Key;
        value = n.Value;
        return true;
    }

    IEnumerable<KeyValuePair<object?, object?>> IKeyValueIndexable.Entries()
    {
        foreach (var (k, v) in InOrderTraversal())
            yield return new KeyValuePair<object?, object?>(k, v);
    }
}
