' ============================================================================
' ModelCompilerQuantAuto.bas — quantizzazione ad AUTO-ALLOCAZIONE (budget bpw).
'
' Il model compiler (namespace LLM) sa quantizzare in due modi:
'   - per-RUOLO / per-POLICY:  LLM QUANT <in>, <out>              (formati decisi da policy/ricetta)
'   - AUTO a budget bpw:       LLM QUANT <in>, <out>, <bpw>      (bit per-TENSORE da sensibilità)
'
' Il terzo argomento (target bpw) attiva l'allocatore greedy: misura l'errore di ricostruzione
' per ogni tensore (eventualmente pesato dall'imatrix) e sceglie Q2_K..Q8_0 sotto il budget.
' Stessa sorgente C del per-ruolo: `llmmodelio_model_quantize_auto` (PUL / gmbllm).
'
' Fixture: samples/Fixtures/models/tiny_big_moe.gguf (versione, MoE fittizio con tensori >=256).
' Output in work/ accanto a questo sample. La CWD deve essere QUESTA cartella (AGENTS §9/§11).
' ============================================================================
IMPORT LLM
IMPORT IO

SUB Main()
    PRINT "=== Model Compiler LLM — QUANT AUTO (budget bpw) ==="
    MKDIR "work"

    IF NOT IO.FILEEXISTS("../../Fixtures/models/tiny_big_moe.gguf") THEN
        PRINT "Fixture assente: samples/Fixtures/models/tiny_big_moe.gguf"
        END
    END IF

    ' ---- 1) AUTO a 3.5 bpw (più piccolo) ----
    LLM QUANT "../../Fixtures/models/tiny_big_moe.gguf", "work/auto_b35.gguf", 3.5
    PRINT "1) QUANT AUTO 3.5 bpw -> work/auto_b35.gguf"

    ' ---- 2) AUTO a 4.5 bpw (più fedele) ----
    LLM QUANT "../../Fixtures/models/tiny_big_moe.gguf", "work/auto_b45.gguf", 4.5
    PRINT "2) QUANT AUTO 4.5 bpw -> work/auto_b45.gguf"

    ' ---- 3) per-RUOLO (senza bpw) per confronto ----
    LLM QUANT "../../Fixtures/models/tiny_big_moe.gguf", "work/role.gguf"
    PRINT "3) QUANT per-ruolo     -> work/role.gguf"

    ' ---- 4) verifica: i formati per-tensore del risultato AUTO ----
    LLM TENSOR IR "work/auto_b45.gguf"

    PRINT "quant-auto: OK"
END SUB
