using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;

namespace Basic.Providers;

/// <summary>Ponte env→registro per il lato C# (parità `gmb_opt_env` lato C).
/// Per i parametri di FUNZIONAMENTO: legge il registro opzioni (se impostato via
/// `AI SET OPTION`) e, in mancanza, ricade sulla variabile d'ambiente. La diagnostica
/// resta `Environment.GetEnvironmentVariable`. Se la lib nativa non è caricata,
/// ricade sull'env senza errore.</summary>
public static class GmbEnv
{
    private const string LibName = "gmb_ai_llm";

    // `gmb_opt_env` è esportato da libgmb_ai_llm (link -u). La lib MLX non lo espone.
    [DllImport(LibName, CharSet = CharSet.Ansi)]
    private static extern IntPtr gmb_opt_env(string name);

    public static string? Get(string name)
    {
        try
        {
            var p = gmb_opt_env(name ?? "");
            if (p != IntPtr.Zero)
            {
                var v = Marshal.PtrToStringUTF8(p);
                if (v != null) return v;
            }
        }
        catch
        {
            /* lib non caricata: fallback settings/env */
        }
        if (TrySettings(name, out var s)) return s;
        return Environment.GetEnvironmentVariable(name);
    }

    // Lettura del SOLO registro (`gmb_opt_get`), senza la ricaduta sull'ambiente: la usa
    // `AI LIST OPTIONS`, che elenca le OPZIONI impostate (`AI SET OPTION` o sezione `engine`)
    // e non i valori che l'harness esporta nel processo. Se la lib nativa non è caricata,
    // ritorna null (il chiamante stampa `(vuoto)`).
    [DllImport(LibName, CharSet = CharSet.Ansi)]
    private static extern int gmb_opt_get(string name, byte[] outBuf, int cap);

    // Scrittura nel registro del runtime C (`gmb_opt_set`). Serve alle gambe managed per le
    // opzioni che il RUNTIME C legge a ogni disegno (namespace `ui.*` di accessibilità): il
    // registro dell'engine (`llm_ai_mlx_opt_set`, dove scrive `MlxOptions`) è un ALTRO registro
    // e il codice C che colora non lo vede.
    [DllImport(LibName, CharSet = CharSet.Ansi)]
    private static extern int gmb_opt_set(string name, string value);

    public static string? GetRegistry(string name)
    {
        try
        {
            var buf = new byte[512];
            if (gmb_opt_get(name ?? "", buf, buf.Length) != 0) return null;
            int n = 0; while (n < buf.Length && buf[n] != 0) n++;
            return System.Text.Encoding.UTF8.GetString(buf, 0, n);
        }
        catch
        {
            /* lib non caricata: nessun valore */
            return null;
        }
    }

    /// <summary>Scrive una voce nel registro opzioni del runtime C. Ritorna true se la
    /// libreria nativa è caricata e il set è riuscito. Usata dall'inoltro di `ui.*`
    /// in IL/PCodeVM, dove il registro dell'engine e quello C sono separati.</summary>
    public static bool SetRegistry(string name, string value)
    {
        try { return gmb_opt_set(name ?? "", value ?? "") == 0; }
        catch
        {
            /* lib non caricata: nessuna scrittura */
            return false;
        }
    }

    /* Lettura INDIPENDENTE dei settings condivisi (stessa gerarchia del C, senza dipendere
     * dalla lib nativa): sezione `engine` di system.settings.json (dir eseguibile) poi
     * app.settings.json (CWD), con l'app che sovrascrive. Cache al primo accesso. */
    private static Dictionary<string, string>? _cache;

    private static bool TrySettings(string name, out string value)
    {
        _cache ??= LoadMerged();
        return _cache.TryGetValue(name, out value!);
    }

    private static Dictionary<string, string> LoadMerged()
    {
        var d = new Dictionary<string, string>(StringComparer.Ordinal);
        MergeEngine(d, Path.Combine(AppContext.BaseDirectory, "system.settings.json")); // dir eseguibile
        MergeEngine(d, "system.settings.json");   // copia locale (dir app), se presente
        MergeEngine(d, "app.settings.json");      // override applicazione (CWD)
        // Un COMPILATO distribuito si lancia da qualunque cartella: porta l'override accanto a sé.
        MergeEngine(d, Path.Combine(AppContext.BaseDirectory, "app.settings.json"));
        return d;
    }

    private static void MergeEngine(Dictionary<string, string> d, string file)
    {
        string js;
        try
        {
            if (!File.Exists(file)) return;
            js = File.ReadAllText(file);
        }
        catch { return; }
        int e = js.IndexOf("\"engine\"", StringComparison.Ordinal);
        if (e < 0) return;
        int ob = js.IndexOf('{', e); if (ob < 0) return;
        int oe = js.IndexOf('}', ob); if (oe < 0) return;
        int i = ob + 1;
        while (i < oe)
        {
            int ks = js.IndexOf('"', i); if (ks < 0 || ks >= oe) break;
            int ke = js.IndexOf('"', ks + 1); if (ke < 0) break;
            int c = js.IndexOf(':', ke); if (c < 0) break;
            string key = js.Substring(ks + 1, ke - ks - 1);
            int vs = js.IndexOf('"', c);
            string val;
            if (vs >= 0 && vs < oe)
            {
                int ve = js.IndexOf('"', vs + 1); if (ve < 0) break;
                val = js.Substring(vs + 1, ve - vs - 1);
                i = ve + 1;
            }
            else
            {
                int ve = c + 1;
                while (ve < oe && js[ve] != ',' && js[ve] != '}' && !char.IsWhiteSpace(js[ve])) ve++;
                val = js.Substring(c + 1, ve - c - 1).Trim();
                i = ve;
            }
            d[key] = val;
        }
    }
}
