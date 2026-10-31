' samples/Community/AiLLM/TrainCommandsTour.bas
' ─────────────────────────────────────────────────────────────────────────────
' TOUR COMPLETO del namespace TRAIN: usa OGNI comando/funzione implementato.
'
'   TRAIN DATASET BUILD|SAMPLE      (preparazione dataset)
'   TRAIN DISTILL BUILD|SAMPLE      (self-distillation)
'   TRAIN FINETUNE / TRAIN RESUME   (fine-tune LoRA SFT)
'   TRAIN LORA APPLY|MERGE|SAVE|LOAD
'   TRAIN EVAL / TRAIN QUALITY      (NLL/perplexity + gate)
'   TRAIN DPO                       (preference optimization)
'   LLM.DATASETSTATS / LLM.TRAINCONFIG / LLM.TRAINCONFIGSET
'   LLM.RUNSJSON / LLM.RUNSCOUNT / LLM.RUNSITEM / LLM.RUNSCLEAR / LLM.LORALIST
'
' I comandi che richiedono un modello MLX locale sono eseguiti con un handler ON ERROR
' dedicato (modello inesistente) per mostrarne la sintassi senza pesi; l'addestramento
' reale è in LoraFinetuneMultiArch.bas / TrainDpo.bas.
' ─────────────────────────────────────────────────────────────────────────────
IMPORT AI
IMPORT LLM
IMPORT IO

SUB Main()
    PRINT "=== TRAIN namespace — tour completo ==="

    ' --- TRAIN DATASET BUILD / DISTILL BUILD (dataset dai template, senza modello) ---
    DIM fixdir AS STRING = PROJECTFOLDER() + "/../../Fixtures/ai"
    DIM tok AS STRING = fixdir + "/tiny_tokenizer.json"
    DIM jp AS STRING = "work/train_tour.jsonl"
    DIM ds AS STRING = "work/train_tour_ds"

    DIM h AS LONG = IO.FCREATE(jp)
    IO.FWRITE(h, "{""prompt"": ""hello world"", ""response"": ""two plus two""}" + CHR(10))
    IO.FCLOSE(h)

    TRAIN DATASET BUILD jp, { "tokenizer": tok, "out": ds, "window": 4, "holdout": 0.5 }
    TRAIN DISTILL BUILD jp, { "tokenizer": tok, "out": "work/train_tour_dst", "window": 4, "holdout": 0.5 }

    ' --- TRAIN DATASET STATS <file> (comando: stampa n/esempi, token, distribuzione target) ---
    TRAIN DATASET STATS ds + ".train.bin"

    ' --- funzioni LLM (dataset/config) ---
    PRINT "DATASETSTATS : "; LLM.DATASETSTATS(ds + ".train.bin")
    PRINT "TRAINCONFIG  : "; LLM.TRAINCONFIG()
    LLM.TRAINCONFIGSET("steps", "12")
    PRINT "TRAINCONFIG* : "; LLM.TRAINCONFIG()
    ' funzioni di registro/esito (stessi valori dei comandi TRAIN RUNS/EXPORT/STOP/LORA SAVE):
    PRINT "TRAINEXPORT  : "; LLM.TRAINEXPORT("work/train_tour_exp_f")
    PRINT "RUNSTOP      : "; LLM.RUNSTOP("99")
    ' NB: chiamata come STATEMENT (funzione invocata, valore scartato) — evita sia la
    ' stampa parziale su errore sia l'assegnazione del tipo di ritorno.
    ON ERROR GOTO F1
    LLM.LORASAVE("work/no_ckpt", "work/train_tour_ad.safetensors")
    PRINT "LORASAVE     : ok"
    GOTO F2
F1:
    PRINT "LORASAVE     : (ckpt inesistente - errore catturato)"
F2:
    ON ERROR GOTO F3
    LLM.LORALOAD("work/no_ad.safetensors", "work/train_tour_imp")
    PRINT "LORALOAD     : ok"
    GOTO F4
F3:
    PRINT "LORALOAD     : (adapter inesistente - errore catturato)"
F4:
    ON ERROR GOTO 0
    PRINT "EVALGATE     : "; LLM.EVALGATE("work/no_ref", "work/no_cand")

    ' --- registro run ---
    PRINT "RUNSCOUNT    : "; LLM.RUNSCOUNT()
    PRINT "RUNSJSON     : "; LLM.RUNSJSON()
    PRINT "RUNSITEM(1)  : "; LLM.RUNSITEM("1")
    PRINT "LORALIST     : "; LLM.LORALIST()

    ' --- comandi di registro/configurazione (§4) ---
    TRAIN CONFIG SET "steps", "12"
    TRAIN CONFIG SHOW
    TRAIN RUNS LIST
    TRAIN RUNS SHOW "1"
    TRAIN STATUS
    TRAIN STATUS "1"
    TRAIN LORA INFO
    TRAIN EXPORT "work/train_tour_exp"
    TRAIN STOP "1"

    ' --- comandi che richiedono un modello (sintassi; errori catturati per comando) ---
    ON ERROR GOTO C1
    TRAIN DATASET SAMPLE jp, { "tokenizer": tok, "out": "work/train_tour_smp", "model": "work/no_model" }
C1:
    ON ERROR GOTO C2
    TRAIN DISTILL SAMPLE jp, { "tokenizer": tok, "out": "work/train_tour_ds2", "model": "work/no_model" }
C2:
    ON ERROR GOTO C2b
    TRAIN DISTILL TRAIN "work/no_ds.bin", { "model": "work/no_model", "out": "work/no_dt" }
C2b:
    ' --- firme §4 a 3 argomenti posizionali (model, dataset/pairs, out) ---
    ON ERROR GOTO C3
    TRAIN FINETUNE "work/no_model", "work/no_ds", "work/no_out"
C3:
    ON ERROR GOTO C3b
    TRAIN RESUME "work/no_model", 5
C3b:
    ON ERROR GOTO C3c
    TRAIN DPO "work/no_model", "work/no_pairs", "work/no_out"
C3c:
    ON ERROR GOTO C4
    TRAIN RESUME "work/no_model"
C4:
    ON ERROR GOTO C5
    TRAIN LORA APPLY "work/no_ckpt"
C5:
    ON ERROR GOTO C6
    TRAIN LORA MERGE "work/no_base", "work/no_adapter", "work/no_out"
C6:
    ON ERROR GOTO C7
    TRAIN LORA SAVE "work/no_model", "work/no_out"
C7:
    ON ERROR GOTO C8
    TRAIN LORA LOAD "no_model", "work/no_path"
C8:
    ON ERROR GOTO C9b
    TRAIN EVAL "work/no_model", "ciao", "work/q.txt"
C9b:
    ON ERROR GOTO C9c
    ' TRAIN EVAL GATE <ref>, <cand> — gate DEDICATO: stampa il verdetto (ACCEPT/REJECT/ERROR).
    TRAIN EVAL GATE "work/no_ref", "work/no_cand"
    TRAIN EXPORT "1", "work/train_tour_exp_byrun"
    ' TRAIN EVAL VALIDATE <result> AGAINST [VAULT] <folder> — alias di AI EVAL VALIDATE.
    ON ERROR GOTO C9c
    TRAIN EVAL VALIDATE "risultato" AGAINST "vault_folder"
C9c:
    ON ERROR GOTO C10
    TRAIN QUALITY "work/no_model", "ciao", "work/q.txt"
C10:
    ON ERROR GOTO C11
    TRAIN DPO "work/no_model", { "chosen": "work/no_c.bin", "rejected": "work/no_r.bin", "steps": 4 }
C11:
    ON ERROR GOTO 0
    PRINT "comandi modello: sintassi eseguita (modello inesistente)."

    ' --- RUNS PRUNE ---
    TRAIN RUNS PRUNE
    PRINT "RUNSCLEAR    : "; LLM.RUNSCLEAR()
    PRINT "RUNSCOUNT*   : "; LLM.RUNSCOUNT()

    ' cleanup
    ON ERROR GOTO 0
    KILL jp
    KILL ds + ".train.bin"
    KILL ds + ".hold.bin"
    KILL "work/train_tour_dst.train.bin"
    KILL "work/train_tour_dst.hold.bin"

    PRINT "=== TRAIN tour completato ==="
END SUB
