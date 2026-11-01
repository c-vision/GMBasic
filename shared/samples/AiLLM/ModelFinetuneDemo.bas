' PARITY-EXCLUDE: carica un modello LOCALE; solo manuale (sentinella).
'' FILE: Ai/ModelFinetuneDemo.bas
' ==============================================================================
'       TRAIN FINETUNE — fine-tune LoRA su un modello MLX locale (superficie BASIC)
' ==============================================================================
' Pattern: il comando (mai parentesi) avvia l'addestramento LoRA di un modello
' MLX locale su un dataset a finestre+next-token; lo stato/esito si leggono con le
' funzioni LLM.FINETUNE* (parentesi). Il training gira sull'harness nativo come
' processo esterno (nessun ponte in-process); il checkpoint <ckpt>_best_{A,B}.raw
' viene salvato per il deploy di verifica.
'
' PREREQUISITI:
'   1) un modello MLX locale (di default quello sotto; cambiare il path);
'   2) il dataset a finestre (regola "keyword NAMESPACE/runtime -> vault") vive in
'      samples/Fixtures/lora_rule.{train,hold}.bin (fixture versionata, MAI scritta
'      dall'esecuzione). Per rigenerarla da altri docs:
'        python3 samples/Fixtures/make_lora_dataset.py <modelDir> <out> rule
'      (il training scrive SOLO in work/: checkpoint <ckpt>_best_{A,B}.raw + report).
' La CWD al run deve essere la cartella del sample (AGENTS §9).
'
' ESITO atteso (vedi gate P3 nel vault todo MLX-Finetune-LoRA-Modelli-Locali):
'   LLM.FINETUNEDONE() = -1, loss train/hold in calo; il greedy su un prompt della
'   classe A punta al token appreso. Eseguibile su IL e PCodeVM.
' NB: superficie TRAIN FINETUNE (vedi Piani/(partial) - MLX-Finetune-Superficie-BASIC.md).

IMPORT AI WITH ALIAS

IMPORT LLM

SUB Main()
    PRINT "=== TRAIN FINETUNE (LoRA su modello MLX locale) ==="

    DIM model$ = "~/ai/Models/Llama-3.2-1B-Instruct-4bit"

    ' Avvia il fine-tune: dataset dalla fixture samples/Fixtures/lora_rule.*.bin,
    ' NB: le chiavi del dict vanno tra virgolette (le chiavi nude multiple sono un
    ' bug noto del parser -> IL invalido; indagare separatamente).
    TRAIN FINETUNE model$, _
                { "dataset": "../Fixtures/lora_rule.train.bin", "holdout": "../Fixtures/lora_rule.hold.bin",
                  "ckpt": "work/ft1", "rank": 4, "layer": 15, "steps": 60,
                  "batch": 4, "lr": 0.01, "seed": 7, "verify": TRUE }

    ' Stato/esito (funzioni di lettura, parentesi)
    IF LLM.FINETUNEDONE() = -1 THEN
        PRINT "Fine-tune completato:"
        PRINT "  train loss = "; LLM.FINETUNELOSS()
        PRINT "  hold  loss = "; LLM.FINETUNEHOLD()
        PRINT "  checkpoint = "; LLM.FINETUNECKPT()
        PRINT "  greedy base = "; LLM.FINETUNEGREEDY()
        ' B3: misure che certificano l'apprendimento (baseline vs dopo).
        PRINT "  greedy after = "; LLM.FINETUNEGREEDYAFTER()
        PRINT "  demo changed = "; LLM.FINETUNEDEMOCHANGED()
        PRINT "  baseline train = "; LLM.FINETUNEBASETRAIN()
        PRINT "  baseline hold = "; LLM.FINETUNEBASEHOLD()
        PRINT "MODEL-FINETUNE-OK"
    ELSE
        PRINT "Fine-tune non riuscito: "; LLM.FINETUNESTATUS()
        PRINT "MODEL-FINETUNE-FAIL"
    END IF

    ' Deploy/verifica: ricarica il checkpoint e confronta la generazione OFF vs ON
    ' su un prompt dal dataset (regola di dominio -> l'adapter deve cambiare il token).
    TRAIN LORA APPLY "work/ft1", _
                { "model": model$, "dataset": "../Fixtures/lora_rule.train.bin" }
    PRINT "Apply status: "; LLM.FINETUNESTATUS()
END SUB
