// FILE: Basic.Connectors.ConnectorFilesystem
// Filesystem connector F1 — sandbox WORKSPACE PROJECT READ+WRITE, /etc DENY, ~/.ssh DENY (SYSTEM.txt §10)

using System.Security.Cryptography;
using System.Text.Json;

namespace Basic.Connectors;

public static class ConnectorFilesystem
{
    public static string Dispatch(string tool, string argsJson)
    {
        var args = TryParseArgs(argsJson);
        return tool.ToLowerInvariant() switch
        {
            "list" => List(GetArg(args, "path") ?? "."),
            "find" => Find(GetArg(args, "path") ?? ".", GetArg(args, "pattern") ?? "*"),
            "read" => Read(GetArg(args, "path") ?? throw new ArgumentException("path required")),
            "write" => Write(GetArg(args, "path") ?? throw new ArgumentException("path required"), GetArg(args, "content") ?? ""),
            "move" => Move(GetArg(args, "from") ?? GetArg(args, "path") ?? throw new ArgumentException("from required"), GetArg(args, "to") ?? throw new ArgumentException("to required")),
            "copy" => Copy(GetArg(args, "from") ?? GetArg(args, "path") ?? throw new ArgumentException("from required"), GetArg(args, "to") ?? throw new ArgumentException("to required")),
            "delete" => Delete(GetArg(args, "path") ?? throw new ArgumentException("path required")),
            "metadata" => Metadata(GetArg(args, "path") ?? throw new ArgumentException("path required")),
            "hash" => Hash(GetArg(args, "path") ?? throw new ArgumentException("path required")),
            "diff" => Diff(GetArg(args, "path1") ?? GetArg(args, "from") ?? "", GetArg(args, "path2") ?? GetArg(args, "to") ?? ""),
            _ => throw new ArgumentException($"Filesystem tool '{tool}' unknown (list/find/read/write/move/copy/delete/metadata/hash/diff)")
        };
    }

    private static string List(string path)
    {
        var full = ResolveWithSandbox(path, requireWrite: false);
        if (File.Exists(full)) return JsonSerializer.Serialize(new { path, type = "file", size = new FileInfo(full).Length });
        if (!Directory.Exists(full)) throw new DirectoryNotFoundException($"Path not found: {path}");
        var entries = Directory.GetFileSystemEntries(full).Select(e => new
        {
            name = Path.GetFileName(e),
            path = MakeRelative(e),
            type = Directory.Exists(e) ? "dir" : "file",
            size = File.Exists(e) ? new FileInfo(e).Length : 0,
            modified = File.GetLastWriteTimeUtc(e).ToString("o")
        }).ToArray();
        return JsonSerializer.Serialize(entries, new JsonSerializerOptions { WriteIndented = true });
    }

    private static string Find(string path, string pattern)
    {
        var full = ResolveWithSandbox(path, requireWrite: false);
        if (!Directory.Exists(full)) throw new DirectoryNotFoundException($"Path not found: {path}");
        var files = Directory.GetFiles(full, pattern, SearchOption.AllDirectories)
            .Select(f => MakeRelative(f)).ToArray();
        return JsonSerializer.Serialize(files, new JsonSerializerOptions { WriteIndented = true });
    }

    private static string Read(string path)
    {
        var full = ResolveWithSandbox(path, requireWrite: false);
        if (!File.Exists(full)) throw new FileNotFoundException($"File not found: {path}");
        var text = File.ReadAllText(full);
        return JsonSerializer.Serialize(new { path, content = text, size = text.Length });
    }

    private static string Write(string path, string content)
    {
        var full = ResolveWithSandbox(path, requireWrite: true);
        var dir = Path.GetDirectoryName(full);
        if (!string.IsNullOrEmpty(dir) && !Directory.Exists(dir)) Directory.CreateDirectory(dir);
        File.WriteAllText(full, content);
        return JsonSerializer.Serialize(new { path, written = content.Length, status = "ok" });
    }

    private static string Move(string from, string to)
    {
        var fullFrom = ResolveWithSandbox(from, requireWrite: true);
        var fullTo = ResolveWithSandbox(to, requireWrite: true);
        var dir = Path.GetDirectoryName(fullTo);
        if (!string.IsNullOrEmpty(dir) && !Directory.Exists(dir)) Directory.CreateDirectory(dir);
        if (File.Exists(fullFrom)) File.Move(fullFrom, fullTo);
        else if (Directory.Exists(fullFrom)) Directory.Move(fullFrom, fullTo);
        else throw new FileNotFoundException($"Source not found: {from}");
        return JsonSerializer.Serialize(new { from, to, status = "moved" });
    }

    private static string Copy(string from, string to)
    {
        var fullFrom = ResolveWithSandbox(from, requireWrite: false);
        var fullTo = ResolveWithSandbox(to, requireWrite: true);
        var dir = Path.GetDirectoryName(fullTo);
        if (!string.IsNullOrEmpty(dir) && !Directory.Exists(dir)) Directory.CreateDirectory(dir);
        if (File.Exists(fullFrom)) File.Copy(fullFrom, fullTo, overwrite: true);
        else if (Directory.Exists(fullFrom)) CopyDir(fullFrom, fullTo);
        else throw new FileNotFoundException($"Source not found: {from}");
        return JsonSerializer.Serialize(new { from, to, status = "copied" });
    }

    private static string Delete(string path)
    {
        var full = ResolveWithSandbox(path, requireWrite: true);
        if (File.Exists(full)) File.Delete(full);
        else if (Directory.Exists(full)) Directory.Delete(full, recursive: true);
        else throw new FileNotFoundException($"Path not found: {path}");
        return JsonSerializer.Serialize(new { path, status = "deleted" });
    }

    private static string Metadata(string path)
    {
        var full = ResolveWithSandbox(path, requireWrite: false);
        if (File.Exists(full))
        {
            var fi = new FileInfo(full);
            return JsonSerializer.Serialize(new { path, type = "file", size = fi.Length, created = fi.CreationTimeUtc, modified = fi.LastWriteTimeUtc, attributes = fi.Attributes.ToString() });
        }
        if (Directory.Exists(full))
        {
            var di = new DirectoryInfo(full);
            return JsonSerializer.Serialize(new { path, type = "dir", created = di.CreationTimeUtc, modified = di.LastWriteTimeUtc, count = di.GetFileSystemInfos().Length });
        }
        throw new FileNotFoundException($"Path not found: {path}");
    }

    private static string Hash(string path)
    {
        var full = ResolveWithSandbox(path, requireWrite: false);
        if (!File.Exists(full)) throw new FileNotFoundException($"File not found: {path}");
        using var sha = SHA256.Create();
        using var fs = File.OpenRead(full);
        var hash = sha.ComputeHash(fs);
        var hex = Convert.ToHexString(hash).ToLowerInvariant();
        return JsonSerializer.Serialize(new { path, sha256 = hex, size = fs.Length });
    }

    private static string Diff(string path1, string path2)
    {
        var full1 = string.IsNullOrWhiteSpace(path1) ? "" : ResolveWithSandbox(path1, requireWrite: false);
        var full2 = string.IsNullOrWhiteSpace(path2) ? "" : ResolveWithSandbox(path2, requireWrite: false);
        if (!File.Exists(full1) || !File.Exists(full2))
            return JsonSerializer.Serialize(new { path1, path2, diff = "one or both files not found" });
        var t1 = File.ReadAllLines(full1);
        var t2 = File.ReadAllLines(full2);
        var diff = new System.Text.StringBuilder();
        int max = Math.Max(t1.Length, t2.Length);
        for (int i = 0; i < max; i++)
        {
            var a = i < t1.Length ? t1[i] : "<EOF>";
            var b = i < t2.Length ? t2[i] : "<EOF>";
            if (a != b) diff.AppendLine($"- {a}\n+ {b}");
        }
        return JsonSerializer.Serialize(new { path1, path2, diff = diff.ToString() });
    }

    private static void CopyDir(string src, string dst)
    {
        Directory.CreateDirectory(dst);
        foreach (var file in Directory.GetFiles(src))
            File.Copy(file, Path.Combine(dst, Path.GetFileName(file)), true);
        foreach (var dir in Directory.GetDirectories(src))
            CopyDir(dir, Path.Combine(dst, Path.GetFileName(dir)));
    }

    // Sandbox: solo WORKSPACE (repo root + work/) è scrivibile; /etc, ~/.ssh, ~/.aws, etc. DENY
    private static string ResolveWithSandbox(string path, bool requireWrite)
    {
        if (string.IsNullOrWhiteSpace(path)) throw new ArgumentException("path required");
        // Expand ~ and relative
        var expanded = path.Trim();
        if (expanded.StartsWith("~/")) expanded = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), expanded[2..]);
        else if (expanded == "~") expanded = Environment.GetFolderPath(Environment.SpecialFolder.UserProfile);
        // Risolvi relativo alla cartella del programma entry-point (work/ locale accanto al .bas, AGENTS.md §9, §11.1)
        // MAI bin/ del compilatore — EntrySource.Dir è la cartella del .bas (impostata da IlRuntime.SetEntrySourcePath → CommonLib.EntrySource)
        var baseDir = CommonLib.EntrySource.Dir;
        var full = Path.IsPathRooted(expanded) ? Path.GetFullPath(expanded) : Path.GetFullPath(Path.Combine(baseDir, expanded));
        var normalized = full.Replace('\\', '/');

        // DENY list (SYSTEM.txt §10)
        var denyPrefixes = new[]
        {
            "/etc/", "/etc",
            "/private/etc/",
            Environment.GetFolderPath(Environment.SpecialFolder.UserProfile) + "/.ssh",
            Environment.GetFolderPath(Environment.SpecialFolder.UserProfile) + "/.aws",
            "/root/.ssh",
            "/var/root/.ssh"
        };
        foreach (var deny in denyPrefixes)
        {
            var d = deny.Replace('\\', '/').TrimEnd('/') + "/";
            if (normalized.StartsWith(d, StringComparison.OrdinalIgnoreCase) || normalized.Equals(d.TrimEnd('/'), StringComparison.OrdinalIgnoreCase))
                throw new UnauthorizedAccessException($"Sandbox DENY: path '{path}' is blocked ({deny})");
        }
        // Se è dentro la repo root o work/, è OK per write; altrimenti per read è OK solo se non in deny e se requireWrite==false
        // Per F1, permettiamo read ovunque tranne deny; write solo dentro cwd/work o repo root
        if (requireWrite)
        {
            var repoRoot = CommonLib.GmbiPaths.RepoRoot.Replace('\\', '/');
            var baseNorm = baseDir.Replace('\\', '/');
            bool inRepo = normalized.StartsWith(repoRoot, StringComparison.OrdinalIgnoreCase);
            bool inCwd = normalized.StartsWith(baseNorm, StringComparison.OrdinalIgnoreCase);
            // Permetti anche /tmp per test
            bool inTmp = normalized.StartsWith("/tmp/", StringComparison.OrdinalIgnoreCase) || normalized.StartsWith(Path.GetTempPath().Replace('\\','/'), StringComparison.OrdinalIgnoreCase);
            if (!inRepo && !inCwd && !inTmp)
                throw new UnauthorizedAccessException($"Sandbox: write to '{path}' denied — allowed: repo root, work/, /tmp");
        }
        return full;
    }

    private static string MakeRelative(string full)
    {
        try
        {
            var baseDir = CommonLib.EntrySource.Dir.Replace('\\', '/');
            var f = full.Replace('\\', '/');
            if (f.StartsWith(baseDir, StringComparison.OrdinalIgnoreCase))
                return f[baseDir.Length..].TrimStart('/');
            var repo = CommonLib.GmbiPaths.RepoRoot.Replace('\\', '/');
            if (f.StartsWith(repo, StringComparison.OrdinalIgnoreCase))
                return f[repo.Length..].TrimStart('/');
            return f;
        }
        catch { return full; }
    }

    private static Dictionary<string, string> TryParseArgs(string json)
    {
        var dict = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
        if (string.IsNullOrWhiteSpace(json)) return dict;
        try
        {
            using var doc = JsonDocument.Parse(json);
            if (doc.RootElement.ValueKind != JsonValueKind.Object) return dict;
            foreach (var prop in doc.RootElement.EnumerateObject())
                dict[prop.Name] = prop.Value.ValueKind == JsonValueKind.String ? prop.Value.GetString() ?? "" : prop.Value.GetRawText();
        }
        catch { }
        return dict;
    }

    private static string? GetArg(Dictionary<string, string> d, string key) => d.TryGetValue(key, out var v) ? v : null;
}
