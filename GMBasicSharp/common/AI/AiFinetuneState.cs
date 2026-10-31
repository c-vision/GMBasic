using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Text;
using System.Text.Json;

namespace Basic.AI;

/// <summary>
/// Fine-tune LoRA su modelli MLX locali (superficie BASIC AI FINETUNE — vedi
/// Piani/(proposal) - MLX-Finetune-Superficie-BASIC.md, fase F2).
///
/// Il training gira IN-PROCESS nella dylib nativa `gmb_ai_finetune`
/// (native/gmb_ai_finetune/gmb_ai_finetune.c) chiamata via P/Invoke — la STESSA logica del runtime
/// C (AGENTS: C#→C ammesso; C→C# mai). Contratto: request JSON in ingresso, response JSON
/// ({"type":"result"|"error","data":{...}}) in uscita. Mai stub: errore esplicito se fallisce.
/// </summary>
public static class AiFinetuneState
{
    public static string Status { get; private set; } = "idle";
    public static bool Ok { get; private set; }
    public static double TrainLoss { get; private set; } = -1;
    public static double HoldLoss { get; private set; } = -1;
    public static double BaseTrainLoss { get; private set; } = -1;
    public static double BaseHoldLoss { get; private set; } = -1;
    public static string Ckpt { get; private set; } = "";
    public static int GreedyBefore { get; private set; } = -1;
    public static int GreedyAfter { get; private set; } = -1;
    public static int DemoChanged { get; private set; }

    /// <summary>Esegue l'harness nativo via IPC v1 (JSON request/response su pipe).
    /// Ritorna true e popola lo stato; se fallisce, error contiene il messaggio.
    /// opts: dict BASIC (IDictionary) o stringa JSON.</summary>
    /// <summary>Azzera le misure di training: una run FALLITA invalida le misure
    /// precedenti (canone condiviso col runtime C — gmb_finetune.c: su errore
    /// loss/hold/base/demo/greedy tornano a -1/0 e Ckpt a "").</summary>
    private static void ResetMeasures()
    {
        TrainLoss = -1; HoldLoss = -1; BaseTrainLoss = -1; BaseHoldLoss = -1;
        GreedyBefore = -1; GreedyAfter = -1; DemoChanged = 0; Ckpt = "";
    }

    /* --- TRAIN CONFIG: default di sessione (speculari a gmb_finetune.c) --- */
    private static int s_cfgRank = 4, s_cfgLayer = 15, s_cfgSteps = 60, s_cfgBatch = 4, s_cfgSeed = 7;
    private static double s_cfgLr = 0.01;

    /// <summary>TRAIN CONFIG SHOW — JSON dei default di sessione (parità col runtime C
    /// gmb_ai_train_config_json): rank/layer/steps/batch/seed/lr.</summary>
    public static string ConfigJson()
        => "{\"rank\":" + s_cfgRank + ",\"layer\":" + s_cfgLayer + ",\"steps\":" + s_cfgSteps
           + ",\"batch\":" + s_cfgBatch + ",\"seed\":" + s_cfgSeed + ",\"lr\":"
           + s_cfgLr.ToString("0.00000", CultureInfo.InvariantCulture) + "}";

    /// <summary>TRAIN CONFIG SET key, value — aggiorna un default e ritorna il JSON aggiornato.</summary>
    public static string ConfigSet(string key, string value)
    {
        switch ((key ?? "").ToLowerInvariant())
        {
            case "rank": s_cfgRank = ParseI(value, s_cfgRank); break;
            case "layer": s_cfgLayer = ParseI(value, s_cfgLayer); break;
            case "steps": s_cfgSteps = ParseI(value, s_cfgSteps); break;
            case "batch": s_cfgBatch = ParseI(value, s_cfgBatch); break;
            case "seed": s_cfgSeed = ParseI(value, s_cfgSeed); break;
            case "lr": s_cfgLr = ParseD(value, s_cfgLr); break;
        }
        return ConfigJson();
    }

    private static int ParseI(string s, int def)
        => int.TryParse(s, NumberStyles.Integer, CultureInfo.InvariantCulture, out var v) ? v : def;
    private static double ParseD(string s, double def)
        => double.TryParse(s, NumberStyles.Float, CultureInfo.InvariantCulture, out var v) ? v : def;

    public static bool Run(string modelDir, object? opts, out string error)
    {
        error = "";
        var ckpt = "work/ft";
        if (string.IsNullOrWhiteSpace(modelDir))
        {
            error = "modello (path) mancante.";
            ResetMeasures(); Status = "error: " + error; Ok = false;
            JobRegistry.Add(JobRegistry.Kind.Finetune, JobRegistry.Status.Error, error, -1, -1, -1, -1, -1, -1, 0, ckpt);
            return false;
        }

        // --- opzioni (default storici del fine-tune) ---
        var dataset = "work/rule.train.bin";
        var holdout = "work/rule.hold.bin";
        var rank = 4; var layer = 15; var steps = 60; var batch = 4; var seed = 7;
        var frozen = ""; var engine = ""; var full = 0;
        double lr = 0.01;
        try
        {
            if (opts is System.Collections.IDictionary dict)
            {
                dataset = Opt(dict, "dataset", dataset);
                holdout = Opt(dict, "holdout", holdout);
                ckpt = Opt(dict, "ckpt", ckpt);
                rank = OptI(dict, "rank", rank);
                layer = OptI(dict, "layer", layer);
                steps = OptI(dict, "steps", steps);
                batch = OptI(dict, "batch", batch);
                seed = OptI(dict, "seed", seed);
                lr = OptD(dict, "lr", lr);
                frozen = Opt(dict, "frozen", frozen);
                engine = Opt(dict, "engine", engine);
                full = OptI(dict, "full", full);
            }
            else if (opts is string json && !string.IsNullOrWhiteSpace(json))
            {
                using var doc = JsonDocument.Parse(json);
                var r = doc.RootElement;
                dataset = Str(r, "dataset", dataset);
                holdout = Str(r, "holdout", holdout);
                ckpt = Str(r, "ckpt", ckpt);
                rank = Int(r, "rank", rank);
                layer = Int(r, "layer", layer);
                steps = Int(r, "steps", steps);
                batch = Int(r, "batch", batch);
                seed = Int(r, "seed", seed);
                lr = Dbl(r, "lr", lr);
                frozen = Str(r, "frozen", frozen);
                engine = Str(r, "engine", engine);
                full = Int(r, "full", full);
            }
            // §4: 'engine' — solo mlx (il training torch non è supportato).
            if (!string.IsNullOrWhiteSpace(engine) && !engine.Equals("mlx", StringComparison.OrdinalIgnoreCase))
            {
                error = $"engine '{engine}' non supportato (solo mlx).";
                Status = "error: " + error; Ok = false; return false;
            }
        }
        catch (Exception ex)
        {
            error = "opzioni non valide: " + ex.Message;
            ResetMeasures(); Status = "error: " + error;
            JobRegistry.Add(JobRegistry.Kind.Finetune, JobRegistry.Status.Error, error, -1, -1, -1, -1, -1, -1, 0, ckpt);
            return false;
        }

        // R2: costruisci richiesta IPC JSON
        var request = new StringBuilder();
        request.Append("{\"id\":1,\"method\":\"finetune\",\"params\":{");
        AppendStr(request, "model_dir", modelDir);
        AppendStr(request, "dataset", dataset);
        AppendStr(request, "holdout", holdout);
        AppendStr(request, "ckpt", ckpt);
        request.Append($",\"rank\":{rank},\"layer\":{layer},\"steps\":{steps},\"batch\":{batch}");
        request.Append($",\"lr\":{lr.ToString("R", CultureInfo.InvariantCulture)},\"seed\":{seed}");
        if (!string.IsNullOrWhiteSpace(frozen)) AppendStr(request, "frozen", frozen);
        if (full != 0) request.Append(",\"full\":1");
        request.Append("}}\n");

        Status = "running";
        // P/Invoke al runtime C (dylib gmb_ai_finetune): niente subprocess. Bloccante come prima
        // (l'harness IPC attendeva il processo): il fine-tune dura minuti.
        // Se la dylib nativa non si carica (deploy rotto/incoerente) la run va comunque registrata
        // come ERROR (come fa la gamba VM/C quando il modello è assente): senza questo, IL lanciava
        // un DllNotFoundException grezzo e NON incrementava il registro run → divergenza misurata
        // su TrainCommandsTour (TRAIN RUNS PRUNE: IL 0 vs VM/C 2).
        string response;
        try
        {
            response = RunNative(request.ToString(), Basic.Providers.FinetuneNative.gmb_ai_finetune_exec);
        }
        catch (Exception ex)
        {
            error = "runtime nativo fine-tune non disponibile: " + ex.Message;
            ResetMeasures(); Status = "error: " + error;
            JobRegistry.Add(JobRegistry.Kind.Finetune, JobRegistry.Status.Error, error, -1, -1, -1, -1, -1, -1, 0, ckpt);
            return false;
        }

        // parsa risposta JSON strutturata
        try
        {
            using var doc = JsonDocument.Parse(response);
            var r = doc.RootElement;
            var type = r.TryGetProperty("type", out var t) ? t.GetString() : "";
            if (type != "result")
            {
                var msg = r.TryGetProperty("data", out var d) && d.TryGetProperty("message", out var m) ? m.GetString() : response;
                error = msg ?? "errore IPC sconosciuto";
                ResetMeasures(); Status = "error: " + error;
                JobRegistry.Add(JobRegistry.Kind.Finetune, JobRegistry.Status.Error, error, -1, -1, -1, -1, -1, -1, 0, ckpt);
                return false;
            }
            var data = r.GetProperty("data");
            var ok = data.TryGetProperty("ok", out var o) && o.ValueKind == JsonValueKind.True;
            if (!ok)
            {
                error = "report riporta ok=false.";
                ResetMeasures(); Status = "error: " + error;
                JobRegistry.Add(JobRegistry.Kind.Finetune, JobRegistry.Status.Error, error, -1, -1, -1, -1, -1, -1, 0, ckpt);
                return false;
            }
            TrainLoss = GetDbl(data, "train", -1);
            HoldLoss = GetDbl(data, "hold", -1);
            BaseTrainLoss = GetDbl(data, "baseTrain", -1);
            BaseHoldLoss = GetDbl(data, "baseHold", -1);
            Ckpt = ckpt;
            GreedyBefore = GetInt(data, "greedyBefore", -1);
            GreedyAfter = GetInt(data, "greedyAfter", -1);
            DemoChanged = GetInt(data, "demoChanged", 0);
            Ok = true; Status = "ok";
            JobRegistry.Add(JobRegistry.Kind.Finetune, JobRegistry.Status.Ok, "ok",
                TrainLoss, HoldLoss, BaseTrainLoss, BaseHoldLoss, GreedyBefore, GreedyAfter, DemoChanged, ckpt);
            return true;
        }
        catch (Exception ex)
        {
            error = "risposta IPC JSON non valida: " + ex.Message;
            ResetMeasures(); Status = "error: " + error;
            JobRegistry.Add(JobRegistry.Kind.Finetune, JobRegistry.Status.Error, error, -1, -1, -1, -1, -1, -1, 0, ckpt);
            return false;
        }
    }

    /// <summary>TRAIN DPO — preference optimization (coppie chosen/rejected). opts: dict/JSON con
    /// chosen, rejected, ckpt, rank, layer, steps, batch, lr, beta, seed. Stesso runtime C nativo
    /// (finetune_dpo_exec, loss DPO + SPSA). Ritorna true e popola lo stato; altrimenti error.</summary>
    public static bool Dpo(string modelDir, object? opts, out string error)
    {
        error = "";
        var chosen = ""; var rejected = ""; var ckpt = "work/dpo"; var pairs = "";
        var rank = 4; var layer = 15; var steps = 40; var batch = 2; var seed = 7; var exact = 1;
        double lr = 5e-3, beta = 0.1;
        try
        {
            if (opts is System.Collections.IDictionary dict)
            {
                chosen = Opt(dict, "chosen", chosen); rejected = Opt(dict, "rejected", rejected);
                ckpt = Opt(dict, "ckpt", ckpt); pairs = Opt(dict, "pairs", pairs);
                rank = OptI(dict, "rank", rank); layer = OptI(dict, "layer", layer); steps = OptI(dict, "steps", steps);
                batch = OptI(dict, "batch", batch); seed = OptI(dict, "seed", seed); exact = OptI(dict, "exact", exact);
                lr = OptD(dict, "lr", lr); beta = OptD(dict, "beta", beta);
            }
            else if (opts is string json && !string.IsNullOrWhiteSpace(json))
            {
                using var doc = JsonDocument.Parse(json);
                var r = doc.RootElement;
                chosen = Str(r, "chosen", chosen); rejected = Str(r, "rejected", rejected); ckpt = Str(r, "ckpt", ckpt); pairs = Str(r, "pairs", pairs);
                rank = Int(r, "rank", rank); layer = Int(r, "layer", layer); steps = Int(r, "steps", steps);
                batch = Int(r, "batch", batch); seed = Int(r, "seed", seed); exact = Int(r, "exact", exact);
                lr = Dbl(r, "lr", lr); beta = Dbl(r, "beta", beta);
            }
        }
        catch (Exception ex)
        {
            error = "opzioni non valide: " + ex.Message;
            Status = "error: " + error; Ok = false; return false;
        }
        // §4: `TRAIN DPO <model>, <pairs>, <out>` — <pairs> è un PREFISSO: chosen/rejected derivano
        // per convenzione (`<pairs>.chosen.bin` / `<pairs>.rejected.bin`) se non dati esplicitamente.
        if (!string.IsNullOrWhiteSpace(pairs))
        {
            if (string.IsNullOrWhiteSpace(chosen)) chosen = pairs + ".chosen.bin";
            if (string.IsNullOrWhiteSpace(rejected)) rejected = pairs + ".rejected.bin";
        }
        if (string.IsNullOrWhiteSpace(modelDir)) { error = "modello (path) mancante."; Status = "error: " + error; Ok = false; return false; }
        if (string.IsNullOrWhiteSpace(chosen)) { error = "opzione 'chosen' (dataset preferite) mancante."; Status = "error: " + error; Ok = false; return false; }
        if (string.IsNullOrWhiteSpace(rejected)) { error = "opzione 'rejected' (dataset rifiutate) mancante."; Status = "error: " + error; Ok = false; return false; }

        var request = new StringBuilder();
        request.Append("{\"id\":1,\"method\":\"dpo\",\"params\":{");
        AppendStr(request, "model_dir", modelDir);
        AppendStr(request, "chosen", chosen);
        AppendStr(request, "rejected", rejected);
        AppendStr(request, "ckpt", ckpt);
        request.Append($",\"rank\":{rank},\"layer\":{layer},\"steps\":{steps},\"batch\":{batch}");
        request.Append($",\"lr\":{lr.ToString("R", CultureInfo.InvariantCulture)},\"beta\":{beta.ToString("R", CultureInfo.InvariantCulture)},\"seed\":{seed},\"exact\":{exact}");
        request.Append("}}\n");

        Status = "running (dpo)";
        string response = RunNative(request.ToString(), Basic.Providers.FinetuneNative.gmb_ai_dpo_exec);
        try
        {
            using var doc = JsonDocument.Parse(response);
            var r = doc.RootElement;
            var type = r.TryGetProperty("type", out var t) ? t.GetString() : "";
            if (type != "result")
            {
                var msg = r.TryGetProperty("data", out var d) && d.TryGetProperty("message", out var m) ? m.GetString() : response;
                error = msg ?? "errore DPO sconosciuto";
                Status = "error: " + error; Ok = false; return false;
            }
            var data = r.GetProperty("data");
            var ok = data.TryGetProperty("ok", out var o) && o.ValueKind == JsonValueKind.True;
            if (!ok) { error = "report DPO riporta ok=false."; Status = "error: " + error; Ok = false; return false; }
            TrainLoss = GetDbl(data, "train", -1);
            BaseTrainLoss = GetDbl(data, "baseTrain", -1);
            Ckpt = ckpt; Ok = true; Status = "ok";
            return true;
        }
        catch (Exception ex)
        {
            error = "risposta DPO non valida: " + ex.Message;
            Status = "error: " + error; Ok = false; return false;
        }
    }

    /// <summary>TRAIN GRPO (RLHF senza critic, RLVR): model + opts{dataset|train,group,maxnew,clip,kl,lr,steps,temperature,seed}.</summary>
    public static bool Grpo(string modelDir, object? opts, out string error)
    {
        error = "";
        var dataset = ""; var ckpt = "work/grpo";
        var rank = 4; var layer = 15; var steps = 40; var group = 4; var maxnew = 1; var seed = 7;
        double lr = 5e-3, clip = 0.2, kl = 0.0, temperature = 1.0;
        try
        {
            if (opts is System.Collections.IDictionary dict)
            {
                dataset = Opt(dict, "dataset", dataset); if (string.IsNullOrWhiteSpace(dataset)) dataset = Opt(dict, "train", dataset);
                ckpt = Opt(dict, "ckpt", ckpt);
                rank = OptI(dict, "rank", rank); layer = OptI(dict, "layer", layer); steps = OptI(dict, "steps", steps);
                group = OptI(dict, "group", group); maxnew = OptI(dict, "maxnew", maxnew); seed = OptI(dict, "seed", seed);
                lr = OptD(dict, "lr", lr); clip = OptD(dict, "clip", clip); kl = OptD(dict, "kl", kl);
                temperature = OptD(dict, "temperature", temperature);
            }
            else if (opts is string json && !string.IsNullOrWhiteSpace(json))
            {
                using var doc = JsonDocument.Parse(json);
                var r = doc.RootElement;
                dataset = Str(r, "dataset", dataset); if (string.IsNullOrWhiteSpace(dataset)) dataset = Str(r, "train", dataset);
                ckpt = Str(r, "ckpt", ckpt);
                rank = Int(r, "rank", rank); layer = Int(r, "layer", layer); steps = Int(r, "steps", steps);
                group = Int(r, "group", group); maxnew = Int(r, "maxnew", maxnew); seed = Int(r, "seed", seed);
                lr = Dbl(r, "lr", lr); clip = Dbl(r, "clip", clip); kl = Dbl(r, "kl", kl);
                temperature = Dbl(r, "temperature", temperature);
            }
        }
        catch (Exception ex)
        {
            error = "opzioni non valide: " + ex.Message; Status = "error: " + error; Ok = false; return false;
        }
        if (string.IsNullOrWhiteSpace(modelDir)) { error = "modello (path) mancante."; Status = "error: " + error; Ok = false; return false; }
        if (string.IsNullOrWhiteSpace(dataset)) { error = "opzione 'dataset' (o 'train') mancante."; Status = "error: " + error; Ok = false; return false; }

        var request = new StringBuilder();
        request.Append("{\"id\":1,\"method\":\"grpo\",\"params\":{");
        AppendStr(request, "model_dir", modelDir);
        AppendStr(request, "dataset", dataset);
        AppendStr(request, "ckpt", ckpt);
        request.Append($",\"rank\":{rank},\"layer\":{layer},\"steps\":{steps},\"group\":{group},\"maxnew\":{maxnew}");
        request.Append($",\"lr\":{lr.ToString("R", CultureInfo.InvariantCulture)},\"clip\":{clip.ToString("R", CultureInfo.InvariantCulture)},\"kl\":{kl.ToString("R", CultureInfo.InvariantCulture)},\"temperature\":{temperature.ToString("R", CultureInfo.InvariantCulture)},\"seed\":{seed}");
        request.Append("}}\n");

        Status = "running (grpo)";
        string response = RunNative(request.ToString(), Basic.Providers.FinetuneNative.gmb_ai_grpo_exec);
        try
        {
            using var doc = JsonDocument.Parse(response);
            var r = doc.RootElement;
            var type = r.TryGetProperty("type", out var t) ? t.GetString() : "";
            if (type != "result")
            {
                var msg = r.TryGetProperty("data", out var d) && d.TryGetProperty("message", out var m) ? m.GetString() : response;
                error = msg ?? "errore GRPO sconosciuto"; Status = "error: " + error; Ok = false; return false;
            }
            var data = r.GetProperty("data");
            var ok = data.TryGetProperty("ok", out var o) && o.ValueKind == JsonValueKind.True;
            if (!ok) { error = "report GRPO riporta ok=false."; Status = "error: " + error; Ok = false; return false; }
            TrainLoss = GetDbl(data, "grpo", -1);
            BaseTrainLoss = GetDbl(data, "base", -1);
            Ckpt = ckpt; Ok = true; Status = "ok";
            return true;
        }
        catch (Exception ex)
        {
            error = "risposta GRPO non valida: " + ex.Message; Status = "error: " + error; Ok = false; return false;
        }
    }

    /// <summary>Deploy/verifica via IPC v1 (AI LORA APPLY). opts: dict BASIC (IDictionary)
    /// o stringa JSON — stessa accettazione di Run e del runtime C (gmb_ai_lora_apply_run_opts
    /// parsa json_opts stringa): senza il ramo stringa, `LLM LORA APPLY "ckpt", "{...}"`
    /// falliva solo su IL/PCode ("model mancante") mentre VM/C/C99 funzionavano (divergenza
    /// di parametri, probe 2026-10-01).</summary>
    public static bool Apply(string ckptPrefix, object? opts, out string error)
    {
        error = "";
        var model = "";
        var dataset = "";
        if (opts is System.Collections.IDictionary d)
        {
            model = Opt(d, "model", "");
            dataset = Opt(d, "dataset", "");
        }
        else if (opts is string json && !string.IsNullOrWhiteSpace(json))
        {
            try
            {
                using var doc = JsonDocument.Parse(json);
                var r = doc.RootElement;
                model = Str(r, "model", "");
                dataset = Str(r, "dataset", "");
            }
            catch (Exception ex)
            {
                error = "opzioni non valide: " + ex.Message;
                Status = "error: " + error; Ok = false; return false;
            }
        }
        if (string.IsNullOrWhiteSpace(model)) { error = "opzione 'model' (path modello MLX) mancante."; Status = "error: " + error; Ok = false; return false; }
        if (string.IsNullOrWhiteSpace(dataset)) { error = "opzione 'dataset' (file .bin per il prompt demo) mancante."; Status = "error: " + error; Ok = false; return false; }

        // R2: costruisci richiesta IPC JSON
        var request = new StringBuilder();
        request.Append("{\"id\":1,\"method\":\"apply\",\"params\":{");
        AppendStr(request, "ckpt", ckptPrefix);
        AppendStr(request, "model_dir", model);
        AppendStr(request, "dataset", dataset);
        request.Append("}}\n");

        Status = "running (apply)";
        // P/Invoke al runtime C (dylib gmb_ai_finetune): niente subprocess.
        string response = RunNative(request.ToString(), Basic.Providers.FinetuneNative.gmb_ai_lora_apply_exec);

        try
        {
            using var doc = JsonDocument.Parse(response);
            var r = doc.RootElement;
            var type = r.TryGetProperty("type", out var t) ? t.GetString() : "";
            if (type != "result")
            {
                error = r.TryGetProperty("data", out var ed) && ed.TryGetProperty("message", out var em) ? em.GetString() ?? "apply fallito" : "apply fallito";
                Status = "error: " + error;
                JobRegistry.Add(JobRegistry.Kind.LoraApply, JobRegistry.Status.Error, error, -1, -1, -1, -1, -1, -1, 0, ckptPrefix);
                return false;
            }
            var data = r.GetProperty("data");
            var ok = data.TryGetProperty("ok", out var o) && o.ValueKind == JsonValueKind.True;
            var msg = data.TryGetProperty("message", out var m) ? m.GetString() ?? "" : "";
            if (!ok || msg.Contains("nessun effetto"))
            {
                error = string.IsNullOrEmpty(msg) ? "apply fallito senza messaggio" : msg;
                Status = "error:" + error;
                JobRegistry.Add(JobRegistry.Kind.LoraApply, JobRegistry.Status.Error, error, -1, -1, -1, -1, -1, -1, 0, ckptPrefix);
                return false;
            }
            Ok = true;
            Status = "ok: " + Tail(msg, 300);
            JobRegistry.Add(JobRegistry.Kind.LoraApply, JobRegistry.Status.Ok, msg, -1, -1, -1, -1, -1, -1, 0, ckptPrefix);
            return true;
        }
        catch (Exception ex)
        {
            error = "risposta IPC JSON non valida: " + ex.Message;
            Status = "error: " + error;
            JobRegistry.Add(JobRegistry.Kind.LoraApply, JobRegistry.Status.Error, error, -1, -1, -1, -1, -1, -1, 0, ckptPrefix);
            return false;
        }
    }

    private static void AppendStr(StringBuilder sb, string key, string val)
    {
        sb.Append($",\"{key}\":\"{EscapeJson(val)}\"");
    }
    private static string EscapeJson(string s)
        => s.Replace("\\", "\\\\").Replace("\"", "\\\"").Replace("\n", "\\n").Replace("\r", "\\r").Replace("\t", "\\t");

    // ---- chiamata nativa in-process (dylib gmb_ai_finetune) ----

    /// <summary>Esegue il runtime C del fine-tune/apply IN-PROCESS e ritorna la response JSON
    /// (formato protocollo: {\"type\":\"result\"|\"error\",\"data\":{...}}). Niente subprocess.</summary>
    private static string RunNative(string request, Func<string, byte[], int, int> fn)
    {
        var buf = new byte[8192];
        int rc = fn(request, buf, buf.Length);
        int n = Array.IndexOf(buf, (byte)0);
        if (n < 0) n = buf.Length;
        var resp = System.Text.Encoding.UTF8.GetString(buf, 0, n);
        if (string.IsNullOrEmpty(resp) && rc != 0)
            return "{\"type\":\"error\",\"data\":{\"message\":\"errore nativo (rc=" + rc + ")\"}}";
        return resp;
    }

    private static string Str(JsonElement e, string k, string d)
        => e.TryGetProperty(k, out var p) && p.ValueKind == JsonValueKind.String ? (p.GetString() ?? d) : d;
    private static string Opt(System.Collections.IDictionary d, string k, string def)
        => d.Contains(k) ? (d[k]?.ToString() ?? def) : def;
    private static int OptI(System.Collections.IDictionary d, string k, int def)
        => d.Contains(k) && d[k] is IConvertible c ? c.ToInt32(CultureInfo.InvariantCulture) : def;
    private static double OptD(System.Collections.IDictionary d, string k, double def)
        => d.Contains(k) && d[k] is IConvertible c ? c.ToDouble(CultureInfo.InvariantCulture) : def;
    private static int Int(JsonElement e, string k, int d)
        => e.TryGetProperty(k, out var p) && p.ValueKind == JsonValueKind.Number ? p.GetInt32() : d;
    private static double Dbl(JsonElement e, string k, double d)
        => e.TryGetProperty(k, out var p) && p.ValueKind == JsonValueKind.Number ? p.GetDouble() : d;
    private static double GetDbl(JsonElement e, string k, double d) => Dbl(e, k, d);
    private static int GetInt(JsonElement e, string k, int d) => Int(e, k, d);
    private static string Tail(string s, int n) => s.Length <= n ? s : s[^n..];
}
