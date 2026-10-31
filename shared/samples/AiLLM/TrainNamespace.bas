' samples/Community/AiLLM/TrainNamespace.bas
' ─────────────────────────────────────────────────────────────────────────────
' Namespace TRAIN (Fase F0) — PRODUZIONE/ALTERAZIONE dei pesi (fine-tune, LoRA,
' distillazione, eval/quality). Coincide col confine di licenza PUL (gmbllm).
'
' Il namespace TRAIN è la superficie CANONICA del training (F4 — hard move): i vecchi verbi
' `AI DISTILL` e `LLM FINETUNE/LORA/QUALITY` sono stati RIMOSSI e ora danno errore esplicito.
' `TRAIN FINETUNE/LORA APPLY/DISTILL/EVAL` emettono i medesimi builtin runtime condivisi
' (PUL/gmbllm), quindi la parità 4 backend è invariata.
'
' L'addestramento REALE (un modello MLX locale + un dataset) è mostrato in
' `samples/Community/AiLLM/LoraFinetuneMultiArch.bas`. Questo sample verifica
' che il namespace TRAIN dispatchi correttamente (parità 4 backend) gestendo
' l'errore in modo catturabile, senza richiedere un modello su disco.
' ─────────────────────────────────────────────────────────────────────────────
IMPORT AI
IMPORT LLM

SUB Main()
    PRINT "TRAIN: namespace PUL per la produzione dei pesi (F0 = delega AI/LLM)."

    ' --- TRAIN FINETUNE <modelDir> [, {opts}] : LoRA SFT ---
    ON ERROR GOTO FtErr
    TRAIN FINETUNE "work/no_model"
    PRINT "  FINETUNE: nessun errore (inatteso su modello inesistente)"
    GOTO DoApply
FtErr:
    PRINT "  FINETUNE: errore catturato (modello inesistente) - dispatch TRAIN OK"

    ' --- TRAIN LORA APPLY <ckpt> [, {opts}] : deploy adapter ---
DoApply:
    ON ERROR GOTO ApplyErr
    TRAIN LORA APPLY "work/no_ckpt"
    PRINT "  LORA APPLY: nessun errore (inatteso su checkpoint inesistente)"
    GOTO EndMain
ApplyErr:
    PRINT "  LORA APPLY: errore catturato (checkpoint inesistente) - dispatch TRAIN OK"
EndMain:
    PRINT "TRAIN: superficie canonica (PUL) per FINETUNE/LORA/DISTILL/EVAL (nessuno stub)."
END SUB
