// FILE: Basic.Providers.FinetuneNative — binding P/Invoke al fine-tune LoRA (dylib gmb_ai_finetune).
//
// PERCHÉ P/Invoke (AGENTS, REGOLA PRIMARIA): il training LoRA è logica C pesante (SPSA+AdamW,
// loss CE full-vocab via Accelerate, adapter nel forward nativo MLX). Il C# la CHIAMA in-process
// invece di spawnare l'eseguibile `gmb_mlx_finetune`/`gmb_mlx_lora_apply`.
//
// Sorgente C: native/gmb_ai_finetune/gmb_ai_finetune.c. Dylib in Resources/libs/<rid>/, risolta
// da Basic.Providers.AiNativeResolver (prefisso gmb_).
using System.Runtime.InteropServices;

namespace Basic.Providers;

public static partial class FinetuneNative
{
    private const string LibName = "gmb_ai_finetune";

    static FinetuneNative()
    {
        AiNativeResolver.EnsureRegistered(typeof(FinetuneNative).Assembly);
    }

    /// <summary>Fine-tune LoRA: request JSON in ingresso, response JSON in `resp`. 0 = ok.</summary>
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_ai_finetune_exec(string request, byte[] resp, int cap);

    /// <summary>Apply/verifica di un adapter: stesso contratto. 0 = ok.</summary>
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_ai_lora_apply_exec(string request, byte[] resp, int cap);

    /// <summary>TRAIN DPO: preference optimization (coppie chosen/rejected). Stesso contratto. 0 = ok.</summary>
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_ai_dpo_exec(string request, byte[] resp, int cap);
    /// <summary>TRAIN GRPO: RLHF senza critic (RLVR). Stesso contratto. 0 = ok.</summary>
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_ai_grpo_exec(string request, byte[] resp, int cap);

    /// <summary>G5 — DISTILL teacher-student (KL soft-logit): request {model_dir,teacher,dataset,ckpt,
    /// rank,layer,steps,batch,lr,temperature,seed}. Response JSON in `resp`. 0 = ok.</summary>
    [LibraryImport(LibName, StringMarshalling = StringMarshalling.Utf8)]
    public static partial int gmb_ai_distill_kl_exec(string request, byte[] resp, int cap);
}
