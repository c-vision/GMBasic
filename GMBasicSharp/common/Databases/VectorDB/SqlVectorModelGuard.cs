// FILE: Basic.SQLite.Vector.SqlVectorModelGuard
// Fase 3 (todo Tokenizer-WordPiece): guardia d'integrità del modello per-collection, condivisa
// da SqlVectorIndexer (scritture) e SqlVectorSearch (letture) — stessa regola, un solo posto.
// Un vettore scritto col modello B non può finire (né essere letto) in una collection dichiarata
// col modello A: la distanza cosine resterebbe un numero e nessun errore partirebbe.
using System.Globalization;
using System.Security.Cryptography;
using System.Text;
using Microsoft.Data.Sqlite;

namespace Basic.SQLite.Vector;

internal static class SqlVectorModelGuard
{
    /// <summary>Digest dell'identità del provider per la collection — ModelDigest esplicito se il
    /// provider lo dà (firma pesi/vocab), altrimenti derivato deterministicamente da
    /// ModelVersion + dimensioni.</summary>
    public static string ComputeDigest(IEmbeddingProvider provider, int dimensions) =>
        provider.ModelDigest.Length > 0
            ? provider.ModelDigest
            : Sha256($"v:{provider.ModelVersion}|d:{dimensions}");

    /// <summary>Guardia all'accesso a una collection esistente — da chiamare SOTTO il lock della
    /// connessione condivisa (indexer: _writeLock; search: _connectionLock). DB pre-migrazione
    /// (digest NULL) → backfill col provider attivo, poi enforcement; digest diverso → errore
    /// esplicito con i DUE digest e le versioni.</summary>
    public static void GuardOrBackfill(SqliteConnection connection, long collectionId, IEmbeddingProvider provider)
    {
        using var find = connection.CreateCommand();
        find.CommandText = "SELECT model_digest, dimensions, model_version FROM vec_collections WHERE id = $id";
        find.Parameters.AddWithValue("$id", collectionId);
        using var reader = find.ExecuteReader();
        if (!reader.Read())
            throw new InvalidOperationException($"Collection {collectionId} not found.");
        var storedDigest = reader.GetValue(0) as string;
        var dims = Convert.ToInt32(reader.GetValue(1), CultureInfo.InvariantCulture);
        var storedVersion = Convert.ToString(reader.GetValue(2), CultureInfo.InvariantCulture) ?? "";
        var current = ComputeDigest(provider, dims);

        if (storedDigest is null || storedDigest.Length == 0)
        {
            if (current.Length == 0)
                return;
            using var backfill = connection.CreateCommand();
            backfill.CommandText = "UPDATE vec_collections SET model_digest = $d WHERE id = $id AND model_digest IS NULL";
            backfill.Parameters.AddWithValue("$d", current);
            backfill.Parameters.AddWithValue("$id", collectionId);
            backfill.ExecuteNonQuery();
            return;
        }

        if (storedDigest != current)
            throw new InvalidOperationException(
                $"Collection {collectionId} appartiene al modello '{storedVersion}' (digest {storedDigest}); " +
                $"il provider attivo è '{provider.ModelVersion}' (digest {current}). " +
                "Rifiuto: mai mescolare modelli nello stesso indice — rigenera la collection col modello corretto.");
    }

    private static string Sha256(string text)
    {
        var bytes = SHA256.HashData(Encoding.UTF8.GetBytes(text));
        return Convert.ToHexString(bytes).ToLowerInvariant();
    }
}