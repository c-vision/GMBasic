' samples/AiLLM/TrainDpo.bas
' ─────────────────────────────────────────────────────────────────────────────
' TRAIN DPO (F3) — DIRECT PREFERENCE OPTIMIZATION.
'
' Allena un adapter LoRA dalle coppie chosen/rejected con la loss DPO standard
' (reference = adapter OFF); gradiente SPSA (stesso motore del fine-tune LoRA).
'
'   TRAIN DPO <model>, { "chosen": <ds.bin>, "rejected": <ds.bin>,
'                        "ckpt": <prefisso>, rank, layer, steps, batch, lr, beta }
'
' `chosen`/`rejected` sono due dataset nel formato .bin del distill (token ids).
' Salva <ckpt>_best_A.raw / <ckpt>_best_B.raw / <ckpt>_best.meta.
' ─────────────────────────────────────────────────────────────────────────────
IMPORT AI

SUB Main()
    PRINT "TRAIN DPO — preference optimization (coppie chosen/rejected)."

    ' Esempio reale richiede un modello MLX locale + i due dataset .bin.
    ' Qui si esegue il percorso d'errore (modello inesistente) — dispatch + gestione.
    ON ERROR GOTO Caught
    TRAIN DPO "work/no_model", { "chosen": "work/no_c.bin", "rejected": "work/no_r.bin", "steps": 8 }
    PRINT "TRAIN DPO: nessun errore (inatteso senza modello)."
    GOTO EndMain
Caught:
    PRINT "TRAIN DPO: errore catturato (modello/dataset inesistenti) — dispatch TRAIN OK."
EndMain:
    PRINT "TRAIN DPO: adapter in <ckpt>_best_{A,B}.raw (run reale con modello+dataset)."
END SUB
