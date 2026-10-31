// ProgramBundle.cs — il pacchetto MINIMO che rende eseguibile un programma compilato su una
// macchina SENZA compilatore.
//
// PERCHÉ. Un `.bas` compilato viene distribuito da solo: chi lo riceve non ha GMBasic installato,
// non ha il repo, non ha `shared/settings/`. Se l'eseguibile non porta con sé i file che gli
// servono, si avvia e non sa né leggere i settings né disegnare un font.
//
// COSA COPIA (accanto all'eseguibile):
//   app.settings.json  ← MERGE fra i settings condivisi e l'`app.settings.json` del programma,
//                        ridotto alle sole sezioni che il RUNTIME legge (vedi RuntimeSections).
//                        L'app vince chiave per chiave sulle chiavi comuni.
//   Resources/*        ← font (font.ttf, SFMono, C64) e sprite
//
// PERCHÉ NON SI DISTRIBUISCE `system.settings.json`. Quel file è il default GLOBALE del
// COMPILATORE e dell'app: contiene `compiler` (cc/cflags/ldflags), `importFolders`,
// `defaultNamespaces`, `libraryFolders`, `defaultProjectsFolder` — roba che serve a compilare,
// non a eseguire. Il compilato non deve portarsela dietro (e non deve poterla "ereditare" per
// sbaglio: se un domani cambiano i default, un binario vecchio non deve cambiare comportamento).
// Restano SOLO le sezioni che il runtime legge davvero: `paths`, `consoleConfiguration`, `engine`.
//
// COME LI TROVA IL RUNTIME: `pr_load` (C) e `GmbiPaths`/`GmbEnv` (C#) guardano la cartella
// dell'eseguibile per primi, quindi il programma distribuito usa i SUOI file.
//
// REGOLA: vale per OGNI modalità di compilazione — anche quella «senza librerie».

using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace CommonLib;

public static class ProgramBundle
{
    /// <summary>Le sole sezioni di settings lette a RUNTIME (verificate nel codice):
    /// `paths` (GmbiPaths / pr_load), `consoleConfiguration` (console/font: gmb_console.c,
    /// gmb_settings.c), `engine` (gmb_opts.c / GmbEnv). Tutto il resto è del compilatore.</summary>
    public static readonly string[] RuntimeSections = ["paths", "consoleConfiguration", "engine"];

    private static readonly JsonDocumentOptions ParseOpts = new()
    {
        CommentHandling = JsonCommentHandling.Skip,
        AllowTrailingCommas = true,
    };

    /// <summary>Scrive accanto all'eseguibile i settings minimi uniti e gli asset.
    /// Ritorna l'elenco dei file scritti/aggiornati (per il log del compilatore).</summary>
    public static IReadOnlyList<string> CopyNextTo(string outputDir, string basFile, Action<string>? log = null,
                                                   string? exePath = null)
    {
        var written = new List<string>();
        if (string.IsNullOrWhiteSpace(outputDir)) return written;
        Directory.CreateDirectory(outputDir);

        // app.settings.json DEL PROGRAMMA (serve ai settings minimi e ai cervelli dichiarati)
        string? appPath = null;
        try
        {
            var srcDir = Path.GetDirectoryName(Path.GetFullPath(basFile));
            if (!string.IsNullOrEmpty(srcDir)) appPath = Path.Combine(srcDir, "app.settings.json");
        }
        catch { /* percorso virtuale */ }

        // 1) Settings: shared/settings/system.settings.json ∩ (override) app.settings.json del
        //    programma → SOLO le sezioni di runtime, in un unico `app.settings.json`.
        try
        {
            var sysPath = Path.Combine(GmbiPaths.SharedConfigDir, "system.settings.json");
            var merged = BuildRuntimeSettings(sysPath, appPath);
            var dest = Path.Combine(outputDir, "app.settings.json");
            // Mai sovrascrivere l'ORIGINE (quando la cartella di output è quella del sorgente).
            if (merged != null && (appPath is null || !SamePath(dest, appPath)))
            {
                var json = merged.ToJsonString(new JsonSerializerOptions { WriteIndented = true });
                WriteIfChanged(dest, json, written, log);
            }
        }
        catch (Exception ex) { log?.Invoke($"[bundle] settings non generati: {ex.Message}"); }

        // 2) Resources/: font e sprite. Pochi file (688 KB), ma un eseguibile senza font non
        //    disegna nulla: si copia la cartella intera invece di indovinare cosa usa il programma.
        try
        {
            var resSrc = GmbiPaths.SharedResourcesDir;
            if (Directory.Exists(resSrc))
            {
                var resDst = Path.Combine(outputDir, "Resources");
                foreach (var f in Directory.EnumerateFiles(resSrc, "*", SearchOption.TopDirectoryOnly))
                {
                    var dst = Path.Combine(resDst, Path.GetFileName(f));
                    Directory.CreateDirectory(resDst);
                    CopyIfNewer(f, dst, written, log);
                }
            }
        }
        catch { /* niente asset condivisi: si compila comunque */ }

        // 3) CERVELLI. Il manifest (metadati, pochi KB) va accanto all'eseguibile in
        //    `settings/brains-manifest.json`: `gmb_paths_brains_manifest` lo legge PRIMA di
        //    quello del checkout, quindi il programma distribuito sa cosa può scaricare.
        //    L'indice `<exe>.brains.txt` elenca i cervelli DICHIARATI dal programma
        //    (`"brains": ["nome[@versione]"]` nel suo app.settings.json): `gmb_brain_fetch`
        //    li porta in `<exe>/brains`, dove la risoluzione li trova senza rete.
        try
        {
            var manifestSrc = Path.Combine(GmbiPaths.SharedConfigDir, "brains-manifest.json");
            string? manifestDst = null;
            if (File.Exists(manifestSrc))
            {
                var settingsDst = Path.Combine(outputDir, "settings");
                Directory.CreateDirectory(settingsDst);
                manifestDst = Path.Combine(settingsDst, "brains-manifest.json");
                CopyIfNewer(manifestSrc, manifestDst, written, log);
            }
            if (appPath is not null && File.Exists(appPath))
            {
                var index = Path.Combine(outputDir, "brains.txt");
                if (NativeBrains.EmitIndex(appPath, manifestDst, index))
                {
                    written.Add(index);
                    log?.Invoke($"[bundle] indice cervelli -> {index}");
                    if (!string.IsNullOrEmpty(exePath))
                    {
                        var beside = exePath + ".brains.txt";
                        try { File.Copy(index, beside, overwrite: true); written.Add(beside); }
                        catch { /* eseguibile in sola lettura: resta <dir>/brains.txt */ }
                    }
                }
            }
        }
        catch (Exception ex) { log?.Invoke($"[bundle] cervelli non indicizzati: {ex.Message}"); }

        // 4) FIXTURE dichiarate dal programma (`"fixtures": ["markets"]`): il COMPILATORE le
        //    assicura PRIMA di eseguire, sempre in `shared/samples/Fixtures/<nome>/` (manifest in
        //    shared/settings). Best-effort: senza rete il programma fallirà sulla fixture
        //    mancante, non qui.
        try
        {
            if (appPath is not null && File.Exists(appPath))
            {
                var n = NativeFixtures.EnsureDeclared(appPath);
                if (n > 0) log?.Invoke($"[bundle] fixture scaricate: {n}");
            }
        }
        catch (Exception ex) { log?.Invoke($"[bundle] fixture non scaricate: {ex.Message}"); }

        // 5) SKILLS + PLUGINS dichiarati dal programma (`"skills": ["board"], "plugins":
        //    ["web-fetch"]` nel suo app.settings.json). Sono CONTENUTO (markdown + eventuale
        //    .bas del plugin): un eseguibile spedito senza di essi si avvia con l'agente privo
        //    di capacità — `AI.SKILLSLIST()` vuota, nessun tool. Si copiano dall'archivio
        //    configurato (`GmbiPaths.SkillsDir` / `PluginsDir`, nel monorepo `shared/skills`,
        //    `shared/plugins`) in `<outputDir>/skills` e `<outputDir>/plugins`, che è
        //    esattamente dove il runtime li cerca (`<root>/skills`, `pr_shared_or_root`).
        //    NIENTE copia implicita dell'archivio INTERO: 161 skill sono prompt, non servono
        //    tutte e dichiararle è una scelta del programma (stesso schema di `"fixtures"`).
        try
        {
            var app = appPath is not null && File.Exists(appPath) ? ReadObject(appPath) : null;
            if (app is not null)
            {
                var n = 0;
                n += CopyDeclared(app, "skills", GmbiPaths.SkillsDir, outputDir, "skills", written, log);
                n += CopyDeclared(app, "plugins", GmbiPaths.PluginsDir, outputDir, "plugins", written, log);
                if (n > 0) log?.Invoke($"[bundle] skills/plugins dichiarati: {n} file");
            }
        }
        catch (Exception ex) { log?.Invoke($"[bundle] skills/plugins non copiati: {ex.Message}"); }

        return written;
    }

    /// <summary>Copia in <c>&lt;outputDir&gt;/&lt;kind&gt;/&lt;nome&gt;</c> le cartelle elencate
    /// dalla chiave <paramref name="key"/> dell'<c>app.settings.json</c> (<c>"skills": ["board"]</c>).
    /// Solo i DICHIARATI: l'archivio intero non si distribuisce. Ritorna i file copiati; una voce
    /// assente dall'archivio è un warning, non un errore (il programma gira lo stesso).</summary>
    private static int CopyDeclared(JsonObject app, string key, string srcRoot, string outputDir,
                                    string kind, List<string> written, Action<string>? log)
    {
        if (app[key] is not JsonArray arr) return 0;
        var n = 0;
        foreach (var item in arr)
        {
            var name = item?.GetValue<string>();
            if (string.IsNullOrWhiteSpace(name)) continue;
            var src = Path.Combine(srcRoot, name);
            if (!Directory.Exists(src))
            {
                log?.Invoke($"[bundle] {kind} '{name}' non trovato in {srcRoot}: non copiato");
                continue;
            }
            foreach (var f in Directory.EnumerateFiles(src, "*", SearchOption.AllDirectories))
            {
                var rel = Path.GetRelativePath(src, f);
                var dst = Path.Combine(outputDir, kind, name, rel);
                var before = written.Count;
                CopyIfNewer(f, dst, written, null);
                if (written.Count > before) n++;
            }
        }
        return n;
    }

    /// <summary>Unisce le sezioni di RUNTIME di `system` e `app` (l'app vince chiave per chiave).
    /// null se non esiste nessuno dei due file.</summary>
    public static JsonObject? BuildRuntimeSettings(string? systemPath, string? appPath)
    {
        var sys = ReadObject(systemPath);
        var app = ReadObject(appPath);
        if (sys is null && app is null) return null;

        var result = new JsonObject();
        foreach (var section in RuntimeSections)
        {
            var s = sys?[section];
            var a = app?[section];
            if (a is null && s is null) continue;
            if (a is JsonObject ao && s is JsonObject so)
            {
                var merged = (JsonObject)so.DeepClone();      // copia dei default
                foreach (var kv in ao) merged[kv.Key] = kv.Value?.DeepClone();   // override per chiave
                result[section] = merged;
            }
            else if (a is not null) result[section] = a.DeepClone();   // lista/valore: l'app sostituisce
            else result[section] = s!.DeepClone();
        }
        return result;
    }

    private static JsonObject? ReadObject(string? path)
    {
        try
        {
            if (string.IsNullOrWhiteSpace(path) || !File.Exists(path)) return null;
            /* ATTENZIONE: `JsonNode.Parse(rawText)` RIFÀ il parse con le opzioni di default (che
             * NON ammettono commenti) → sui nostri settings (JSONC) falliva e il file veniva
             * ignorato in silenzio (sintomo: si distribuiva solo l'app, senza i default globali).
             * Qui si passa `documentOptions` esplicito. */
            var node = JsonNode.Parse(File.ReadAllText(path), nodeOptions: null, documentOptions: ParseOpts);
            return node as JsonObject;
        }
        catch { return null; }
    }

    private static bool SamePath(string a, string b)
    {
        try { return string.Equals(Path.GetFullPath(a), Path.GetFullPath(b), StringComparison.OrdinalIgnoreCase); }
        catch { return false; }
    }

    private static void WriteIfChanged(string dst, string content, List<string> written, Action<string>? log)
    {
        try
        {
            if (File.Exists(dst) && File.ReadAllText(dst) == content) return;   // già aggiornato
            var d = Path.GetDirectoryName(dst);
            if (!string.IsNullOrEmpty(d)) Directory.CreateDirectory(d);
            File.WriteAllText(dst, content);
            written.Add(dst);
            log?.Invoke($"[bundle] {Path.GetFileName(dst)}");
        }
        catch (Exception ex) { log?.Invoke($"[bundle] non scritto {dst}: {ex.Message}"); }
    }

    private static void CopyIfNewer(string src, string dst, List<string> written, Action<string>? log)
    {
        try
        {
            if (!File.Exists(src)) return;
            var d = Path.GetDirectoryName(dst);
            if (!string.IsNullOrEmpty(d)) Directory.CreateDirectory(d);
            var si = new FileInfo(src);
            if (File.Exists(dst))
            {
                var di = new FileInfo(dst);
                if (di.Length == si.Length && di.LastWriteTimeUtc >= si.LastWriteTimeUtc) return;
            }
            File.Copy(src, dst, overwrite: true);
            written.Add(dst);
            log?.Invoke($"[bundle] {Path.GetFileName(dst)}");
        }
        catch (Exception ex) { log?.Invoke($"[bundle] non copiato {src}: {ex.Message}"); }
    }
}
