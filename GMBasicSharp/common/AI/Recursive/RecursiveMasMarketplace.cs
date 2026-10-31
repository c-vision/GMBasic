// FILE: Basic.Agents.Recursive.RecursiveMasMarketplace
// Marketplace HF (collezioni RecursiveMAS, sez. 8) — push/pull dei checkpoint del team
// (`.team.json` + `.team.json.links.json`) tra la cartella locale
// `<configDir>/recursivemas/<team>/` e un repo HuggingFace (`recursivemas/<team>/...`).
// Protocollo GIT (affidabile: clone + add + commit + push; niente endpoint HTTP deprecati).
// Token: env `HF_TOKEN`/`HUGGINGFACE_TOKEN`, altrimenti il file scratch dell'utente
// (documentato, NON versionato).

using System.Diagnostics;
using System.Net.Http.Headers;
using CommonLib;

namespace Basic.Agents.Recursive;

public static class RecursiveMasMarketplace
{
    private const string Hub = "https://huggingface.co";

    /// <summary>Token HF: configurazione AI (`AI SET HFTOKEN` / ai.json `hfToken`), poi env
    /// `HF_TOKEN`/`HUGGINGFACE_TOKEN`, poi il file scratch (Rider: .../AGENTS/HUGGINGFACE.txt).</summary>
    public static string? ResolveToken(string? token = null)
    {
        if (!string.IsNullOrWhiteSpace(token)) return token;
        var cfg = Basic.AI.AiConfig.HfToken;
        if (!string.IsNullOrWhiteSpace(cfg)) return cfg;
        var env = Environment.GetEnvironmentVariable("HUGGINGFACE_TOKEN")
               ?? Environment.GetEnvironmentVariable("HF_TOKEN");
        if (!string.IsNullOrWhiteSpace(env)) return env;
        var scratch = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
            "JetBrains", "Rider2026.2", "scratches", "AI", "AGENTS", "HUGGINGFACE.txt");
        if (File.Exists(scratch))
            foreach (var line in File.ReadAllLines(scratch))
            {
                var t = line.Trim();
                if (t.StartsWith("hf_", StringComparison.Ordinal)) return t;
            }
        return null;
    }

    /// <summary>Repo HF di default: configurazione AI (`AI SET HFREPO` / ai.json `hfRepo`).</summary>
    public static string? DefaultRepo() => Basic.AI.AiConfig.HfRepo;

    private static string Sanitize(string team) => new(team.Where(char.IsLetterOrDigit).ToArray());

    private static string TeamDir(string team)
    {
        var safe = Sanitize(team);
        if (safe.Length == 0) safe = "team";
        return Path.Combine(GmbiPaths.RecursiveMasDir, safe);
    }

    /// <summary>Namespace HF del token (whoami) per risolvere i repo senza '/' .</summary>
    public static string? ResolveRepo(string repoId, string? token = null)
    {
        if (string.IsNullOrWhiteSpace(repoId)) return null;
        if (repoId.Contains('/')) return repoId;
        var tok = ResolveToken(token);
        if (tok is null) return null;
        using var client = new HttpClient { BaseAddress = new Uri(Hub + "/api/") };
        client.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", tok);
        using var resp = client.GetAsync("whoami-v2").GetAwaiter().GetResult();
        if (!resp.IsSuccessStatusCode) return null;
        using var doc = System.Text.Json.JsonDocument.Parse(resp.Content.ReadAsStringAsync().GetAwaiter().GetResult());
        return doc.RootElement.TryGetProperty("name", out var n) ? $"{n.GetString()}/{repoId}" : null;
    }

    /// <summary>Push dei checkpoint del team su HF (git clone + add + commit + push).
    /// Il repo di default è la configurazione `hfRepo` (AI SET HFREPO) se `repoId` è vuoto.</summary>
    public static bool Push(string team, string repoId, string? token = null)
    {
        var tok = ResolveToken(token) ?? throw new InvalidOperationException(
            "Token HF mancante: imposta HF_TOKEN/HUGGINGFACE_TOKEN, AI SET HFTOKEN o crea il file HUGGINGFACE.txt.");
        if (string.IsNullOrWhiteSpace(repoId)) repoId = DefaultRepo() ?? throw new InvalidOperationException("Repo HF mancante: imposta AI SET HFREPO.");
        var full = ResolveRepo(repoId, tok) ?? repoId;
        var safe = Sanitize(team);
        var localDir = TeamDir(team);
        if (!Directory.Exists(localDir)) return false;
        var files = Directory.GetFiles(localDir, "*.json");
        if (files.Length == 0) return false;

        var work = Path.Combine(Path.GetTempPath(), "gmb-hf-" + Guid.NewGuid().ToString("N"));
        try
        {
            if (!Git(null, "clone", $"https://user:{tok}@huggingface.co/{full}", work)) return false;
            var dest = Path.Combine(work, "recursivemas", safe);
            Directory.CreateDirectory(dest);
            foreach (var f in files) File.Copy(f, Path.Combine(dest, Path.GetFileName(f)), overwrite: true);
            if (!Git(work, "add", "-A")) return false;
            if (!Git(work, "-c", "user.email=checkpoint@gmbasic.local", "-c", "user.name=GMBasic",
                "commit", "-m", $"checkpoint {safe}", "--allow-empty")) return false;
            return Git(work, "push");
        }
        finally
        {
            if (Directory.Exists(work)) Directory.Delete(work, recursive: true);
        }
    }

    /// <summary>Pull dei checkpoint del team da HF nella cartella locale.
    /// Il repo di default è la configurazione `hfRepo` (AI SET HFREPO) se `repoId` è vuoto.</summary>
    public static bool Pull(string team, string repoId, string? token = null)
    {
        var tok = ResolveToken(token);
        if (string.IsNullOrWhiteSpace(repoId)) repoId = DefaultRepo() ?? "";
        var full = ResolveRepo(repoId, tok) ?? repoId;
        var safe = Sanitize(team);
        var dir = TeamDir(team);
        Directory.CreateDirectory(dir);
        var ok = false;
        foreach (var name in new[] { $"{safe}.team.json", $"{safe}.team.json.links.json" })
        {
            var remote = $"recursivemas/{safe}/{name}";
            var bytes = DownloadFile(full, remote, tok);
            if (bytes is not null) { File.WriteAllBytes(Path.Combine(dir, name), bytes); ok = true; }
        }
        return ok;
    }

    private static byte[]? DownloadFile(string repoId, string remotePath, string? token)
    {
        using var client = new HttpClient();
        if (!string.IsNullOrWhiteSpace(token))
            client.DefaultRequestHeaders.Authorization = new AuthenticationHeaderValue("Bearer", token);
        using var resp = client.GetAsync($"{Hub}/{repoId}/resolve/main/{remotePath}").GetAwaiter().GetResult();
        return resp.IsSuccessStatusCode ? resp.Content.ReadAsByteArrayAsync().GetAwaiter().GetResult() : null;
    }

    private static bool Git(string? cwd, params string[] args)
    {
        var psi = new ProcessStartInfo("git") { RedirectStandardOutput = true, RedirectStandardError = true };
        if (cwd is not null) psi.WorkingDirectory = cwd;
        foreach (var a in args) psi.ArgumentList.Add(a);
        using var p = Process.Start(psi);
        if (p is null) return false;
        p.StandardOutput.ReadToEnd();
        p.StandardError.ReadToEnd();
        p.WaitForExit(120000);
        return p.ExitCode == 0;
    }
}
