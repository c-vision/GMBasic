' PARITY-EXCLUDE: catena pesante su modello LOCALE MLX; solo manuale (richiede i pesi su disco).
' FILE: AiLLM/ModelCompilerMlxToDs4.bas
' =====================================================================================
' CATENA MODEL COMPILER END-TO-END (MLX -> GGUF -> QUANT -> VERIFY).
'   0. INSPECT       : caratteristiche del modello (read-only, SENZA convertire ne' caricare)
'   1. MODEL CONVERT : checkpoint MLX/HF (safetensors) -> GGUF  (converter codec + .gmbarch)
'   2. PLAN          : classifica i tensori per ruolo (dal .gmbarch), scrive il piano
'   3. POLICY SET    : per questa PROVA gli esperti routed vanno in MXFP4 (blocco 32)
'   4. QUANT         : dequant sorgente + requant (pool parallelo) -> GGUF compresso
'   5. VERIFY        : PLAN del modello compresso (tipi per-ruolo)
'
' NOTA (perche' MXFP4): IQ2_XXS (come tutti i formati _K) quantizza a blocchi di 256 pesi -> serve
' un asse contiguo multiplo di 256. In DeepSeek-V2-Lite moe_intermediate_size=1408 (5x256+128) NON
' e' multiplo di 256 -> IQ2_XXS e' inapplicabile sui down_proj degli esperti. MXFP4 usa blocchi da
' 32 (1408 = 44x32) -> applicabile. `LLM MODEL INSPECT` mostra `inter` PRIMA di quantizzare.
' =====================================================================================
IMPORT LLM

SUB Main()
    MKDIR "work"
    PRINT "=== Model Compiler LLM: MLX -> GGUF -> QUANT (esperti MXFP4) ==="
    IF NOT IO.FILEEXISTS("~/ai/Models/DeepSeek-V2-Lite-Chat-4bit-mlx/config.json") THEN
        PRINT "sorgente assente: serve DeepSeek-V2-Lite-Chat-4bit-mlx in ~/ai/Models"
        END
    END IF
    PRINT "0) INSPECT (caratteristiche del modello, read-only) ..."
    LLM MODEL INSPECT "~/ai/Models/DeepSeek-V2-Lite-Chat-4bit-mlx"
    PRINT "1) CONVERT MLX/HF -> GGUF ..."
    LLM MODEL CONVERT "~/ai/Models/DeepSeek-V2-Lite-Chat-4bit-mlx", "work/dsv2l.gguf", "GGUF"
    PRINT "2) PLAN ..."
    LLM PLAN "work/dsv2l.gguf", "work/dsv2l_piano.txt"
    PRINT "3) POLICY (prova: esperti routed -> MXFP4) ..."
    LLM POLICY SET "ROUTED_EXPERT", "mxfp4"
    LLM POLICY SET "ATTN_KV", "q8_0"
    LLM POLICY SET "OUTPUT", "q8_0"
    PRINT "4) QUANT (dequant + requant, parallelo) ..."
    LLM QUANT "work/dsv2l.gguf", "work/dsv2l_mxfp4.gguf"
    PRINT "5) VERIFY (PLAN del compresso) ..."
    LLM PLAN "work/dsv2l_mxfp4.gguf", "work/dsv2l_verify.txt"
    PRINT "OK: work/dsv2l_mxfp4.gguf"
END SUB
