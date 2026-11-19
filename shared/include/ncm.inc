' ncm.inc — namespace NCM: generazione di testo con Next-Concept Modeling (arXiv 2610.14162), FFI su libgmb_torch.
'
' NCM genera il testo dal grossolano al fine: 12 livelli di codici discreti (da 1 a 128 posizioni), ogni livello in UN
' solo forward del transformer, poi un decoder VQ-VAE riporta i codici ai token. I pesi sono quelli pubblicati da
' HazyResearch (https://huggingface.co/hazyresearch/ncm-checkpoints, licenza MIT): per LM1B small servono
'   lm1b/small/generator.pt    (390 MB)   e    lm1b/tokenizer.pt   (598 MB)
' piu' il vocabolario di bert-base-uncased (vocab.txt). Sono checkpoint `torch.save`: li legge il lettore C++ di
' libgmb_torch (nessun Python). Modello non condizionato, frasi da 128 token tipo notizie (corpus LM1B).
'
'   IMPORT NCM
'   DIM h AS LONG
'   h = NCM.LOADMODEL("lm1b/small/generator.pt", "lm1b/tokenizer.pt", "vocab.txt", 3)   ' device: 0 CPU, 3 METAL (MPS)
'   IF h = 0 THEN PRINT NCM.LASTERROR() : END
'   PRINT NCM.GENERATE(h, 4, 42)        ' 4 testi (uno per riga), seed 42 (seed < 0: casuale)
'   NCM.FREEMODEL(h)
'
' Il campione inizia con [CLS] nel modello LM1B: il testo restituito lo omette (come il decoder di riferimento).
' Su Apple M5 Max (MPS) circa 28 ms per campione con batch da 64 o piu'.

NAMESPACE NCM

' Carica generatore, tokenizer e vocabolario. device: 0 = CPU, 1 = CUDA, 3 = METAL. Ritorna un handle (> 0) o 0.
DECLARE FUNCTION LOADMODEL LIB "gmb_torch" ALIAS "gmb_ml_ncm_load" (generatorPt AS STRING, tokenizerPt AS STRING, vocabTxt AS STRING, device AS LONG) AS LONG
DECLARE SUB FREEMODEL LIB "gmb_torch" ALIAS "gmb_ml_ncm_free" (handle AS LONG)
' n testi separati da newline. seed >= 0 rende la generazione riproducibile.
DECLARE FUNCTION GENERATE LIB "gmb_torch" ALIAS "gmb_ml_ncm_text" (handle AS LONG, n AS LONG, seed AS LONG) AS STRING
' Ultimo messaggio d'errore dell'host torch ("" se nessuno).
DECLARE FUNCTION LASTERROR LIB "gmb_torch" ALIAS "gmb_ml_last_error" () AS STRING

