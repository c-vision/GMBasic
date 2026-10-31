' ============================================================================
' QualityGguf.bas — perplexity/NLL di un modello GGUF, col motore di GMBasic.
'
' `TRAIN EVAL <model.gguf>, <dataset>` (alias `LLM QUALITY`) accetta ora anche un **file .gguf**:
' il tokenizer viene ricostruito dai token/merges EMBEDDED nel GGUF e il modello è caricato
' dall'engine MLX/Metal (`llm_ai_mlx_load_model`, ARCH_TABLE). Così si misura la PPL del GGUF
' quantizzato da GMBasic senza strumenti esterni.
'
' TAG: heavy   (richiede un modello GGUF locale; non entra nella suite automatica)
' NB: negli argomenti dei comandi TRAIN/LLM i termini nudi sono stringhe LETTERALI: i PATH
'     vanno scritti tra virgolette (le variabili NON sono supportate lì).
' Output in work/ accanto a questo sample (CWD = questa cartella, AGENTS §9/§11).
' ============================================================================
IMPORT LLM
IMPORT IO

SUB Main()
    IF NOT IO.FILEEXISTS("/Users/gaetano/ai/Models/gguf/olmoe-1b-7b-0924-instruct-q4_k_m.gguf") THEN
        PRINT "Modello assente: ~/ai/Models/gguf/olmoe-1b-7b-0924-instruct-q4_k_m.gguf"
        END
    END IF
    MKDIR "work"

    ' dataset multi-prompt: una riga = un prompt; si riporta la NLL/perplexity MEDIA
    DIM h AS LONG = IO.FCREATE("work/eval_ds.txt")
    IO.FWRITE(h, "La capitale dell'Italia e Roma." + CHR(10))
    IO.FWRITE(h, "Il cielo e azzurro e il mare e blu." + CHR(10))
    IO.FWRITE(h, "Oggi il tempo e bello e caldo." + CHR(10))
    IO.FCLOSE(h)

    PRINT "=== TRAIN EVAL su GGUF (motore Metal) ==="
    TRAIN EVAL "/Users/gaetano/ai/Models/gguf/olmoe-1b-7b-0924-instruct-q4_k_m.gguf", "work/eval_ds.txt"
    PRINT "quality-gguf: OK"
END SUB
