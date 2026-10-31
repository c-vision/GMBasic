using System;
using System.Globalization;
using System.IO;
using System.Text;
using System.Text.Json;

namespace Basic.AI;

/// <summary>
/// Stream D — AI DISTILL (self-distillation lm_head-only), fase D1 (BUILD).
/// La logica dataset/report vive in native/gmb_runtime_c/src/gmb_distill.c (§0bis, condivisa
/// coi backend C); questa classe orchestra le opzioni e chiama la native via P/Invoke (C#→C).
/// BUILD: jsonl di coppie (prompt,risposta) → &lt;out&gt;.train.bin / .hold.bin (formato .bin
/// di make_lora_dataset.py), con tokenizer.json del model_dir.
/// SAMPLE/TRAIN/APPLY/EVAL/RESTORE sono fasi successive (non ancora implementate).
/// </summary>
public static class AiDistillState
{
    public static string Status { get; private set; } = "idle";
    public static bool Ok { get; private set; }
    public static int Pairs { get; private set; }
    public static int TrainSamples { get; private set; }
    public static int HoldSamples { get; private set; }
    public static int Samplen { get; private set; }
    public static int SampleFail { get; private set; }
    public static int SampleDone { get; private set; }
    public static double EvalTokBase { get; private set; } = -1;
    public static double EvalPassBase { get; private set; } = -1;
    public static double EvalTokDist { get; private set; } = -1;
    public static double EvalPassDist { get; private set; } = -1;
    public static double EvalAccBase { get; private set; } = -1;
    public static double EvalAccDist { get; private set; } = -1;
    public static double TrainLoss { get; private set; } = -1;
    public static double TrainHold { get; private set; } = -1;
    public static string TrainCkpt { get; private set; } = "";
    public static int TrainDone { get; private set; }
    public static string OutPrefix { get; private set; } = "";
    /// <summary>Ultimo body JSON restituito dal runtime condiviso (stesso formato dei backend C:
    /// <c>{"type":"result","data":{…}}</c>). I comandi lo ritornano per parità cross-backend.</summary>
    public static string LastBody { get; private set; } = "";

    /// <summary>Esegue BUILD. opts (dict BASIC o JSON): model (dir con tokenizer.json),
    /// tokenizer (path esplicito), out (prefisso output), window (def 8), holdout (def 0.1),
    /// max (def 0 = tutte). Ritorna true e popola lo stato; error contiene il messaggio.</summary>
    public static bool Build(string jsonlPath, object? opts, out string error)
    {
        error = "";
        if (string.IsNullOrWhiteSpace(jsonlPath)) { error = "path jsonl mancante."; Status = "error:" + error; Ok = false; return false; }
        var model = "";
        var tokenizer = "";
        var outPrefix = jsonlPath.EndsWith(".jsonl", StringComparison.OrdinalIgnoreCase)
            ? jsonlPath[..^6] : jsonlPath;
        var window = 8; double holdout = 0.1; var maxPairs = 0;
        try
        {
            if (opts is System.Collections.IDictionary d)
            {
                model = Opt(d, "model", model);
                tokenizer = Opt(d, "tokenizer", tokenizer);
                outPrefix = Opt(d, "out", outPrefix);
                window = OptI(d, "window", window);
                holdout = OptD(d, "holdout", holdout);
                maxPairs = OptI(d, "max", maxPairs);
            }
            else if (opts is string pos && !string.IsNullOrWhiteSpace(pos) && !pos.StartsWith('{'))
            {
                // TRAIN DATASET BUILD <jsonl>, <out>: 2° argomento posizionale = out (non JSON).
                outPrefix = pos;
            }
            else if (opts is string json && !string.IsNullOrWhiteSpace(json) && json != "{}")
            {
                using var doc = JsonDocument.Parse(json);
                var r = doc.RootElement;
                model = Str(r, "model", model);
                tokenizer = Str(r, "tokenizer", tokenizer);
                outPrefix = Str(r, "out", outPrefix);
                window = Int(r, "window", window);
                holdout = Dbl(r, "holdout", holdout);
                maxPairs = Int(r, "max", maxPairs);
            }
        }
        catch (Exception ex) { error = "opzioni non valide: " + ex.Message; Status = "error:" + error; Ok = false; return false; }

        if (string.IsNullOrWhiteSpace(tokenizer))
        {
            if (string.IsNullOrWhiteSpace(model)) { error = "serve 'model' (dir) o 'tokenizer' (path tokenizer.json)."; Status = "error:" + error; Ok = false; return false; }
            tokenizer = Path.Combine(model, "tokenizer.json");
        }
        if (!File.Exists(tokenizer)) { error = "tokenizer.json non trovato: " + tokenizer; Status = "error:" + error; Ok = false; return false; }

        Status = "running";
        var optsJson = JsonSerializer.Serialize(new Dictionary<string, object>
        {
            ["model"] = model, ["tokenizer"] = tokenizer, ["out"] = outPrefix,
            ["window"] = window, ["holdout"] = holdout, ["max"] = maxPairs
        });
        var bb = System.Runtime.InteropServices.Marshal.AllocHGlobal(8192);
        try
        {
            if (Basic.Providers.DistillNative.gmb_distill_build_opts(jsonlPath, optsJson, bb, 8192) != 0)
            { error = System.Runtime.InteropServices.Marshal.PtrToStringUTF8(bb) ?? "build fallita"; Status = "error"; Ok = false; return false; }
            LastBody = System.Runtime.InteropServices.Marshal.PtrToStringUTF8(bb) ?? "";
        }
        finally { System.Runtime.InteropServices.Marshal.FreeHGlobal(bb); }
        Pairs = Basic.Providers.DistillNative.gmb_distill_pairs();
        TrainSamples = Basic.Providers.DistillNative.gmb_distill_trainsamples();
        HoldSamples = Basic.Providers.DistillNative.gmb_distill_holdsamples();
        OutPrefix = outPrefix;
        Ok = true; Status = "ok";
        return true;
    }

    /// <summary>Esegue SAMPLE. opts: model (dir), out (jsonl di output), k (def 4), temp (def 0.7),
    /// maxlen (def 256), verify ("rule"|"code"|""), seed. Ritorna true e popola lo stato.</summary>
    public static bool Sample(string prompts, object? opts, out string error)
    {
        error = "";
        if (string.IsNullOrWhiteSpace(prompts)) { error = "path prompts mancante."; Status = "error:" + error; Ok = false; return false; }
        var model = "";
        var outPath = prompts.EndsWith(".jsonl", StringComparison.OrdinalIgnoreCase)
            ? prompts[..^6] + ".distill.jsonl" : prompts + ".distill.jsonl";
        var verify = "";
        var k = 4; var maxlen = 256; var seed = 20260909; double temp = 0.7;
        try
        {
            if (opts is System.Collections.IDictionary d)
            {
                model = Opt(d, "model", model);
                outPath = Opt(d, "out", outPath);
                verify = Opt(d, "verify", verify);
                k = OptI(d, "k", k); maxlen = OptI(d, "maxlen", maxlen); seed = OptI(d, "seed", seed);
                temp = OptD(d, "temp", temp);
            }
            else if (opts is string json && !string.IsNullOrWhiteSpace(json) && json != "{}")
            {
                using var doc = JsonDocument.Parse(json);
                var r = doc.RootElement;
                model = Str(r, "model", model);
                outPath = Str(r, "out", outPath);
                verify = Str(r, "verify", verify);
                k = Int(r, "k", k); maxlen = Int(r, "maxlen", maxlen); seed = Int(r, "seed", seed);
                temp = Dbl(r, "temp", temp);
            }
        }
        catch (Exception ex) { error = "opzioni non valide: " + ex.Message; Status = "error:" + error; Ok = false; return false; }
        if (string.IsNullOrWhiteSpace(model)) { error = "serve 'model' (dir del modello)."; Status = "error:" + error; Ok = false; return false; }

        var optsJson = JsonSerializer.Serialize(new Dictionary<string, object>
        {
            ["model"] = model, ["out"] = outPath, ["k"] = k, ["temp"] = temp,
            ["maxlen"] = maxlen, ["verify"] = verify, ["seed"] = seed
        });

        Status = "running";
        var buf = System.Runtime.InteropServices.Marshal.AllocHGlobal(8192);
        try
        {
            if (Basic.Providers.DistillNative.gmb_distill_sample_opts(prompts, optsJson, buf, 8192) != 0)
            {
                error = System.Runtime.InteropServices.Marshal.PtrToStringUTF8(buf) ?? "sample fallito";
                Status = "error"; Ok = false; return false;
            }
            LastBody = System.Runtime.InteropServices.Marshal.PtrToStringUTF8(buf) ?? "";
        }
        finally { System.Runtime.InteropServices.Marshal.FreeHGlobal(buf); }

        Samplen = Basic.Providers.DistillNative.gmb_distill_samplen();
        SampleFail = Basic.Providers.DistillNative.gmb_distill_samplefail();
        SampleDone = (int)Basic.Providers.DistillNative.gmb_distill_sampledone();
        OutPrefix = outPath;
        Ok = true; Status = "ok";
        return true;
    }

    /// <summary>Esegue EVAL paired (idx 0 base / idx 1 adapter). opts: model, ckpt (prefisso adapter),
    /// out, maxlen (def 256), repeat (def 3), temp (def 0.7), verify, seed.</summary>
    public static bool Eval(string prompts, object? opts, out string error)
    {
        error = "";
        if (string.IsNullOrWhiteSpace(prompts)) { error = "path prompts mancante."; Status = "error:" + error; Ok = false; return false; }
        var model = "";
        var ckpt = "";
        var outPath = "work/eval_distill.json";
        var verify = "";
        var maxlen = 256; var repeat = 3; var seed = 20260909; double temp = 0.7;
        try
        {
            if (opts is System.Collections.IDictionary d)
            {
                model = Opt(d, "model", model); ckpt = Opt(d, "ckpt", ckpt);
                outPath = Opt(d, "out", outPath); verify = Opt(d, "verify", verify);
                maxlen = OptI(d, "maxlen", maxlen); repeat = OptI(d, "repeat", repeat); seed = OptI(d, "seed", seed);
                temp = OptD(d, "temp", temp);
            }
            else if (opts is string json && !string.IsNullOrWhiteSpace(json) && json != "{}")
            {
                using var doc = JsonDocument.Parse(json);
                var r = doc.RootElement;
                model = Str(r, "model", model); ckpt = Str(r, "ckpt", ckpt);
                outPath = Str(r, "out", outPath); verify = Str(r, "verify", verify);
                maxlen = Int(r, "maxlen", maxlen); repeat = Int(r, "repeat", repeat); seed = Int(r, "seed", seed);
                temp = Dbl(r, "temp", temp);
            }
        }
        catch (Exception ex) { error = "opzioni non valide: " + ex.Message; Status = "error:" + error; Ok = false; return false; }
        if (string.IsNullOrWhiteSpace(model)) { error = "serve 'model' (dir del modello)."; Status = "error:" + error; Ok = false; return false; }

        var optsJson = JsonSerializer.Serialize(new Dictionary<string, object>
        {
            ["model"] = model, ["ckpt"] = ckpt, ["out"] = outPath, ["maxlen"] = maxlen,
            ["repeat"] = repeat, ["temp"] = temp, ["verify"] = verify, ["seed"] = seed
        });

        Status = "running";
        var buf = System.Runtime.InteropServices.Marshal.AllocHGlobal(8192);
        try
        {
            if (Basic.Providers.DistillNative.gmb_distill_eval_opts(prompts, optsJson, buf, 8192) != 0)
            {
                error = System.Runtime.InteropServices.Marshal.PtrToStringUTF8(buf) ?? "eval fallita";
                Status = "error"; Ok = false; return false;
            }
            LastBody = System.Runtime.InteropServices.Marshal.PtrToStringUTF8(buf) ?? "";
        }
        finally { System.Runtime.InteropServices.Marshal.FreeHGlobal(buf); }

        EvalTokBase = Basic.Providers.DistillNative.gmb_distill_evaltok(0);
        EvalPassBase = Basic.Providers.DistillNative.gmb_distill_evalpass(0);
        EvalTokDist = Basic.Providers.DistillNative.gmb_distill_evaltok(1);
        EvalPassDist = Basic.Providers.DistillNative.gmb_distill_evalpass(1);
        EvalAccBase = Basic.Providers.DistillNative.gmb_distill_evalacc(0);
        EvalAccDist = Basic.Providers.DistillNative.gmb_distill_evalacc(1);
        Ok = true; Status = "ok";
        return true;
    }

    public static double EvalTok(int idx) => idx == 1 ? EvalTokDist : EvalTokBase;
    public static double EvalPass(int idx) => idx == 1 ? EvalPassDist : EvalPassBase;
    public static double EvalAcc(int idx) => idx == 1 ? EvalAccDist : EvalAccBase;

    /// <summary>APPLY: verifica l'adapter (schema LORA APPLY) e, se opts.backup, salva il backup
    /// del checkpoint. opts: model (dir), dataset, backup (prefisso), ngen.</summary>
    public static bool Apply(string ckpt, object? opts, out string error)
    {
        error = "";
        if (string.IsNullOrWhiteSpace(ckpt)) { error = "prefisso ckpt mancante."; Status = "error:" + error; Ok = false; return false; }
        var model = ""; var dataset = ""; var backup = ""; var ngen = 6;
        if (opts is System.Collections.IDictionary d)
        {
            model = Opt(d, "model", model); dataset = Opt(d, "dataset", dataset);
            backup = Opt(d, "backup", backup); ngen = OptI(d, "ngen", ngen);
        }
        else if (opts is string json && !string.IsNullOrWhiteSpace(json) && json != "{}")
        {
            using var doc = JsonDocument.Parse(json);
            var r = doc.RootElement;
            model = Str(r, "model", model); dataset = Str(r, "dataset", dataset);
            backup = Str(r, "backup", backup); ngen = Int(r, "ngen", ngen);
        }
        if (string.IsNullOrWhiteSpace(model)) { error = "serve 'model' (dir del modello)."; Status = "error:" + error; Ok = false; return false; }
        var optsJson = JsonSerializer.Serialize(new Dictionary<string, object>
        { ["model"] = model, ["dataset"] = dataset, ["backup"] = backup, ["ngen"] = ngen });
        Status = "running";
        var buf = System.Runtime.InteropServices.Marshal.AllocHGlobal(8192);
        try
        {
            if (Basic.Providers.DistillNative.gmb_distill_apply_opts(ckpt, optsJson, buf, 8192) != 0)
            { error = System.Runtime.InteropServices.Marshal.PtrToStringUTF8(buf) ?? "apply fallita"; Status = "error"; Ok = false; return false; }
            LastBody = System.Runtime.InteropServices.Marshal.PtrToStringUTF8(buf) ?? "";
        }
        finally { System.Runtime.InteropServices.Marshal.FreeHGlobal(buf); }
        Ok = true; Status = "ok"; return true;
    }

    /// <summary>RESTORE: ricopia i file dal backup (prefisso) al checkpoint di origine.</summary>
    public static bool Restore(string backup, out string error)
    {
        error = "";
        if (string.IsNullOrWhiteSpace(backup)) { error = "prefisso backup mancante."; Status = "error:" + error; Ok = false; return false; }
        Status = "running";
        var buf = System.Runtime.InteropServices.Marshal.AllocHGlobal(8192);
        try
        {
            if (Basic.Providers.DistillNative.gmb_distill_restore_opts(backup, buf, 8192) != 0)
            { error = System.Runtime.InteropServices.Marshal.PtrToStringUTF8(buf) ?? "restore fallito"; Status = "error"; Ok = false; return false; }
            LastBody = System.Runtime.InteropServices.Marshal.PtrToStringUTF8(buf) ?? "";
        }
        finally { System.Runtime.InteropServices.Marshal.FreeHGlobal(buf); }
        Ok = true; Status = "ok"; return true;
    }

    /// <summary>TRAIN head-only (frozen default "all-but-head"). opts: model, holdout, ckpt, rank,
    /// layer, steps, batch, lr, seed, frozen.</summary>
    public static bool Train(string dataset, object? opts, out string error)
    {
        error = "";
        if (string.IsNullOrWhiteSpace(dataset)) { error = "path dataset mancante."; Status = "error:" + error; Ok = false; return false; }
        var model = "";
        var holdout = ""; var ckpt = "";
        var frozen = "all-but-head";
        var rank = 4; var layer = 15; var steps = 60; var batch = 4; var seed = 7; double lr = 0.01;
        var teacher = ""; double temperature = 2.0;   /* G5: teacher esterno -> distill KL */
        try
        {
            if (opts is System.Collections.IDictionary d)
            {
                model = Opt(d, "model", model); holdout = Opt(d, "holdout", holdout); ckpt = Opt(d, "ckpt", ckpt);
                frozen = Opt(d, "frozen", frozen); teacher = Opt(d, "teacher", teacher);
                rank = OptI(d, "rank", rank); layer = OptI(d, "layer", layer); steps = OptI(d, "steps", steps);
                batch = OptI(d, "batch", batch); seed = OptI(d, "seed", seed); lr = OptD(d, "lr", lr);
                temperature = OptD(d, "temperature", temperature);
            }
            else if (opts is string json && !string.IsNullOrWhiteSpace(json) && json != "{}")
            {
                using var doc = JsonDocument.Parse(json);
                var r = doc.RootElement;
                model = Str(r, "model", model); holdout = Str(r, "holdout", holdout); ckpt = Str(r, "ckpt", ckpt);
                frozen = Str(r, "frozen", frozen); teacher = Str(r, "teacher", teacher);
                rank = Int(r, "rank", rank); layer = Int(r, "layer", layer); steps = Int(r, "steps", steps);
                batch = Int(r, "batch", batch); seed = Int(r, "seed", seed); lr = Dbl(r, "lr", lr);
                temperature = Dbl(r, "temperature", temperature);
            }
        }
        catch (Exception ex) { error = "opzioni non valide: " + ex.Message; Status = "error:" + error; Ok = false; return false; }
        if (string.IsNullOrWhiteSpace(model)) { error = "serve 'model' (dir del modello)."; Status = "error:" + error; Ok = false; return false; }
        if (!string.IsNullOrWhiteSpace(teacher))
        {
            /* G5 — distill teacher-student (KL soft-logit): student=model, teacher=dir del teacher. */
            var req = JsonSerializer.Serialize(new Dictionary<string, object>
            {
                ["model_dir"] = model, ["teacher"] = teacher, ["dataset"] = dataset, ["ckpt"] = ckpt,
                ["rank"] = rank, ["layer"] = layer, ["steps"] = steps, ["batch"] = batch, ["lr"] = lr,
                ["temperature"] = temperature, ["seed"] = seed
            });
            Status = "running";
            var rb = new byte[8192];
            if (Basic.Providers.FinetuneNative.gmb_ai_distill_kl_exec(req, rb, rb.Length) != 0)
            { error = "distill KL fallita"; Status = "error"; Ok = false; return false; }
            LastBody = System.Text.Encoding.UTF8.GetString(rb).TrimEnd('\0');
            TrainCkpt = ckpt + "_best";
            Ok = true; Status = "ok"; return true;
        }
        var optsJson = JsonSerializer.Serialize(new Dictionary<string, object>
        {
            ["model"] = model, ["holdout"] = holdout, ["ckpt"] = ckpt, ["frozen"] = frozen,
            ["rank"] = rank, ["layer"] = layer, ["steps"] = steps, ["batch"] = batch, ["lr"] = lr, ["seed"] = seed
        });
        Status = "running";
        var buf = System.Runtime.InteropServices.Marshal.AllocHGlobal(8192);
        try
        {
            if (Basic.Providers.DistillNative.gmb_distill_train_opts(dataset, optsJson, buf, 8192) != 0)
            { error = System.Runtime.InteropServices.Marshal.PtrToStringUTF8(buf) ?? "train fallito"; Status = "error"; Ok = false; return false; }
            LastBody = System.Runtime.InteropServices.Marshal.PtrToStringUTF8(buf) ?? "";
        }
        finally { System.Runtime.InteropServices.Marshal.FreeHGlobal(buf); }
        TrainLoss = Basic.Providers.DistillNative.gmb_distill_loss();
        TrainHold = Basic.Providers.DistillNative.gmb_distill_hold();
        var cp = Basic.Providers.DistillNative.gmb_distill_ckpt();
        TrainCkpt = cp == IntPtr.Zero ? "" : (System.Runtime.InteropServices.Marshal.PtrToStringUTF8(cp) ?? "");
        TrainDone = (int)Basic.Providers.DistillNative.gmb_distill_done();
        Ok = true; Status = "ok"; return true;
    }

    public static string Report()
    {
        var buf = System.Runtime.InteropServices.Marshal.AllocHGlobal(1024);
        try
        {
            if (Basic.Providers.DistillNative.gmb_distill_report(buf, 1024) == 0)
                return System.Runtime.InteropServices.Marshal.PtrToStringUTF8(buf) ?? "";
            return "{\"error\":\"report non disponibile\"}";
        }
        finally { System.Runtime.InteropServices.Marshal.FreeHGlobal(buf); }
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
}
