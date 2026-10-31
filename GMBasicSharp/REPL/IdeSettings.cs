// FILE: Basic.Executor.IdeSettings.cs

using System.IO;
using System.Text.Json;
using System.Text.Json.Serialization;
using CommonLib;

namespace Basic.Executor;

/// <summary>
/// Impostazioni IDE (Fase 6 — IDE-TUI-TODO 6.2). Salvate in JSON su
/// <c>config/ide.settings.json</c> (GmbiPaths — configDir configurabile in
/// system.settings.json sezione "paths"). Caricate al boot e applicate al volo
/// dal dialog Options (Opts → Options...).
/// </summary>
public sealed class IdeSettings
{
    [JsonPropertyName("tabWidth")]
    public int TabWidth { get; set; } = 2;

    [JsonPropertyName("showLineNumbers")]
    public bool ShowLineNumbers { get; set; } = true;

    [JsonPropertyName("autoSaveInterval")]
    public int AutoSaveInterval { get; set; } = 0;              // 0 = off, secondi

    [JsonPropertyName("backupOnSave")]
    public bool BackupOnSave { get; set; } = true;

    [JsonPropertyName("syntaxHighlighting")]
    public bool SyntaxHighlighting { get; set; } = true;

    [JsonPropertyName("theme")]
    public string Theme { get; set; } = "qbasic";               // "qbasic" | "dark" | "light"

    [JsonPropertyName("fontSize")]
    public int FontSize { get; set; } = 13;

    [JsonPropertyName("showOutputPanel")]
    public bool ShowOutputPanel { get; set; } = true;

    [JsonPropertyName("showWatchPanel")]
    public bool ShowWatchPanel { get; set; } = false;

    [JsonPropertyName("immediateWindow")]
    public bool ImmediateWindow { get; set; } = false;

    public static string DefaultPath => GmbiPaths.IdeSettingsFile;

    public static IdeSettings Load()
    {
        try
        {
            var path = DefaultPath;
            if (!File.Exists(path)) return new IdeSettings();
            var json = File.ReadAllText(path);
            return JsonSerializer.Deserialize<IdeSettings>(json) ?? new IdeSettings();
        }
        catch
        {
            return new IdeSettings();
        }
    }

    public bool Save()
    {
        try
        {
            var path = DefaultPath;
            Directory.CreateDirectory(Path.GetDirectoryName(path)!);
            var json = JsonSerializer.Serialize(this, new JsonSerializerOptions { WriteIndented = true });
            File.WriteAllText(path, json);
            return true;
        }
        catch
        {
            return false;
        }
    }
}
