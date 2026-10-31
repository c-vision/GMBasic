// FILE: Basic.AI.Knowledge.VaultIndex
// Second Brain — L1 VaultIndex: file_index O(1) lookup per path assoluto.
// Shape (per il piano AI-Second-Brain-Vision §1 L1): ogni file mappa a
// {document_id, chunk_id, title, content, tags, folder_path, frontmatter,
//  content_hash, embedding_model, embedding_dimension, chunker_version, indexed_at}.
// W1 implementa SOLO il lookup di base su vaults.db: folder_path, content_hash, indexed_at
// per path (dedup incrementale). frontmatter (W2), chunking e embedding (W4) popoleranno
// i campi mancanti — le colonne ci sono, i valori restano null finche' il rispettivo
// livello non le scrive. Nessun campo e' rimosso dal modello del piano.
using System.Text.Json;
using Microsoft.Data.Sqlite;

namespace Basic.AI.Knowledge;

public static class VaultIndex
{
    private static SqliteConnection Open()
    {
        var dir = Path.GetDirectoryName(VaultManager.DbPath)!;
        if (!Directory.Exists(dir)) Directory.CreateDirectory(dir);
        var conn = new SqliteConnection($"Data Source={VaultManager.DbPath};Pooling=False");
        conn.Open();
        return conn;
    }

    /// <summary>True se il file (path assoluto) e' nell'indice del vault.</summary>
    public static bool IsIndexed(string vaultName, string path)
    {
        using var conn = Open();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = """
            SELECT COUNT(*) FROM vault_files vf
            JOIN vaults v ON v.id = vf.vault_id
            WHERE v.name=$n AND vf.path=$p;
            """;
        cmd.Parameters.AddWithValue("$n", vaultName);
        cmd.Parameters.AddWithValue("$p", Path.GetFullPath(path));
        return (long)(cmd.ExecuteScalar() ?? 0L) > 0;
    }

    /// <summary>Content_hash SHA256 del file indicizzato, o null se assente.</summary>
    public static string? GetContentHash(string vaultName, string path)
    {
        using var conn = Open();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = """
            SELECT vf.content_hash FROM vault_files vf
            JOIN vaults v ON v.id = vf.vault_id
            WHERE v.name=$n AND vf.path=$p;
            """;
        cmd.Parameters.AddWithValue("$n", vaultName);
        cmd.Parameters.AddWithValue("$p", Path.GetFullPath(path));
        return cmd.ExecuteScalar() as string;
    }

    /// <summary>Folder_path relativo ('/' per la root) del file indicizzato, o null.</summary>
    public static string? GetFolderPath(string vaultName, string path)
    {
        using var conn = Open();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = """
            SELECT vf.folder_path FROM vault_files vf
            JOIN vaults v ON v.id = vf.vault_id
            WHERE v.name=$n AND vf.path=$p;
            """;
        cmd.Parameters.AddWithValue("$n", vaultName);
        cmd.Parameters.AddWithValue("$p", Path.GetFullPath(path));
        return cmd.ExecuteScalar() as string;
    }

    /// <summary>Lista dei path assoluti in una cartella relativa del vault.</summary>
    public static string ListFilesInFolder(string vaultName, string folderPath)
    {
        using var conn = Open();
        var files = new List<string>();
        using var cmd = conn.CreateCommand();
        cmd.CommandText = """
            SELECT vf.path FROM vault_files vf
            JOIN vaults v ON v.id = vf.vault_id
            WHERE v.name=$n AND vf.folder_path=$f
            ORDER BY vf.path;
            """;
        cmd.Parameters.AddWithValue("$n", vaultName);
        cmd.Parameters.AddWithValue("$f", folderPath);
        using var r = cmd.ExecuteReader();
        while (r.Read()) files.Add(r.GetString(0));
        return JsonSerializer.Serialize(files);
    }
}
