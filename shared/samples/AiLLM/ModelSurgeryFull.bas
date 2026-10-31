' PARITY-EXCLUDE: model surgery su modello LOCALE pesante (sentinella anti-saturazione).
' FILE: AiLLM/ModelSurgeryFull.bas
' =====================================================================================
' MODEL SURGERY §6 (criterio di chiusura) — ciclo COMPLETO: RIMUOVERE + AGGIUNGERE +
' MODIFICARE + VALIDATE + DIFF + ROLLBACK, con generazione finale e DELTA misurabile.
'
'   0. CHECKPOINT   : punto di rollback
'   1. PRUNE        : RIMUOVERE 64 -> 56 esperti (router coerente)
'   2. ADD          : AGGIUNGERE (ri-compone gli esperti rimossi dall'originale)
'   3. ABLATE       : MODIFICARE (azzera un blocco per pattern/ruolo)
'   4. MODEL SAVE   : EMITTER (artefatto finale)
'   5. VALIDATE     : integrita' strutturale
'   6. DIFF         : degrado misurabile (originale vs finale)
'   7. CONVERT+gen  : il modello trasformato carica e genera
'   8. ROLLBACK     : ripristino dal checkpoint
'
' Prerequisito: work/dsv2l_mxfp4.gguf (dalla catena ModelCompilerMlxToDs4.bas).
' =====================================================================================
IMPORT AI WITH ALIAS
IMPORT LLM

SUB Main()
    PRINT "=== Model Surgery §6 — ciclo completo (remove+add+modify+validate+rollback) ==="
    IF NOT IO.FILEEXISTS("work/dsv2l_mxfp4.gguf") THEN
        PRINT "assente work/dsv2l_mxfp4.gguf (esegui prima ModelCompilerMlxToDs4.bas)"
        END
    END IF

    PRINT "0) CHECKPOINT (punto di rollback)"
    LLM MODEL CHECKPOINT "work/dsv2l_mxfp4.gguf", "work/ck", "orig"

    PRINT "1) PRUNE (RIMUOVERE) 64 -> 56 esperti"
    LLM PRUNE "work/dsv2l_mxfp4.gguf", "work/f_p56.gguf", 56

    PRINT "2) ADD (AGGIUNGERE) — ri-compone gli esperti dall'originale"
    LLM MODEL ADD "work/f_p56.gguf", "work/dsv2l_mxfp4.gguf", "work/f_composed.gguf"

    PRINT "3) ABLATE (MODIFICARE) — azzera gli esperti del layer 10"
    LLM MODEL ABLATE "work/f_composed.gguf", "work/f_mod.gguf", "layers.10.mlp.switch_mlp"

    PRINT "4) MODEL SAVE (EMITTER)"
    LLM MODEL SAVE "work/f_mod.gguf", "work/f_final.gguf"

    PRINT "5) VALIDATE (integrita')"
    LLM MODEL VALIDATE "work/f_final.gguf", "work/f_validate.txt"

    PRINT "6) DIFF (degrado misurabile: originale vs finale)"
    LLM MODEL DIFF "work/dsv2l_mxfp4.gguf", "work/f_final.gguf", "work/f_diff.txt"

    PRINT "7) CONVERT GGUF -> MLX e genera col modello trasformato"
    LLM MODEL CONVERT "work/f_final.gguf", "work/f_final_mlx", "MLX"
    AI REGISTER "sfull", "completion", "work/f_final_mlx", "{""engine"":""mlx"",""swap"":""true"",""maxTokens"":""32"",""temperature"":""0"",""contextSize"":""2048"",""eosToken"":""<｜end▁of▁sentence｜>""}"
    AI SET ACTIVE "completion", "sfull"
    AI AGENT CREATE "ag"
    AI AGENT SET "ag", maxtokens, "32"
    DIM r$ AS STRING
    r$ = AI.AGENTASKSYNC("ag", "Rispondi in una riga in italiano: qual e' la capitale dell'Italia?")
    PRINT "RISPOSTA: "; r$
    AI AGENT DELETE "ag"

    PRINT "8) ROLLBACK (ripristino dal checkpoint) + VALIDATE"
    LLM MODEL ROLLBACK "work/ck", "orig", "work/f_restored.gguf"
    LLM MODEL VALIDATE "work/f_restored.gguf", "work/f_restore_val.txt"
    PRINT "OK: ciclo completo (remove+add+modify) + validate + diff + rollback, con generazione."
END SUB
