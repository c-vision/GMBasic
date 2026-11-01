' samples/AiLLM/TrainRuns.bas
' ─────────────────────────────────────────────────────────────────────────────
' TRAIN RUNS (F1) — registro dei job di training/apply.
'
' `LLM.RUNSCOUNT()`  -> numero di run registrate (training + apply).
' `LLM.RUNSJSON()`   -> JSON {"count":N,"runs":[{id,kind,status,train,hold,
'                       baseTrain,baseHold,greedyBefore,greedyAfter,
'                       demoChanged,ckpt}, ...]}.
'
' Ogni `TRAIN FINETUNE` / `TRAIN LORA APPLY` (o i corrispettivi `LLM …`) registra una
' run (ok o error). Il registro è per-processo e la loro storia è identica su tutti e
' 5 i backend (IL, PCodeVM, VM/C, C99Native).
'
' Questo sample non avvia training (nessun modello): mostra la query su registro vuoto,
' verificabile su tutti i backend senza pesi. L'addestramento reale che POPOLA il
' registro è in `LoraFinetuneMultiArch.bas`.
' ─────────────────────────────────────────────────────────────────────────────
IMPORT LLM

SUB Main()
    PRINT "TRAIN RUNS — registro job (vuoto a inizio processo)."
    PRINT "run registrate: "; LLM.RUNSCOUNT()

    DIM js AS STRING = LLM.RUNSJSON()
    PRINT "JSON registro : "; js

    IF INSTR(js, "count") > 0 AND INSTR(js, "runs") > 0 THEN
        PRINT "TRAIN RUNS: query disponibile su tutti i 4 backend."
    ELSE
        PRINT "TRAIN RUNS: JSON inatteso."
    END IF
END SUB
