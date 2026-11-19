' PARITY-EXCLUDE: heavy — carica ~1 GB di checkpoint NCM (HazyResearch/ncm, MIT); solo manuale.
' FILE: AiLLM/NcmGenerate.bas
' ==============================================================================
'   NCM (Next-Concept Modeling, arXiv 2610.14162) — generazione di testo dal grossolano al fine
' ==============================================================================
' NCM non genera un token alla volta: produce 12 livelli di codici discreti (da 1 a 128 posizioni),
' ogni livello in UN solo forward del transformer, e un decoder VQ-VAE riporta i codici ai token.
' Questo sample usa il modello LM1B small (97M parametri, frasi da 128 token tipo notizie, non condizionato).
'
' PREREQUISITI
'   1) checkpoint, da https://huggingface.co/hazyresearch/ncm-checkpoints :
'        lm1b/small/generator.pt (390 MB) e lm1b/tokenizer.pt (598 MB)
'   2) il vocabolario di bert-base-uncased (vocab.txt)
'   3) libgmb_torch (host libtorch) aggiornato con le funzioni gmb_ml_ncm_*
' Cambia i tre percorsi qui sotto.
IMPORT NCM

CONST GENERATOR = "~/ai/ncm/lm1b/small/generator.pt"
CONST TOKENIZER = "~/ai/ncm/lm1b/tokenizer.pt"
CONST VOCAB     = "~/ai/hf-cache/hub/models--bert-base-uncased/snapshots/86b5e0934494bd15c9632b12f734a8a67f723594/vocab.txt"

SUB Main()
    DIM h AS LONG
    ' device: 0 = CPU, 3 = METAL (Apple GPU)
    h = NCM.LOADMODEL(GENERATOR, TOKENIZER, VOCAB, 3)
    IF h = 0 THEN
        PRINT "NCM.LOADMODEL fallita: "; NCM.LASTERROR()
        RETURN
    END IF
    DIM t0 AS DOUBLE
    t0 = TIMER
    PRINT NCM.GENERATE(h, 8, 42)
    PRINT "8 campioni in "; FORMAT(TIMER - t0, "0.00"); " s"
    NCM.FREEMODEL(h)
END SUB
