// AI/ModelDownload/ModelScanner.cs — GGUF/MLX/Ollama type-detection for the REPL LLM command
using Basic.AI.Rag;
using Basic.Providers;

namespace Basic.ModelDownload;

public enum DetectedModelType { Gguf, Mlx, Ollama }

public sealed record DetectedModel(DetectedModelType Type, string Name, string Path);

/// <summary>Result of an auto-detected model source (path or scheme): il tipo di trasporto,
/// la capability (chat/ocr/audio), e la configurazione completa per registrare il provider.
/// Usato da PROVIDER.REGISTER per costruire il provider senza JSON a mano.</summary>
public sealed record DetectedModelConfig(
    AiCapability Capability,
    string Kind,
    IReadOnlyDictionary<string, string> Config);

/// <summary>Finds locally-present models under a set of search roots (typically AIPATH) by
/// inspecting folder contents -- never a declared type per path. GGUF: any *.gguf file. MLX:
/// a folder with config.json + at least one *.safetensors. Ollama: a folder shaped like
/// Ollama's own on-disk store (manifests/ + blobs/), walked for its manifest tags rather than
/// downloaded (Ollama manages its own store; we only ever read it).</summary>
public static class ModelScanner
{
    private const int MaxDepth = 3;

    /// <summary>Detects the type of a single path (not a search root) -- used by
    /// PROVIDER.REGISTER to pick which provider 'kind' and config key a given file/folder
    /// needs, without duplicating the GGUF/MLX rules <see cref="ScanAll"/> uses. Ollama's store
    /// layout is deliberately not handled here -- it's a running server, not a loadable
    /// file/folder a provider can point at directly. Returns null if the path doesn't exist or
    /// matches neither shape.</summary>
    public static DetectedModel? DetectSingle(string path)
    {
        var expanded = PathExpansion.Expand(path);
        if (File.Exists(expanded) && expanded.EndsWith(".gguf", StringComparison.OrdinalIgnoreCase))
            return new DetectedModel(DetectedModelType.Gguf, Path.GetFileNameWithoutExtension(expanded), expanded);
        if (Directory.Exists(expanded) && IsMlxRepoDir(expanded))
            return new DetectedModel(DetectedModelType.Mlx, new DirectoryInfo(expanded).Name, expanded);
        return null;
    }

    /// <summary>Auto-detect completo di un path locale: capability + kind + config provider.
    /// Rileva il mmproj (projector visione/audio) accanto al GGUF -- convenzione di naming:
    /// il mmproj contiene nel nome "PaddleOCR"/"mmproj" per OCR e "Qwen2.5-Omni"/"Omni"/"mmproj"
    /// per audio. Se un GGUF ha un mmproj che non si riesce a classificare, si assume OCR
    /// (fallback sicuro: PaddleOCR è il caso più comune) ma la capability resta dichiarabile.</summary>
    public static DetectedModelConfig DetectConfig(string path, AiCapability? declaredCapability = null)
    {
        var expanded = PathExpansion.Expand(path);
        if (File.Exists(expanded) && expanded.EndsWith(".gguf", StringComparison.OrdinalIgnoreCase))
        {
            var mmproj = FindMmproj(expanded);
            var capability = declaredCapability
                ?? (mmproj is null ? AiCapability.Chat : ClassifyMmproj(mmproj));
            // Modello GGUF la cui architettura è gestita DIRETTAMENTE dal motore MLX embedded
            // (oggi DS4/DeepSeek V4 Flash): il BASIC lo carica col motore MLX, non con llama.cpp.
            // Solo chat e senza mmproj (DS4 è testo; i multimodali restano a llama.cpp).
            if (capability == AiCapability.Chat && mmproj is null && MlxEngineSupportsGguf(expanded))
                return new DetectedModelConfig(AiCapability.Chat, "mlx",
                    new Dictionary<string, string> { ["modelDir"] = expanded });
            var kind = capability switch
            {
                AiCapability.Ocr => "llamacpp-vision",
                AiCapability.Audio => "llamacpp-audio",
                _ => "llamacpp",
            };
            var config = new Dictionary<string, string> { ["modelPath"] = expanded };
            if (mmproj is not null)
            {
                config["mmprojPath"] = mmproj;
                var gm = EmitGmmod(expanded, mmproj);
                if (gm is not null) config["gmmodPath"] = gm;
            }
            return new DetectedModelConfig(capability, kind, config);
        }
        if (Directory.Exists(expanded))
        {
            if (IsMlxRepoDir(expanded))
                return new DetectedModelConfig(AiCapability.Chat, "mlx",
                    new Dictionary<string, string> { ["modelDir"] = expanded });

            // Cartella che contiene GGUF scaricati (es. samples/ai/models/gguf/): il GGUF
            // principale è quello che ha accanto un mmproj, se presente; altrimenti un GGUF
            // qualunque. Usato dopo un download hf: che scrive i file direttamente in Dest.
            var ggufFiles = SafeGetFiles(expanded, "*.gguf").ToList();
            if (ggufFiles.Count > 0)
            {
                var main = ggufFiles.FirstOrDefault(f => FindMmproj(f) is not null) ?? ggufFiles[0];
                var mmproj = FindMmproj(main);
                var capability = declaredCapability
                    ?? (mmproj is null ? AiCapability.Chat : ClassifyMmproj(mmproj));
                if (capability == AiCapability.Chat && mmproj is null && MlxEngineSupportsGguf(main))
                    return new DetectedModelConfig(AiCapability.Chat, "mlx",
                        new Dictionary<string, string> { ["modelDir"] = main });
                var kind = capability switch
                {
                    AiCapability.Ocr => "llamacpp-vision",
                    AiCapability.Audio => "llamacpp-audio",
                    _ => "llamacpp",
                };
                var config = new Dictionary<string, string> { ["modelPath"] = main };
                if (mmproj is not null)
                {
                    config["mmprojPath"] = mmproj;
                    var gm = EmitGmmod(main, mmproj);
                    if (gm is not null) config["gmmodPath"] = gm;
                }
                return new DetectedModelConfig(capability, kind, config);
            }
        }
        throw new InvalidOperationException(
            $"'{path}' non e' un file .gguf ne' una cartella repo MLX (config.json + almeno un .safetensors).");
    }

    /// <summary>True se l'architettura del GGUF è gestita DIRETTAMENTE dal motore MLX embedded
    /// (LLMEnv/LLMEngine/mlx, ARCH_TABLE — oggi solo DS4/DeepSeek V4 Flash, arch "deepseek4").
    /// Auto-route del REGISTER: questi .gguf vanno al motore MLX, tutti gli altri a llama.cpp.
    /// Se la libreria nativa non è disponibile si ricade su llama.cpp (mai un'eccezione: è solo
    /// una scelta di routing).</summary>
    private static bool MlxEngineSupportsGguf(string ggufPath)
    {
        try { return MlxEngineNative.llm_ai_mlx_gguf_supported(ggufPath) == 1; }
        catch { return false; }
    }

    /// <summary>Cerca il file mmproj (projector visione/audio) accanto al GGUF: convention di
    /// naming `*mmproj*.gguf` (es. PaddleOCR-VL-1.6-GGUF-mmproj.gguf, mmproj-Qwen2.5-Omni...).
    /// Il path è passato già espanso.</summary>
    private static string? FindMmproj(string ggufPath)
    {
        var dir = Path.GetDirectoryName(ggufPath);
        if (string.IsNullOrEmpty(dir)) return null;
        try
        {
            var baseName = Path.GetFileNameWithoutExtension(ggufPath);
            string? fallback = null;
            foreach (var f in Directory.GetFiles(dir, "*.gguf", SearchOption.TopDirectoryOnly))
            {
                if (string.Equals(f, ggufPath, StringComparison.OrdinalIgnoreCase)) continue;
                var name = Path.GetFileName(f);
                if (!name.Contains("mmproj", StringComparison.OrdinalIgnoreCase)) continue;
                // Preferisci il mmproj con lo STESSO prefisso del modello (es. PaddleOCR-VL-1.6-GGUF-mmproj
                // accanto a PaddleOCR-VL-1.6-GGUF). Fallback: primo "mmproj" della cartella.
                var modelStem = baseName;
                if (modelStem.EndsWith("-GGUF", StringComparison.OrdinalIgnoreCase))
                    modelStem = modelStem[..^5];
                else if (modelStem.EndsWith(".gguf", StringComparison.OrdinalIgnoreCase))
                    modelStem = modelStem[..^5];
                if (name.StartsWith(modelStem, StringComparison.OrdinalIgnoreCase))
                    return f;
                fallback ??= f;
            }
            return fallback;
        }
        catch { /* best effort */ }
        return null;
    }

    /// <summary>Classifica la capability dal nome del mmproj: "Omni"/"Audio" -> audio;
    /// altrimenti OCR (fallback). Basata sulla convention dei modelli scaricati (PaddleOCR-VL,
    /// Qwen2.5-Omni).</summary>
    private static AiCapability ClassifyMmproj(string mmprojPath)
    {
        // Prima la modality DICHIARATA (ABI/.llmarch o .gmmod via llmmodelio_model_modality_str):
        // la dichiarazione vince sulla convention di naming. Se non risolta -> fallback naming.
        try
        {
            var declared = LlmModelNative.ModalityOf(mmprojPath);
            if (declared == "audio") return AiCapability.Audio;
            if (declared is "vision" or "ocr" or "omni") return AiCapability.Ocr;
        }
        catch { /* best effort: fallback alla convention di naming */ }
        var name = Path.GetFileName(mmprojPath);
        if (name.Contains("omni", StringComparison.OrdinalIgnoreCase)
            || name.Contains("audio", StringComparison.OrdinalIgnoreCase)
            || name.Contains("whisper", StringComparison.OrdinalIgnoreCase)
            || name.Contains("parakeet", StringComparison.OrdinalIgnoreCase))
            return AiCapability.Audio;
        return AiCapability.Ocr; // default: visione
    }

    /// <summary>Emette il descriptor `.gmmod` (&lt;modelStem&gt;.gmmod) da modello+mmproj REALI
    /// (modality dall'ABI via llmmodelio_model_modality). Best-effort: null se non risolvibile.
    /// Rende la dichiarazione PERSISTENTE su disco, così loader/provider/registry la consumano
    /// invece di ri-dedurla dal naming.</summary>
    private static string? EmitGmmod(string modelPath, string mmprojPath)
    {
        try
        {
            var gm = Path.ChangeExtension(modelPath, ".gmmod");
            if (LlmModelNative.llmmodelio_model_modality(modelPath, mmprojPath, gm) == 0) return gm;
        }
        catch { /* best effort */ }
        return null;
    }

    public static List<DetectedModel> ScanAll(IEnumerable<string> roots)
    {
        var results = new List<DetectedModel>();
        var seen = new HashSet<string>(StringComparer.Ordinal);
        foreach (var root in roots)
        {
            var expanded = PathExpansion.Expand(root);
            if (string.IsNullOrWhiteSpace(expanded) || !Directory.Exists(expanded)) continue;
            if (!seen.Add(Path.GetFullPath(expanded))) continue; // dedup if two roots resolve to the same folder
            ScanRoot(expanded, results);
        }
        return results;
    }

    private static void ScanRoot(string root, List<DetectedModel> results)
    {
        if (IsOllamaStore(root)) { results.AddRange(ScanOllama(root)); return; }
        ScanDir(root, results, depth: 0);
    }

    private static void ScanDir(string dir, List<DetectedModel> results, int depth)
    {
        if (depth > MaxDepth) return;

        foreach (var f in SafeGetFiles(dir, "*.gguf"))
            results.Add(new DetectedModel(DetectedModelType.Gguf, Path.GetFileNameWithoutExtension(f), f));

        if (IsMlxRepoDir(dir))
        {
            results.Add(new DetectedModel(DetectedModelType.Mlx, new DirectoryInfo(dir).Name, dir));
            return; // a recognized MLX repo folder is a leaf -- don't recurse into its shard files
        }

        foreach (var sub in SafeGetDirectories(dir))
            ScanDir(sub, results, depth + 1);
    }

    private static bool IsMlxRepoDir(string dir) =>
        File.Exists(Path.Combine(dir, "config.json")) && SafeGetFiles(dir, "*.safetensors").Any();

    private static bool IsOllamaStore(string dir) =>
        Directory.Exists(Path.Combine(dir, "manifests")) && Directory.Exists(Path.Combine(dir, "blobs"));

    private static IEnumerable<DetectedModel> ScanOllama(string root)
    {
        var manifestsRoot = Path.Combine(root, "manifests");
        if (!Directory.Exists(manifestsRoot)) yield break;

        foreach (var tagFile in SafeGetFilesRecursive(manifestsRoot))
        {
            var rel = Path.GetRelativePath(manifestsRoot, tagFile);
            var parts = rel.Split(Path.DirectorySeparatorChar);
            if (parts.Length < 2) continue; // need at least <model>/<tag>
            var tag = parts[^1];
            var model = parts[^2];
            yield return new DetectedModel(DetectedModelType.Ollama, $"{model}:{tag}", tagFile);
        }
    }

    private static IEnumerable<string> SafeGetFiles(string dir, string pattern)
    {
        try { return Directory.GetFiles(dir, pattern, SearchOption.TopDirectoryOnly); }
        catch { return []; }
    }

    private static IEnumerable<string> SafeGetDirectories(string dir)
    {
        try { return Directory.GetDirectories(dir); }
        catch { return []; }
    }

    private static IEnumerable<string> SafeGetFilesRecursive(string dir)
    {
        try { return Directory.GetFiles(dir, "*", SearchOption.AllDirectories); }
        catch { return []; }
    }
}
