' PARITY-EXCLUDE: carica un modello LOCALE (self-distillation); esempio manuale (sentinella).
'' FILE: AiLLM/DistillPipeline.bas
' ==============================================================================
'   TRAIN DISTILL — pipeline di self-distillation lm_head-only (superficie BASIC)
' ==============================================================================
' Dimostra TUTTA la superficie Stream D, in quest'ordine:
'   BUILD  : jsonl di coppie (prompt,risposta) -> dataset .bin train/hold
'   TRAIN  : adapter head-only (frozen "all-but-head") sul modello, ckpt A/B + meta
'   SAMPLE : raccolta di k risposte per prompt + filtro VERIFY -> jsonl di coppie
'   EVAL   : valutazione paired (idx 0 = base, idx 1 = modello+adapter)
'   APPLY  : verifica l'adapter + backup del checkpoint
'   RESTORE: ripristina il checkpoint dal backup
' Comandi legacy (mai parentesi); stato letto con le funzioni LLM.DISTILL*().
'
' PREREQUISITI:
'   - un modello MLX locale (default Llama-3.2-1B-Instruct-4bit; cambiare il path);
'   - il dataset fixture samples/Fixtures/ai/gsm8k_300.jsonl (campi question/answer);
'   - abilitare il caricamento:  touch /Users/gaetano/ai/Models/.gmbasic-allow-load
' La CWD al run deve essere la cartella del sample (AGENTS §9): la pipeline scrive in work/.
'
' NOTA: il comando e' `TRAIN DISTILL ...`, le funzioni sono `LLM.DISTILL*()`.

IMPORT AI
IMPORT LLM
IMPORT IO

SUB Main()
    PRINT "=== TRAIN DISTILL — pipeline self-distillation ==="

    DIM model AS STRING = "/Users/gaetano/ai/Models/Llama-3.2-1B-Instruct-4bit"
    IF NOT IO.FILEEXISTS("/Users/gaetano/ai/Models/.gmbasic-allow-load") THEN
        PRINT "Modello NON caricato (protezione anti-saturazione)."
        PRINT "Per eseguire:  touch /Users/gaetano/ai/Models/.gmbasic-allow-load  e rilancia."
        END
    END IF
    IF NOT IO.FILEEXISTS(model + "/tokenizer.json") THEN
        PRINT "Modello assente: "; model
        END
    END IF

    DIM jsonl AS STRING = "../../Fixtures/ai/gsm8k_300.jsonl"
    DIM ds AS STRING = "work/distill_ds"
    DIM ckpt AS STRING = "work/distill_ckpt"
    DIM pairs AS STRING = "work/distill_pairs.jsonl"
    DIM backup AS STRING = "work/distill_head.orig"

    ' ---- 1) BUILD: jsonl -> work/distill_ds.train.bin + .hold.bin ----
    TRAIN DISTILL BUILD jsonl, { "model": model, "out": ds, "window": 16, "holdout": 0.2, "max": 40 }
    PRINT "BUILD  : "; LLM.DISTILLSTATUS()
    IF NOT IO.FILEEXISTS(ds + ".train.bin") THEN
        PRINT "BUILD-FAIL"
        END
    END IF

    ' ---- 2) TRAIN head-only (frozen "all-but-head") ----
    TRAIN DISTILL TRAIN ds + ".train.bin", _
        { "model": model, "holdout": ds + ".hold.bin", "ckpt": ckpt, _
          "frozen": "all-but-head", "rank": 4, "steps": 20, "batch": 2, "lr": 0.01, "seed": 7 }
    PRINT "TRAIN  : done="; LLM.DISTILLDONE(); " loss="; LLM.DISTILLLOSS(); " hold="; LLM.DISTILLHOLD()
    PRINT "         ckpt="; LLM.DISTILLCKPT()

    ' ---- 3) SAMPLE: k risposte per prompt + filtro VERIFY ----
    TRAIN DISTILL SAMPLE jsonl, _
        { "model": model, "out": pairs, "k": 2, "temp": 0.7, "maxlen": 48, "verify": "rule" }
    PRINT "SAMPLE : samplen="; LLM.DISTILLSAMPLEN(); " samplefail="; LLM.DISTILLSAMPLEFAIL(); _
          " sampledone="; LLM.DISTILLSAMPLEDONE()

    ' ---- 4) EVAL paired: idx 0 = base, idx 1 = modello+adapter ----
    TRAIN DISTILL EVAL jsonl, _
        { "model": model, "ckpt": ckpt, "out": "work/distill_eval.json", _
          "repeat": 2, "temp": 0.7, "maxlen": 48, "verify": "rule" }
    PRINT "EVAL   : baseTok="; LLM.DISTILLEVALTOK(0); " basePass="; LLM.DISTILLEVALPASS(0); " baseAcc="; LLM.DISTILLEVALACCEPT(0)
    PRINT "         distTok="; LLM.DISTILLEVALTOK(1); " distPass="; LLM.DISTILLEVALPASS(1); " distAcc="; LLM.DISTILLEVALACCEPT(1)

    ' ---- 5) APPLY: verifica adapter + backup del checkpoint ----
    TRAIN DISTILL APPLY ckpt, { "model": model, "dataset": ds + ".train.bin", "backup": backup }
    PRINT "APPLY  : "; LLM.DISTILLSTATUS()

    ' ---- 6) RESTORE: ripristina il checkpoint dal backup ----
    TRAIN DISTILL RESTORE backup
    PRINT "RESTORE: fatto (backup "; backup; ")"

    PRINT "DISTILL-PIPELINE-OK"
END SUB
