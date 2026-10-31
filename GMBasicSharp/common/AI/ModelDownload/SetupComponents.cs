// AI/ModelDownload/SetupComponents.cs — REGISTRO UNIFICATO dei componenti scaricabili
// on-demand (comando SETUP, sostituisce ML e BROWSER). Ogni componente ha il proprio
// manifesto in Resources/ (torch: ml-manifest.json, chromium: chromium-manifest.json,
// OCR: ocr-manifest.json) e il proprio Fetch*; qui si normalizzano risultati e cache.
//   gmb setup TORCH|BROWSER|OCR|ALL  ·  gmb setup status  ·  gmb setup rm <comp|ALL>

using System;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;

namespace Basic.AI.ModelDownload;
using Basic.ModelDownload;

/// <summary>Risultato normalizzato di un setup componente (stessi campi di MlSetupResult/
/// ChromiumSetupResult/OcrSetupResult — unificati qui per il comando SETUP).</summary>
public sealed record SetupResult(bool Success, string? Error, string CacheDir, long BytesTotal, IReadOnlyList<string> Files);

/// <summary>Un componente scaricabile on-demand.</summary>
public sealed class SetupComponent
{
    public required string Name { get; init; }          // TORCH / BROWSER / OCR
    public required string Description { get; init; }   // cosa scarica (help)
    public required Func<string> CacheDir { get; init; }     // per il RID corrente
    public required Func<string> ManifestPath { get; init; }
    public required string ReadyNote { get; init; }          // cosa abilita dopo il setup
    public required Func<string, bool> IsCached { get; init; }
    public required Func<Action<string>?, Action<long, long>?, CancellationToken, Task<SetupResult>> Setup { get; init; }
}

/// <summary>Registry unificato: TORCH, BROWSER, OCR (+ ALL).</summary>
public static class SetupComponents
{
    public static readonly SetupComponent[] All =
    {
        new()
        {
            Name = "LIBS",
            Description = "librerie native on-demand (support+core+engine) in ~/.gmbasic/libs/<rid>/",
            CacheDir = () => CommonLib.LibsResolver.CacheDir(CommonLib.LibsResolver.Rid()),
            ManifestPath = () => CommonLib.LibsResolver.LocalManifestPath,
            ReadyNote = "librerie native (SDL, pdf_oxide, OCR, motori AI, lib gmb_*) pronte.",
            IsCached = rid =>
            {
                try
                {
                    var groups = CommonLib.LibsResolver.BaseGroups;
                    var st = CommonLib.LibsResolver.CheckAsync(groups, rid).GetAwaiter().GetResult();
                    return st.Count > 0 && st.All(x => x.Status == CommonLib.LibsFileStatus.Cached);
                }
                catch { return false; }
            },
            Setup = async (log, prog, ct) =>
            {
                var rid = CommonLib.LibsResolver.Rid();
                var groups = CommonLib.LibsResolver.BaseGroups;
                var total = 0;
                var files = new List<string>();
                foreach (var g in groups)
                {
                    try
                    {
                        total += await CommonLib.LibsResolver.EnsureGroupAsync(g, rid, log, null, ct).ConfigureAwait(false);
                        files.Add(g);
                    }
                    catch (Exception ex)
                    {
                        return new SetupResult(false, $"gruppo '{g}': {ex.Message}",
                                               CommonLib.LibsResolver.CacheDir(rid), total, files);
                    }
                }
                return new SetupResult(true, null, CommonLib.LibsResolver.CacheDir(rid), total, files);
            },
        },
        new()
        {
            Name = "TORCH",
            Description = "librerie runtime ML (host libgmb_torch + libtorch CPU)",
            CacheDir = () => MlCache.Dir(MlCache.ResolveRid()),
            ManifestPath = FetchTorchLib.DefaultManifestPath,
            ReadyNote = "TENSOR.*/TRAIN.* pronti.",
            IsCached = FetchTorchLib.IsCached,
            Setup = async (log, prog, ct) =>
            {
                var r = await FetchTorchLib.EnsureAsync(FetchTorchLib.DefaultManifestPath(), log,
                    prog == null ? null : (Action<ModelDownloader.DownloadProgress>)(p => prog(p.BytesDone, p.BytesTotal)), ct);
                return new SetupResult(r.Success, r.Error, r.CacheDir, r.BytesTotal, r.Files);
            },
        },
        new()
        {
            Name = "BROWSER",
            Description = "Chromium (Chrome for Testing) per il plugin browser",
            CacheDir = () => ChromiumCache.Dir(ChromiumCache.ResolveRid()),
            ManifestPath = FetchChromiumLib.DefaultManifestPath,
            ReadyNote = "plugin browser (naviga/screenshot/evaluate) pronto.",
            IsCached = FetchChromiumLib.IsCached,
            Setup = async (log, prog, ct) =>
            {
                var r = await FetchChromiumLib.EnsureAsync(FetchChromiumLib.DefaultManifestPath(), log, prog, ct);
                return new SetupResult(r.Success, r.Error, r.CacheDir, r.BytesTotal, r.Files);
            },
        },
        new()
        {
            Name = "OBSCURA",
            Description = "Obscura (Rust+V8, ~70MB/~30MB) — browser headless per agenti via CDP (alternativa leggera a Chromium, CUA §10)",
            CacheDir = () => ObscuraCache.Dir(ObscuraCache.ResolveRid()),
            ManifestPath = FetchObscuraLib.DefaultManifestPath,
            ReadyNote = "BrowserProvider obscura pronto (CDP, navigate/snapshot/click/fill).",
            IsCached = FetchObscuraLib.IsCached,
            Setup = async (log, prog, ct) =>
            {
                var r = await FetchObscuraLib.EnsureAsync(FetchObscuraLib.DefaultManifestPath(), log, prog, ct);
                return new SetupResult(r.Success, r.Error, r.CacheDir, r.BytesTotal, r.Files);
            },
        },
        // OCR (Tesseract) NON è un componente separato: i file (libtesseract, libleptonica,
        // tessdata/eng) sono inclusi nel bundle LIBS di Resources/libs/<os>/ (gmb setup LIBS).
        // La funzione gmb_doc_ocr.c trova tesseract in Resources/libs/<os>/.
    };

    /// <summary>Trova un componente per nome (case-insensitive).</summary>
    public static SetupComponent? Find(string name) =>
        All.FirstOrDefault(c => c.Name.Equals(name, StringComparison.OrdinalIgnoreCase));
}
