// FILE: Basic.ISAM.BPlusInternalNode.cs
// ReSharper disable InconsistentNaming
// ReSharper disable CheckNamespace

using Basic.ISAM;
using Basic.Databases;

namespace Basic.ISAM;

/// <summary>
/// Rappresenta un nodo interno (o di instradamento) in un B+Tree. I nodi interni
/// non contengono dati, ma solo chiavi di separazione e puntatori (offset)
/// ai nodi figli, guidando la navigazione all'interno dell'albero.
/// </summary>
internal class BPlusInternalNode : BPlusNode
{
    // Comparer per gestire le chiavi (byte[]) nelle liste ordinate.
    private static readonly ByteArrayComparer KeyComparer = new();

    /// <summary>
    /// Lista degli offset dei nodi figli. Ci sarà sempre un puntatore in più rispetto al numero di chiavi.
    /// ChildrenOffsets[0] punta al sottoalbero con chiavi minore di Keys[0].
    /// ChildrenOffsets[i] punta al sottoalbero con chiavi >= Keys[i-1] minore di Keys[i].
    /// </summary>
    public List<long> ChildrenOffsets { get; } = new();

    /// <summary>
    /// Inizializza una nuova istanza della classe <BPlusInternalNode/>, impostando il nodo come non-foglia.
    /// </summary>
    public BPlusInternalNode()
    {
        IsLeaf = false;
    }

    /// <summary>
    /// Inserisce una nuova coppia chiave/offset nel sotto-albero corretto e gestisce la potenziale divisione (split) dei nodi figli.
    /// </summary>
    /// <param name="key">La chiave da inserire.</param>
    /// <param name="recordOffset">L'offset del record associato alla chiave.</param>
    /// <param name="manager">Il B+Tree Manager necessario per caricare/salvare altri nodi.</param>
    /// <param name="context">Il contesto della transazione corrente per la gestione dei lock.</param>
    /// <returns>
    /// Un oggetto <see cref="BPlusInsertResult"/> che descrive l'esito dell'operazione, indicando se questo nodo
    /// o un suo discendente è stato modificato e, in caso di divisione, fornendo il nuovo nodo fratello creato e la chiave promossa.
    /// </returns>
    /// <remarks>
    /// Se un nodo figlio si divide, questo metodo riceve la chiave "promossa" e il puntatore al nuovo nodo fratello,
    /// inserendoli in sé stesso. Se anche questo inserimento causa un superamento della capacità massima,
    /// anche questo nodo interno si dividerà, propagando a sua volta una chiave e un nuovo nodo al livello superiore.
    /// </remarks>
    public override BPlusInsertResult Insert(byte[] key, long recordOffset, BPlusTreeManager manager, TransactionContext? context)
    {
        const int maxDepth = 64;
        var spine = new List<(BPlusInternalNode Node, int ChildIndex)>(8);
        var visited = new HashSet<long>();
        var node = (BPlusNode)this;

        for (var depth = 0; !node.IsLeaf; depth++)
        {
            if (depth >= maxDepth)
            {
                throw new InvalidOperationException(
                    $"Profondità B+ eccessiva (>{maxDepth}); possibile ciclo o indice corrotto.");
            }

            var internalNode = (BPlusInternalNode)node;
            var index = internalNode.Keys.BinarySearch(key, KeyComparer);
            if (index < 0)
            {
                index = ~index;
            }

            var childOffset = internalNode.ChildrenOffsets[index];
            if (childOffset <= 0)
            {
                throw new InvalidOperationException($"Offset figlio B+ non valido: {childOffset}.");
            }

            if (!visited.Add(childOffset))
            {
                throw new InvalidOperationException(
                    $"Ciclo B+ rilevato a offset figlio {childOffset}; indice probabilmente corrotto.");
            }

            spine.Add((internalNode, index));
            node = manager.LoadNode(childOffset);
        }

        var result = ((BPlusLeafNode)node).Insert(key, recordOffset, manager, context);
        if (result.NewNode == null)
        {
            if (result.NodeUpdated)
            {
                manager.SaveNode(node);
            }

            return new BPlusInsertResult { NodeUpdated = false };
        }

        if (result.PromotedKey is null)
        {
            throw new InvalidOperationException(
                "Lo split di un nodo figlio deve sempre restituire una chiave promossa.");
        }

        var split = result;
        for (var i = spine.Count - 1; i >= 0; i--)
        {
            var (parent, childIndex) = spine[i];
            if (split.NewNodeOffset < 0)
            {
                split.NewNodeOffset = manager.SaveNode(split.NewNode!);
            }

            parent.Keys.Insert(childIndex, split.PromotedKey);
            parent.ChildrenOffsets.Insert(childIndex + 1, split.NewNodeOffset);

            if (parent.KeyCount <= manager.GetMaxKeysForInternalNode())
            {
                manager.SaveNode(parent);
                return new BPlusInsertResult { NodeUpdated = false };
            }

            var sibling = new BPlusInternalNode();
            var splitIndex = BPlusFillFactor.ComputeInternalSplitIndex(parent.KeyCount);
            var promotedKey = parent.Keys[splitIndex];

            sibling.Keys.AddRange(parent.Keys.Skip(splitIndex + 1));
            sibling.ChildrenOffsets.AddRange(parent.ChildrenOffsets.Skip(splitIndex + 1));
            parent.Keys.RemoveRange(splitIndex, parent.KeyCount - splitIndex);
            parent.ChildrenOffsets.RemoveRange(splitIndex + 1, parent.ChildrenOffsets.Count - (splitIndex + 1));

            manager.SaveNode(parent);
            var siblingOffset = manager.SaveNode(sibling);

            split = new BPlusInsertResult
            {
                NodeUpdated = true,
                NewNode = sibling,
                NewNodeOffset = siblingOffset,
                PromotedKey = promotedKey,
            };
        }

        return split;
    }

    /// <summary>
    /// Cerca una chiave scendendo ricorsivamente nel sotto-albero appropriato.
    /// </summary>
    /// <param name="key">La chiave da cercare.</param>
    /// <param name="manager">Il B+Tree Manager necessario per caricare i nodi figli.</param>
    /// <returns>
    /// Una lista di offset di record corrispondenti alla chiave, se trovata nel nodo foglia di destinazione;
    /// altrimenti, una lista vuota.
    /// </returns>
    public override List<long> Search(byte[] key, BPlusTreeManager manager)
    {
        var index = Keys.BinarySearch(key, KeyComparer);
        if (index < 0)
        {
            // La chiave non corrisponde a un separatore, ~index ci dà il punto di inserimento,
            // che corrisponde all'indice del sotto-albero da seguire.
            index = ~index;
        }
        else
        {
            // La chiave corrisponde esattamente a un separatore. Per convenzione del B+Tree,
            // la chiave stessa e tutte quelle maggiori si trovano nel sotto-albero a destra del separatore.
            index += 1;
        }

        // Usa il manager per caricare il nodo figlio corretto dal disco.
        var childNode = manager.LoadNode(ChildrenOffsets[index]);

        // Chiama ricorsivamente Search sul nodo figlio.
        return childNode.Search(key, manager);
    }

    /// <inheritdoc />
    public override List<long> SearchLowerBound(byte[] key, BPlusTreeManager manager)
    {
        var index = Keys.BinarySearch(key, KeyComparer);
        if (index < 0)
        {
            index = ~index;
        }
        else
        {
            index += 1;
        }

        var childNode = manager.LoadNode(ChildrenOffsets[index]);
        return childNode.SearchLowerBound(key, manager);
    }

    /// <summary>
    /// Trova il sotto-albero corretto, vi delega la cancellazione e, se necessario, orchestra il ribilanciamento
    /// del nodo figlio (tramite il manager) in caso di underflow (carenza di chiavi).
    /// </summary>
    /// <param name="key">La chiave da rimuovere.</param>
    /// <param name="recordOffset">L'offset specifico del record da rimuovere.</param>
    /// <param name="manager">Il B+Tree Manager che contiene la logica di ribilanciamento.</param>
    /// <param name="context">Il contesto della transazione corrente per la gestione dei lock.</param>
    /// <returns>True se un record è stato effettivamente rimosso.</returns>
    public override bool Delete(byte[] key, long recordOffset, BPlusTreeManager manager, TransactionContext? context)
    {
        // 1. Trova il sotto-albero corretto dove la chiave dovrebbe trovarsi.
        int childIndex;
        var index = Keys.BinarySearch(key, KeyComparer);
        if (index < 0)
        {
            childIndex = ~index;
        }
        else
        {
            childIndex = index + 1; // Se la chiave corrisponde a un separatore, vai a destra.
        }

        var childNode = manager.LoadNode(ChildrenOffsets[childIndex]);
        var recordRemoved = childNode.Delete(key, recordOffset, manager, context);

        if (!recordRemoved)
        {
            return false; // Cancellazione non avvenuta, nessuna modifica necessaria.
        }

        // 2. Controlla se il nodo figlio è sufficientemente pieno dopo la cancellazione.
        if (childNode.KeyCount >= GetChildFillThreshold(childNode, manager))
        {
            // Se il nodo è stato modificato (es. in un sotto-albero) ma non è in underflow,
            // è sufficiente salvarlo.
            manager.SaveNode(childNode);
            return true;
        }

        // 3. --- GESTIONE UNDERFLOW DEL NODO FIGLIO ---
        // Il nodo figlio è troppo vuoto e necessita di un riequilibrio.

        // 3a. Tenta di ridistribuire (prendere in prestito) dal fratello SINISTRO.
        if (childIndex > 0)
        {
            var leftSibling = manager.LoadNode(ChildrenOffsets[childIndex - 1]);
            if (leftSibling.KeyCount > GetChildFillThreshold(leftSibling, manager))
            {
                manager.Redistribute(this, childIndex - 1, leftSibling, childNode);
                return true;
            }
        }

        // 3b. Tenta di ridistribuire (prendere in prestito) dal fratello DESTRO.
        if (childIndex < KeyCount) // Esiste un fratello destro
        {
            var rightSibling = manager.LoadNode(ChildrenOffsets[childIndex + 1]);
            if (rightSibling.KeyCount > GetChildFillThreshold(rightSibling, manager))
            {
                manager.Redistribute(this, childIndex, childNode, rightSibling);
                return true;
            }
        }

        // 4. Se la ridistribuzione non è possibile, esegue il MERGE (unione) con un fratello.
        if (childIndex > 0)
        {
            // Merge con il fratello sinistro.
            manager.Merge(this, childIndex - 1);
        }
        else
        {
            // Merge con il fratello destro.
            manager.Merge(this, childIndex);
        }
        
        // Salva le modifiche a questo nodo (il padre), che potrebbero essere cambiate
        // a seguito della ridistribuzione o del merge.
        manager.SaveNode(this);
        return true;
    }

    private static int GetChildFillThreshold(BPlusNode node, BPlusTreeManager manager)
    {
        var capacity = node is BPlusLeafNode
            ? manager.GetMaxKeysPerNode()
            : manager.GetMaxKeysForInternalNode();
        return capacity / 2;
    }

    /// <summary>
    /// Trova l'indice del puntatore al figlio corretto in cui continuare la ricerca o l'inserimento.
    /// </summary>
    /// <param name="key">La chiave da cercare.</param>
    /// <returns>L'indice (0-based) del puntatore al sotto-albero corretto.</returns>
    public int FindChildIndexFor(byte[] key)
    {
        // Esegue una ricerca binaria per trovare il punto di inserimento.
        var index = Keys.BinarySearch(key, KeyComparer);

        // Se BinarySearch restituisce un valore negativo, il suo complemento bitwise (~)
        // ci dà l'indice del primo elemento che è più grande della chiave.
        // Questo è esattamente l'indice del puntatore che dobbiamo seguire.
        return index < 0 ? ~index : index + 1;
    }

    /// <summary>
    /// Serializza lo stato completo del nodo (flag IsLeaf, chiavi e puntatori ai figli) in un flusso binario.
    /// </summary>
    /// <param name="writer">Il BinaryWriter su cui scrivere i dati del nodo.</param>
    public override void Serialize(BinaryWriter writer)
    {
        writer.Write(IsLeaf);
        writer.Write(KeyCount);

        foreach (var key in Keys)
        {
            writer.Write(key.Length);
            writer.Write(key);
        }

        writer.Write(ChildrenOffsets.Count);
        foreach (var offset in ChildrenOffsets)
        {
            writer.Write(offset);
        }
    }

    /// <summary>
    /// Deserializza lo stato del nodo da un flusso binario, popolando le liste delle chiavi e dei puntatori ai figli.
    /// </summary>
    /// <param name="reader">Il BinaryReader da cui leggere i dati del nodo.</param>
    public override void Deserialize(BinaryReader reader)
    {
        IsLeaf = reader.ReadBoolean();
        var keyCount = reader.ReadInt32();

        Keys.Clear();
        ChildrenOffsets.Clear();

        for (var i = 0; i < keyCount; i++)
        {
            var keyLength = reader.ReadInt32();
            Keys.Add(reader.ReadBytes(keyLength));
        }

        var childCount = reader.ReadInt32();
        for (var i = 0; i < childCount; i++)
        {
            ChildrenOffsets.Add(reader.ReadInt64());
        }
    }
}