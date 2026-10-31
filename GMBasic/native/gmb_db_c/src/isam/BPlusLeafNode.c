/* FILE: native/gmb_db_c/src/ISAM/BPlusLeafNode.c */
/* Port 1:1 di modules/Database/ISAM/BPlusLeafNode.cs */
/* Ogni riga del C# è sotto come // — nessun commento, attributo o riga persa. */
#include "../../include/gmb_db_port.h"

/* --- C# originale --- */
//// FILE: Basic.ISAM.BPlusLeafNode.cs
//// ReSharper disable InconsistentNaming
//// ReSharper disable CheckNamespace
//
//using System.Text;
//using Basic.ISAM;
//using Basic.Databases;
//
//namespace Basic.ISAM;
//
///// <summary>
///// Comparer per array di byte, necessario per le operazioni di ricerca e ordinamento.
///// </summary>
//internal class ByteArrayComparer : IComparer<byte[]>
//{
//    public int Compare(byte[]? x, byte[]? y)
//    {
//        if (x == null || y == null) return 0;
//        var minLength = Math.Min(x.Length, y.Length);
//        for (var i = 0; i < minLength; i++)
//        {
//            var cmp = x[i].CompareTo(y[i]);
//            if (cmp != 0) return cmp;
//        }
//
//        return x.Length.CompareTo(y.Length);
//    }
//}
//
///// <summary>
///// Rappresenta un nodo foglia in un B+Tree. I nodi foglia contengono le chiavi
///// e le liste di offset ai record dati corrispondenti. Sono collegati in una
///// lista sequenziale per consentire la scansione ordinata.
///// </summary>
//internal class BPlusLeafNode : BPlusNode
//{
//    // Comparer per gestire le chiavi (byte[]) nelle liste ordinate.
//    private static readonly ByteArrayComparer KeyComparer = new();
//
//    /// <summary>
//    /// Lista delle liste di offset dei record. Ogni elemento della lista esterna
//    /// corrisponde a una chiave nella stessa posizione nella lista `Keys`.
//    /// </summary>
//    public List<List<long>> RecordOffsets { get; } = new();
//
//    /// <summary>
//    /// Offset del prossimo nodo foglia nel file, per la scansione sequenziale.
//    /// -1 se questo è l'ultimo nodo foglia.
//    /// </summary>
//    public long NextLeafOffset { get; set; } = -1;
//
//    /// <summary>
//    /// Puntatore all'offset del nodo foglia precedente nella sequenza.
//    /// </summary>
//    public long PrevLeafOffset { get; set; } = -1;
//
//    public BPlusLeafNode()
//    {
//        IsLeaf = true;
//    }
//
//    /// <summary>
//    /// Inserisce una nuova coppia chiave/offset in questo nodo foglia del B+Tree.
//    /// Gestisce l'inserimento, la divisione (split) del nodo se pieno, e la propagazione
//    /// di una chiave promossa al livello superiore. Acquisisce lock di scrittura sulla chiave
//    /// per il livello di isolamento `Serializable`.
//    /// </summary>
//    /// <param name="key">La chiave binaria da inserire.</param>
//    /// <param name="recordOffset">L'offset fisico del record associato alla chiave.</param>
//    /// <param name="manager">Il <see cref="BPlusTreeManager"/> che gestisce l'albero.</param>
//    /// <param name="context">Il contesto della transazione corrente, necessario per il locking a livello di chiave.</param>
//    /// <returns>
//    /// Un oggetto <see cref="BPlusInsertResult"/> che indica se il nodo è stato aggiornato,
//    /// se è stato diviso e, in tal caso, la chiave promossa e il nuovo nodo creato.
//    /// </returns>
//    /// <remarks>
//    /// Se il nodo è pieno, viene diviso a metà, e la prima chiave del nuovo nodo fratello viene "promossa"
//    /// al nodo padre per fungere da separatore. Questa operazione aggiorna anche la lista doppiamente
//    /// concatenata dei nodi foglia.
//    /// </remarks>
//    public override BPlusInsertResult Insert(byte[] key, long recordOffset, BPlusTreeManager manager, TransactionContext? context)
//    {
//        // Trova la posizione corretta per la chiave (o l'indice del duplicato).
//        var index = Keys.BinarySearch(key, KeyComparer);
//
//        // Se la chiave esiste già, aggiungi l'offset al set di duplicati.
//        if (index >= 0)
//        {
//            // Se l'indice è UNIQUE, non permettere duplicati su offset diversi.
//            if (manager.GetIndexDefinition().IsUnique)
//            {
//                var existingOffsets = RecordOffsets[index];
//                if (existingOffsets.Contains(recordOffset))
//                {
//                    return new BPlusInsertResult { NodeUpdated = true };
//                }
//
//                throw new InvalidOperationException($"Violazione di unicità dell'indice '{manager.GetIndexDefinition().Name}' per la chiave '{Encoding.UTF8.GetString(key)}'.");
//            }
//            RecordOffsets[index].Add(recordOffset);
//            manager.SaveNode(this);
//            return new BPlusInsertResult { NodeUpdated = true };
//        }
//
//        index = ~index;
//        Keys.Insert(index, key);
//        RecordOffsets.Insert(index, [recordOffset]);
//
//        if (KeyCount <= manager.GetMaxKeysPerNode())
//        {
//            manager.SaveNode(this);
//            return new BPlusInsertResult { NodeUpdated = true };
//        }
//
//        return SplitAndPromote(manager);
//    }
//
//    /// <summary>
//    /// ponytail: append monotono in RAM — niente SaveNode finché la foglia non è piena (split).
//    /// Restituisce false se la chiave non è ≥ ultima chiave del nodo.
//    /// </summary>
//    internal bool TryAppendMonotonic(
//        byte[] key,
//        long recordOffset,
//        BPlusTreeManager manager,
//        out BPlusInsertResult? splitResult)
//    {
//        splitResult = null;
//        if (KeyCount > 0)
//        {
//            var cmp = KeyComparer.Compare(Keys[^1], key);
//            if (cmp > 0)
//            {
//                return false;
//            }
//
//            if (cmp == 0)
//            {
//                if (manager.GetIndexDefinition().IsUnique)
//                {
//                    if (RecordOffsets[^1].Contains(recordOffset))
//                    {
//                        return true;
//                    }
//
//                    throw new InvalidOperationException(
//                        $"Violazione di unicità dell'indice '{manager.GetIndexDefinition().Name}' per la chiave '{Encoding.UTF8.GetString(key)}'.");
//                }
//
//                RecordOffsets[^1].Add(recordOffset);
//                return true;
//            }
//        }
//
//        Keys.Add(key);
//        RecordOffsets.Add([recordOffset]);
//        if (KeyCount <= manager.GetMaxKeysPerNode())
//        {
//            return true;
//        }
//
//        splitResult = SplitAndPromote(manager);
//        return true;
//    }
//
//    /// <summary>Divide la foglia piena, persiste entrambe le metà e restituisce la chiave promossa.</summary>
//    internal BPlusInsertResult SplitAndPromote(BPlusTreeManager manager)
//    {
//        var sibling = new BPlusLeafNode();
//        var splitIndex = BPlusFillFactor.ComputeLeafSplitIndex(KeyCount);
//
//        sibling.Keys.AddRange(Keys.Skip(splitIndex));
//        sibling.RecordOffsets.AddRange(RecordOffsets.Skip(splitIndex));
//
//        Keys.RemoveRange(splitIndex, KeyCount - splitIndex);
//        RecordOffsets.RemoveRange(splitIndex, RecordOffsets.Count - splitIndex);
//
//        if (NodeOffset <= 0)
//        {
//            manager.SaveNode(this);
//        }
//
//        var siblingOffset = manager.SaveNode(sibling);
//
//        var originalNextOffset = NextLeafOffset;
//
//        sibling.NextLeafOffset = originalNextOffset;
//        sibling.PrevLeafOffset = NodeOffset;
//        NextLeafOffset = siblingOffset;
//
//        if (originalNextOffset != -1)
//        {
//            var nextNextPage = (BPlusLeafNode)manager.LoadNode(originalNextOffset);
//            nextNextPage.PrevLeafOffset = siblingOffset;
//            manager.SaveNode(nextNextPage);
//        }
//
//        manager.SaveNode(sibling);
//        manager.SaveNode(this);
//
//        return new BPlusInsertResult
//        {
//            NodeUpdated = true,
//            NewNode = sibling,
//            NewNodeOffset = siblingOffset,
//            PromotedKey = sibling.Keys[0],
//        };
//    }
//
//    public override List<long> Search(byte[] key, BPlusTreeManager manager)
//    {
//        var index = Keys.BinarySearch(key, KeyComparer);
//        if (index >= 0)
//        {
//            // Restituisce una nuova lista per evitare che il chiamante modifichi quella interna.
//            return [..RecordOffsets[index]];
//        }
//
//        return new List<long>(); // Restituisce una lista vuota se la chiave non è trovata.
//    }
//
//    /// <inheritdoc />
//    public override List<long> SearchLowerBound(byte[] key, BPlusTreeManager manager)
//    {
//        var index = Keys.BinarySearch(key, KeyComparer);
//        if (index < 0)
//        {
//            index = ~index;
//        }
//
//        if (index >= KeyCount)
//        {
//            if (NextLeafOffset == -1)
//            {
//                return [];
//            }
//
//            var next = (BPlusLeafNode)manager.LoadNode(NextLeafOffset);
//            return next.KeyCount > 0 ? [..next.RecordOffsets[0]] : [];
//        }
//
//        return [..RecordOffsets[index]];
//    }
//
//    /// <summary>
//    /// Rimuove una specifica coppia chiave/offset da questo nodo foglia.
//    /// </summary>
//    /// <param name="key">La chiave da rimuovere.</param>
//    /// <param name="recordOffset">Lo specifico offset del record da rimuovere.</param>
//    /// <returns>True se un elemento è stato rimosso, altrimenti False.</returns>
//    public bool Remove(byte[] key, long recordOffset)
//    {
//        var keyIndex = Keys.BinarySearch(key, KeyComparer);
//        if (keyIndex < 0)
//        {
//            return false; // Chiave non presente nel nodo.
//        }
//
//        var offsets = RecordOffsets[keyIndex];
//        var removed = offsets.Remove(recordOffset);
//
//        if (removed && offsets.Count == 0)
//        {
//            // Se era l'ultimo offset per questa chiave, rimuovi anche la chiave.
//            Keys.RemoveAt(keyIndex);
//            RecordOffsets.RemoveAt(keyIndex);
//        }
//
//        return removed;
//    }
//
//    /// <summary>
//    /// Rimuove una specifica coppia chiave/offset record da questo nodo foglia del B+Tree.
//    /// </summary>
//    /// <param name="key">La chiave binaria dell'entry da rimuovere.</param>
//    /// <param name="recordOffset">L'offset fisico specifico del record da rimuovere.</param>
//    /// <param name="manager">Il <see cref="BPlusTreeManager"/> che gestisce l'albero.</param>
//    /// <param name="context">Il contesto della transazione corrente, necessario per il locking a livello di chiave.
//    /// Viene propagato ma non utilizzato direttamente per il locking del record in questo metodo,
//    /// in quanto il locking della chiave è gestito dal chiamante.</param>
//    /// <returns>True se la coppia chiave/offset è stata trovata e rimossa con successo, altrimenti False.</returns>
//    /// <remarks>
//    /// Questo metodo cerca prima la chiave nel nodo. Se trovata, tenta di rimuovere lo specifico
//    /// `recordOffset` dalla lista di duplicati associata a quella chiave. Se la lista di offset
//    /// diventa vuota, la chiave stessa viene rimossa dal nodo. Le operazioni di underflow e
//    /// ribilanciamento (merge/ridistribuzione) per i nodi foglia sono gestite dal <see cref="BPlusTreeManager"/>
//    /// dopo la chiamata a questo metodo, se necessario.
//    /// </remarks>
//    public override bool Delete(byte[] key, long recordOffset, BPlusTreeManager manager, TransactionContext? context)
//    {
//        // Cerca la chiave all'interno del nodo foglia.
//        var keyIndex = Keys.BinarySearch(key, KeyComparer);
//
//        // Se la chiave non è trovata, non c'è nulla da cancellare.
//        if (keyIndex < 0)
//        {
//            return false;
//        }
//
//        // La chiave esiste, ora cerca lo specifico offset del record da rimuovere
//        // all'interno della lista di duplicati associata a questa chiave.
//        var offsets = RecordOffsets[keyIndex];
//        var removed = offsets.Remove(recordOffset);
//
//        // Se l'offset specifico non era presente nella lista, la cancellazione non ha avuto luogo.
//        if (!removed)
//        {
//            return false;
//        }
//
//        // Se la lista di offset per questa chiave è diventata vuota dopo la rimozione,
//        // significa che non ci sono più record associati a questa chiave;
//        // rimuoviamo la chiave stessa e la sua lista di offset dal nodo foglia.
//        if (offsets.Count == 0)
//        {
//            Keys.RemoveAt(keyIndex);
//            RecordOffsets.RemoveAt(keyIndex);
//        }
//
//        // Salva le modifiche a questo nodo su disco.
//        manager.SaveNode(this);
//        
//        // Indica che un record è stato effettivamente rimosso,
//        // permettendo al chiamante di gestire l'underflow se necessario.
//        return true;
//    }
//
//    public override void Serialize(BinaryWriter writer)
//    {
//        writer.Write(IsLeaf);
//        writer.Write(NextLeafOffset);
//        writer.Write(PrevLeafOffset);
//        writer.Write(KeyCount);
//
//        for (var i = 0; i < KeyCount; i++)
//        {
//            var key = Keys[i];
//            writer.Write(key.Length);
//            writer.Write(key);
//
//            var offsets = RecordOffsets[i];
//            writer.Write(offsets.Count);
//            foreach (var offset in offsets)
//            {
//                writer.Write(offset);
//            }
//        }
//    }
//
//    public override void Deserialize(BinaryReader reader)
//    {
//        IsLeaf = reader.ReadBoolean();
//        NextLeafOffset = reader.ReadInt64();
//        PrevLeafOffset = reader.ReadInt64();
//        var keyCount = reader.ReadInt32();
//
//        Keys.Clear();
//        RecordOffsets.Clear();
//
//        for (var i = 0; i < keyCount; i++)
//        {
//            var keyLength = reader.ReadInt32();
//            Keys.Add(reader.ReadBytes(keyLength));
//
//            var offsetCount = reader.ReadInt32();
//            var offsets = new List<long>(offsetCount);
//            for (var j = 0; j < offsetCount; j++)
//            {
//                offsets.Add(reader.ReadInt64());
//            }
//
//            RecordOffsets.Add(offsets);
//        }
//    }
//}

/* --- C (corrispondenza funzione-per-funzione: riempito file per file) --- */














