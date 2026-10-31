// FILE: Basic.Providers.ChatTranscripts
// Chat template per i provider LLM embedded (MLX).
// PRIMARIO (automatico, 2026-09-19): `ResolveFromDir(modelDir)` carica il `chat_template` REALE
// del modello — da `tokenizer_config.json` (chiave `chat_template`) o dal file `chat_template.jinja`
// — e lo rende col motore Jinja in-repo (`common/Jinja`, esteso per coprire i template reali:
// macro/namespace/filtri/metodi). Cosi' OGNI modello funziona senza scrivere tabelle per-modello.
// FALLBACK: se il modello non ha template o l'engine non lo rende (JinjaError), si usa la tabella
// scritta a mano per `model_type` (`ByModelType`: llama/mixtral/deepseek_v4) e, in ultima istanza,
// ChatML. Estratto da MlxLlmCompleter (FASE 1b.5), 
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.Json;
using GmBasic.Jinja;

namespace Basic.Providers;

internal static class ChatTranscripts
{
    internal delegate string Builder(IReadOnlyList<(string Role, string Content)> messages, bool addAssistantOpener);

    internal readonly record struct Template(Builder Build, string DefaultEosToken);

    internal static readonly Template ChatMl = new(BuildChatMl, "<|im_end|>");

    /// <summary>Per-<c>model_type</c> chat templates -- lookup table di piccole funzioni scritte a
    /// mano, non un motore Jinja. Solo entry VERIFICATE contro un artefatto reale:
    /// - "llama": dalla tokenizer_config.json di Llama-3.2-1B-Instruct-4bit (forma core per-turno,
    ///   senza lo scaffold JSON dei tool e il boilerplate delle date);
    /// - "mixtral": formato <c>[INST]...[/INST]</c> di Mistral, stabile da anni.
    /// "qwen3_moe"/"qwen3"/"qwen2" cadono sul fallback ChatML (stessa funzione).</summary>
    internal static readonly Dictionary<string, Template> ByModelType = new()
    {
        ["llama"] = new Template(BuildLlama3, "<|eot_id|>"),
        ["mixtral"] = new Template(BuildMistral, "</s>"),
        ["deepseek_v4"] = new Template(BuildDeepSeek, "<｜end▁of▁sentence｜>"),
    };

    internal static Template Resolve(string? modelType) =>
        modelType is not null && ByModelType.TryGetValue(modelType, out var t) ? t : ChatMl;

    /// <summary>Legge <c>model_type</c> da <c>config.json</c> (se presente). Null (mai eccezioni)
    /// su qualsiasi errore: un config non riconosciuto degrada al fallback ChatML, non blocca il
    /// load del modello per una questione di templating.</summary>
    internal static string? ReadModelType(string modelDir)
    {
        try
        {
            var configPath = Path.Combine(modelDir, "config.json");
            if (!File.Exists(configPath)) return null;
            using var doc = JsonDocument.Parse(File.ReadAllText(configPath));
            return doc.RootElement.TryGetProperty("model_type", out var mt) ? mt.GetString() : null;
        }
        catch
        {
            return null;
        }
    }

    /// <summary>Risolve il chat template REALE del modello dalla sua cartella: prima
    /// <c>chat_template</c> in <c>tokenizer_config.json</c> (stringa, o prima stringa di una
    /// lista), poi il file <c>chat_template.jinja</c>. Il template viene renderizzato con
    /// <see cref="Jinja"/> e validato eager su un turno minimo: qualunque
    /// <see cref="JinjaError"/> o errore di I/O restituisce <c>null</c>, cosi' il chiamante
    /// degrada a <see cref="Resolve"/> (tabella scritta a mano), mai un crash al primo render.
    /// <c>tools</c> non viene passato di proposito (i template lo gestiscono con
    /// <c>default</c>/<c>if</c>).</summary>
    internal static Template? ResolveFromDir(string modelDir)
    {
        try
        {
            string? tpl = ReadChatTemplate(modelDir);
            if (string.IsNullOrEmpty(tpl)) return null;
            string? eos = ReadTokenString(modelDir, "eos_token");
            string? bos = ReadTokenString(modelDir, "bos_token");
            Builder builder = (messages, addAssistantOpener) =>
            {
                var vars = new Dictionary<string, object?>(StringComparer.Ordinal)
                {
                    ["messages"] = messages
                        .Select(m => (object?)new Dictionary<string, object?>(StringComparer.Ordinal)
                        {
                            ["role"] = m.Role,
                            ["content"] = m.Content,
                        })
                        .ToList(),
                    ["add_generation_prompt"] = addAssistantOpener,
                    ["bos_token"] = bos ?? "",
                    ["eos_token"] = eos ?? "",
                };
                return Jinja.Render(tpl, vars);
            };
            // Validazione EAGER su ENTRAMBI i render del path a sessione: [system, user] (turno
            // normale) e [system] (primo append). Alcuni template (es. qwen3_5) sollevano
            // "No user query found" sul render system-only: in quel caso si RIFIUTA il template
            // automatico (null) e si ricade per l'INTERO completer sulla tabella per model_type
            // (backup) / ChatML — mai mixare due formati nella stessa sessione KV.
            builder(new List<(string Role, string Content)> { ("user", "Ciao") }, true);
            builder(new List<(string Role, string Content)> { ("system", "SYS") }, false);
            return new Template(builder, eos ?? "");
        }
        catch (JinjaError) { return null; }
        catch { return null; }
    }

    /// <summary>Template per un modello GGUF single-file, dai meta letti dal motore
    /// (<c>general.architecture</c> + <c>tokenizer.chat_template</c> + eos/bos). DS4 (arch
    /// <c>deepseek4</c>/<c>deepseek_v41</c>) mantiene la tabella scritta a mano (ha il forward
    /// dedicato); un GGUF generico con chat_template REALE lo rende col motore Jinja; altrimenti
    /// ritorna null e il chiamante ricade su <see cref="Resolve"/> per model_type / ChatML.</summary>
    internal static Template? FromGgufMeta(string? arch, string? chatTemplate, string? eosToken, string? bosToken)
    {
        if (arch is "deepseek4" or "deepseek_v41") return ByModelType["deepseek_v4"];
        if (string.IsNullOrEmpty(chatTemplate)) return null;
        try
        {
            var tpl = chatTemplate;
            var eos = eosToken ?? "";
            var bos = bosToken ?? "";
            Builder builder = (messages, addAssistantOpener) =>
            {
                var vars = new Dictionary<string, object?>(StringComparer.Ordinal)
                {
                    ["messages"] = messages
                        .Select(m => (object?)new Dictionary<string, object?>(StringComparer.Ordinal)
                        {
                            ["role"] = m.Role,
                            ["content"] = m.Content,
                        })
                        .ToList(),
                    ["add_generation_prompt"] = addAssistantOpener,
                    ["bos_token"] = bos,
                    ["eos_token"] = eos,
                };
                return Jinja.Render(tpl, vars);
            };
            // Validazione eager (come ResolveFromDir): se il template non rende su un turno
            // minimo, si rifiuta e il chiamante usa il fallback.
            builder(new List<(string Role, string Content)> { ("user", "Ciao") }, true);
            builder(new List<(string Role, string Content)> { ("system", "SYS") }, false);
            return new Template(builder, eos);
        }
        catch (JinjaError) { return null; }
        catch { return null; }
    }

    private static string? ReadChatTemplate(string modelDir)
    {
        try
        {
            var tc = Path.Combine(modelDir, "tokenizer_config.json");
            if (File.Exists(tc))
            {
                using var doc = JsonDocument.Parse(File.ReadAllText(tc));
                if (doc.RootElement.TryGetProperty("chat_template", out var ct))
                {
                    if (ct.ValueKind == JsonValueKind.String)
                    {
                        var s = ct.GetString();
                        if (!string.IsNullOrEmpty(s)) return s;
                    }
                    else if (ct.ValueKind == JsonValueKind.Array)
                    {
                        foreach (var el in ct.EnumerateArray())
                        {
                            if (el.ValueKind == JsonValueKind.String)
                            {
                                var s = el.GetString();
                                if (!string.IsNullOrEmpty(s)) return s;
                            }
                            if (el.ValueKind == JsonValueKind.Object &&
                                el.TryGetProperty("template", out var t) && t.ValueKind == JsonValueKind.String)
                            {
                                var s = t.GetString();
                                if (!string.IsNullOrEmpty(s)) return s;
                            }
                        }
                    }
                }
            }
        }
        catch { /* si prova il file .jinja */ }
        var jf = Path.Combine(modelDir, "chat_template.jinja");
        return File.Exists(jf) ? File.ReadAllText(jf) : null;
    }

    private static string? ReadTokenString(string modelDir, string key)
    {
        foreach (var file in new[] { "tokenizer_config.json", "config.json" })
        {
            var p = Path.Combine(modelDir, file);
            if (!File.Exists(p)) continue;
            try
            {
                using var doc = JsonDocument.Parse(File.ReadAllText(p));
                if (!doc.RootElement.TryGetProperty(key, out var el)) continue;
                if (el.ValueKind == JsonValueKind.String) return el.GetString();
                if (el.ValueKind == JsonValueKind.Object &&
                    el.TryGetProperty("content", out var c) && c.ValueKind == JsonValueKind.String)
                    return c.GetString();
            }
            catch { /* token opzionale */ }
        }
        return null;
    }

    /// <summary>Qwen ChatML, costruito a mano -- combacia con
    /// <c>tokenizer.apply_chat_template(..., add_generation_prompt=True)</c> verificato in mlx-lm.
    /// Transcript arbitrario (come apply_template di llama.cpp), così una sessione cresce turno
    /// per turno. Fallback per ogni model_type non in <see cref="ByModelType"/>.</summary>
    internal static string BuildChatMl(IReadOnlyList<(string Role, string Content)> messages, bool addAssistantOpener)
    {
        var sb = new StringBuilder();
        foreach (var (role, content) in messages)
            sb.Append("<|im_start|>").Append(role).Append('\n').Append(content).Append("<|im_end|>\n");
        if (addAssistantOpener)
            sb.Append("<|im_start|>assistant\n");
        return sb.ToString();
    }

    /// <summary>Llama 3.x: <c>&lt;|begin_of_text|&gt;</c> una volta, poi
    /// <c>&lt;|start_header_id|&gt;{role}&lt;|end_header_id|&gt;\n\n{content}&lt;|eot_id|&gt;</c> per
    /// turno (forma core del template ufficiale, senza scaffold tool/date).</summary>
    internal static string BuildLlama3(IReadOnlyList<(string Role, string Content)> messages, bool addAssistantOpener)
    {
        var sb = new StringBuilder("<|begin_of_text|>");
        foreach (var (role, content) in messages)
            sb.Append("<|start_header_id|>").Append(role).Append("<|end_header_id|>\n\n").Append(content).Append("<|eot_id|>");
        if (addAssistantOpener)
            sb.Append("<|start_header_id|>assistant<|end_header_id|>\n\n");
        return sb.ToString();
    }

    /// <summary>DeepSeek V4 Flash (DS4): forma semplificata (thinking=false) del chat_template
    /// nel GGUF. Token speciali (id): <c>&lt;｜begin▁of▁sentence｜&gt;</c> (0, BOS),
    /// <c>&lt;｜end▁of▁sentence｜&gt;</c> (1, EOS), <c>&lt;｜User｜&gt;</c> (128803),
    /// <c>&lt;｜Assistant｜&gt;</c> (128804). Layout: BOS, [system quasi letterale prima del primo
    /// turno], poi per ogni turno <c>&lt;｜User｜&gt;{user}</c> e
    /// <c>&lt;｜Assistant｜&gt;{assistant}&lt;｜end▁of▁sentence｜&gt;</c>; il generation prompt è un
    /// turno user non terminato seguito da <c>&lt;｜Assistant｜&gt;</c>.</summary>
    internal static string BuildDeepSeek(IReadOnlyList<(string Role, string Content)> messages, bool addAssistantOpener)
    {
        var sb = new StringBuilder("<｜begin▁of▁sentence｜>");
        string? pendingSystem = null;
        foreach (var (role, content) in messages)
        {
            if (role == "system") { pendingSystem = content; continue; }
            if (role is "user" or "tool")
            {
                sb.Append("<｜User｜>");
                if (pendingSystem is not null) { sb.Append(pendingSystem); pendingSystem = null; }
                sb.Append(content);
            }
            else if (role == "assistant")
            {
                sb.Append("<｜Assistant｜>").Append(content).Append("<｜end▁of▁sentence｜>");
            }
        }
        if (addAssistantOpener)
            sb.Append("<｜Assistant｜>");
        return sb.ToString();
    }

    /// <summary>Mistral/Mixtral: formato classico <c>[INST]...[/INST]</c>. Non c'è sintassi dedicata
    /// per il system: un messaggio di sistema è bufferizzato e preposto al successivo turno
    /// <c>[INST]</c>. <c>addAssistantOpener</c> è no-op (un turno user/tool termina già con
    /// <c>[/INST]</c>, che è il punto di continuazione dell'assistant).</summary>
    internal static string BuildMistral(IReadOnlyList<(string Role, string Content)> messages, bool addAssistantOpener)
    {
        var sb = new StringBuilder("<s>");
        string? pendingSystem = null;
        foreach (var (role, content) in messages)
        {
            if (role == "system") { pendingSystem = content; continue; }
            if (role is "user" or "tool")
            {
                sb.Append("[INST] ");
                if (pendingSystem is not null)
                {
                    sb.Append(pendingSystem).Append("\n\n");
                    pendingSystem = null;
                }
                sb.Append(content).Append(" [/INST]");
            }
            else if (role == "assistant")
            {
                sb.Append(content).Append("</s>");
            }
        }
        return sb.ToString();
    }
}
