' PARITY-EXCLUDE: heavy — carica modelli LOCALI + training (minuti); solo manuale.
' FILE: AiLLM/LoraFinetuneMultiArch.bas
' ==============================================================================
'   TRAIN FINETUNE — fine-tune LoRA su OGNI architettura addestrabile supportata
' ==============================================================================
' PARAMETRIZZATO: il modello e i path passano da GLOBALI + text-substitution Clipper
' ("&gModel" dentro una stringa espande la variabile GLOBALE). Gli argomenti dei COMANDI
' LLM restano stringhe letterali ma con la sostituzione: `TRAIN FINETUNE "&gModel", {...}`.
' (La sostituzione "&var" vale per le GLOBALI — commit clipper-d2-d3 48548a986.)
'
' Il trainer risolve H/VOCAB/VIN/VOUT a runtime dal modello (config.json + header
' safetensors) e AUTO-RILEVA il modulo LoRA target (v_proj -> kv_b_proj -> q_proj -> ...):
' lo stesso comando funziona su arch diverse (llama/qwen/deepseek MLA/gemma/glm/gpt-oss/kimi).
'
' PREREQUISITI (per ogni modello che vuoi addestrare):
'   1) il modello MLX locale in ~/ai/Models/<nome>;
'   2) il dataset a finestre del SUO tokenizer (gli id dipendono dal vocab):
'        python3 samples/Fixtures/make_lora_dataset.py <modelDir> work/<LABEL> rule
'      -> work/<LABEL>.train.bin + work/<LABEL>.hold.bin
'   3) sentinella anti-saturazione: touch ~/ai/Models/.gmbasic-allow-load
' Per aggiungere un modello: una riga RunOne(<dir>, "<LABEL>") in Main.

IMPORT AI WITH ALIAS
IMPORT LLM

' Globali usate dalla text-substitution nei comandi LLM (le locali non sono espanse).
GLOBAL gModel AS STRING
GLOBAL gDataset AS STRING
GLOBAL gHold AS STRING
GLOBAL gCk AS STRING

' Un modello: fine-tune LoRA + deploy/verifica. mdir=path, mname=nome logico (LABEL).
SUB RunOne(mdir AS STRING, mname AS STRING)
    gModel = mdir
    gDataset = "work/" & mname & ".train.bin"
    gHold = "work/" & mname & ".hold.bin"
    gCk = "work/lora_" & mname
    PRINT "--- " & mname & " ---"
    IF NOT IO.FILEEXISTS(mdir & "/config.json") THEN
        PRINT "  (modello assente, skip)"
        EXIT SUB
    END IF
    IF NOT IO.FILEEXISTS(gDataset) THEN
        PRINT "  (dataset assente; genera: make_lora_dataset.py " & mdir & " work/" & mname & " rule)"
        EXIT SUB
    END IF

    ' Fine-tune LoRA: comando (mai parentesi); dim/modulo auto-rilevati dal modello.
    ' Gli argomenti sono letterali con text-substitution "&g…" (globali).
    TRAIN FINETUNE "&gModel", { "dataset": "&gDataset", "holdout": "&gHold", "ckpt": "&gCk", _
                             "rank": 4, "layer": 6, "steps": 40, "batch": 2, _
                             "lr": 0.02, "seed": 7 }
    IF LLM.FINETUNEDONE() = -1 THEN
        PRINT "  train="; LLM.FINETUNELOSS(); " base="; LLM.FINETUNEBASETRAIN(); _
              " changed="; LLM.FINETUNEDEMOCHANGED(); " ckpt="; LLM.FINETUNECKPT()
    ELSE
        PRINT "  non riuscito: "; LLM.FINETUNESTATUS()
    END IF
    ' Deploy/verifica: ricarica l'adapter e confronta la generazione OFF vs ON.
    TRAIN LORA APPLY "&gCk", { "model": "&gModel", "dataset": "&gDataset" }
    PRINT "  apply: "; LLM.FINETUNESTATUS()
    PRINT ""
END SUB

SUB Main()
    PRINT "=== TRAIN FINETUNE — tour multi-architettura (parametrizzato) ==="
    IF NOT IO.FILEEXISTS("~/ai/Models/.gmbasic-allow-load") THEN
        PRINT "Sentinella assente: touch ~/ai/Models/.gmbasic-allow-load"
        END
    END IF
    MKDIR "work"

    RunOne("~/ai/Models/Llama-3.2-1B-Instruct-4bit", "Llama321B")
    RunOne("~/ai/Models/Qwen3-8B-4bit", "Qwen38B")
    RunOne("~/ai/Models/DeepSeek-R1-Distill-Qwen-32B-MLX-8Bit", "DSR1Qwen32B")
    RunOne("~/ai/Models/DeepSeek-V2-Lite-Chat-4bit-mlx", "DSV2Lite")
    RunOne("~/ai/Models/gemma-4-31b-it-4bit", "Gemma431B")
    RunOne("~/ai/Models/GLM-4.5-Air-4bit", "GLM45Air")
    RunOne("~/ai/Models/gpt-oss-20b-MXFP4-Q8", "GptOss20B")
    RunOne("~/ai/Models/Kimi-Linear-48B-A3B-Instruct-4bit", "KimiLinear48B")
    RunOne("~/ai/Models/Qwen3-Coder-30B-A3B-Instruct-8bit", "Qwen3Coder30B")
    RunOne("~/ai/Models/Qwen3.6-27B-4bit", "Qwen36_27B")
    RunOne("~/ai/Models/Ternary-Bonsai-4B-mlx-2bit", "TernaryBonsai4B")
    RunOne("~/ai/Models/Qwen3-Coder-Next-mxfp4-mlx", "Qwen3CoderNext")

    PRINT "LANCIA-OK lora_finetune_multiar"
END SUB
