// FILE: AI.Tests.ModelScannerTests
using Basic.ModelDownload;
using FluentAssertions;

namespace AI.Tests;

[Trait(AITestCategories.Name, AITestCategories.Fast)]
public sealed class ModelScannerTests : IDisposable
{
    private readonly string _root = Path.Combine(Path.GetTempPath(), "gmbasic-scanner-test-" + Guid.NewGuid().ToString("N"));

    public ModelScannerTests() => Directory.CreateDirectory(_root);
    public void Dispose() { if (Directory.Exists(_root)) Directory.Delete(_root, recursive: true); }

    [Fact(DisplayName = "Detects a flat .gguf file directly under an AIPATH folder")]
    public void ScanAll_FlatGguf_DetectedAsGguf()
    {
        var ggufDir = Path.Combine(_root, "gguf");
        Directory.CreateDirectory(ggufDir);
        File.WriteAllBytes(Path.Combine(ggufDir, "tinyllama-q4.gguf"), [1, 2, 3]);

        var models = ModelScanner.ScanAll([ggufDir]);

        models.Should().ContainSingle(m => m.Type == DetectedModelType.Gguf && m.Name == "tinyllama-q4");
    }

    [Fact(DisplayName = "Detects an MLX repo folder (config.json + .safetensors) and does not recurse into its shards")]
    public void ScanAll_MlxRepoFolder_DetectedAsMlxNotRecursed()
    {
        var mlxDir = Path.Combine(_root, "mlx", "Qwen2.5-0.5B-Instruct-4bit");
        Directory.CreateDirectory(mlxDir);
        File.WriteAllText(Path.Combine(mlxDir, "config.json"), "{}");
        File.WriteAllBytes(Path.Combine(mlxDir, "model.safetensors"), [1, 2, 3]);

        var models = ModelScanner.ScanAll([Path.Combine(_root, "mlx")]);

        models.Should().ContainSingle();
        models[0].Type.Should().Be(DetectedModelType.Mlx);
        models[0].Name.Should().Be("Qwen2.5-0.5B-Instruct-4bit");
    }

    [Fact(DisplayName = "Detects Ollama store layout (manifests/+blobs/) and reads model:tag from manifest paths")]
    public void ScanAll_OllamaStore_DetectedAsOllamaWithTag()
    {
        var manifestDir = Path.Combine(_root, "ollama", "manifests", "registry.ollama.ai", "library", "llama3");
        Directory.CreateDirectory(manifestDir);
        Directory.CreateDirectory(Path.Combine(_root, "ollama", "blobs"));
        File.WriteAllText(Path.Combine(manifestDir, "latest"), "{}");

        var models = ModelScanner.ScanAll([Path.Combine(_root, "ollama")]);

        models.Should().ContainSingle(m => m.Type == DetectedModelType.Ollama && m.Name == "llama3:latest");
    }

    [Fact(DisplayName = "Nonexistent AIPATH entries are skipped without throwing")]
    public void ScanAll_NonexistentRoot_ReturnsEmptyWithoutThrowing()
    {
        var act = () => ModelScanner.ScanAll([Path.Combine(_root, "does-not-exist")]);
        act.Should().NotThrow();
        act().Should().BeEmpty();
    }

    [Fact(DisplayName = "DetectConfig: un .gguf non gestito dal motore MLX resta a llama.cpp (fallback)")]
    public void DetectConfig_GgufNotMlxSupported_FallsBackToLlamaCpp()
    {
        // File .gguf con contenuto non valido: llm_ai_mlx_gguf_supported ritorna 0 (o la lib
        // nativa non è disponibile) → routing di default a llama.cpp, nessuna eccezione.
        var gguf = Path.Combine(_root, "not-a-real-model.gguf");
        File.WriteAllBytes(gguf, [1, 2, 3, 4]);

        var cfg = ModelScanner.DetectConfig(gguf);

        cfg.Kind.Should().Be("llamacpp");
        cfg.Config["modelPath"].Should().Be(gguf);
    }

    [Fact(DisplayName = "'~' in a search root expands to the home directory")]
    public void ScanAll_TildeRoot_Expands()
    {
        var expanded = PathExpansion.Expand("~/Library");
        expanded.Should().Be(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile), "Library"));
        expanded.Should().NotStartWith("~");
    }
}
