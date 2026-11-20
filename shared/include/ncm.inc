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

' Parametri comuni: i percorsi che iniziano con "~/" sono espansi nella cartella utente; i relativi lo sono alla cartella del programma.

' Carica generatore, tokenizer e vocabolario. device: 0 = CPU, 1 = CUDA, 3 = METAL (GPU Apple); se il dispositivo non c'e' ritorna 0.
' Ritorna un handle (> 0) oppure 0 (motivo in LASTERROR). Si possono tenere piu' modelli vivi insieme.
DECLARE FUNCTION LOADMODEL LIB "gmb_torch" ALIAS "gmb_ml_ncm_load" (generatorPt AS STRING, tokenizerPt AS STRING, vocabTxt AS STRING, device AS LONG) AS LONG
DECLARE SUB FREEMODEL LIB "gmb_torch" ALIAS "gmb_ml_ncm_free" (handle AS LONG)
' n testi (n > 0), uno per riga, con un newline anche dopo l'ultimo. seed >= 0 rende la generazione riproducibile, seed < 0 la rende casuale.
' Ritorna "" su errore (handle non valido, n <= 0).
DECLARE FUNCTION GENERATE LIB "gmb_torch" ALIAS "gmb_ml_ncm_text" (handle AS LONG, n AS LONG, seed AS LONG) AS STRING
' Ultimo messaggio d'errore dell'host torch ("" se nessuno).
DECLARE FUNCTION LASTERROR LIB "gmb_torch" ALIAS "gmb_ml_last_error" () AS STRING


' ── Modelli CONDIZIONATI dal prompt (checkpoint XSum: riassunto di un documento) ─────────────────────────────────────────
' Il documento e' il prefisso che il generatore vede: i primi P token (512 nel checkpoint XSum) del BPE di GPT-2, padding a destra.
' Servono i file del repo HF hazyresearch/ncm-checkpoints/xsum: generator.pt, tokenizer.pt, config.yaml (parametri che non stanno nei pesi:
' rope, prefisso, cascata, penalita' di generazione) e il BPE di GPT-2 (vocab.json, merges.txt).
'
'   h = NCM.LOADCONDITIONAL("xsum/generator.pt", "xsum/tokenizer.pt", "xsum/config.yaml", "gpt2/vocab.json", "gpt2/merges.txt", 3)
'   PRINT NCM.GENERATEFROM(h, documento, 0, -1)       ' riassunto (un solo testo)
'
' Carica un modello condizionato. Ritorna un handle (> 0) oppure 0 (motivo in LASTERROR). FREEMODEL lo libera come gli altri.
DECLARE FUNCTION LOADCONDITIONAL LIB "gmb_torch" ALIAS "gmb_ml_ncm_load_cond" (generatorPt AS STRING, tokenizerPt AS STRING, configYaml AS STRING, vocabJson AS STRING, mergesTxt AS STRING, device AS LONG) AS LONG
' Testo generato dal prompt, tagliato al primo fine-testo. Il campionamento del config XSum e' greedy (top_k = 1): il seed conta solo se il
' config campiona. cfgScale = guidance senza classificatore (0 = spenta, costo doppio se > 0); un valore negativo usa quello del config.
' Ritorna "" su errore (handle non valido, modello non condizionato: vedi LASTERROR).
DECLARE FUNCTION GENERATEFROM LIB "gmb_torch" ALIAS "gmb_ml_ncm_complete" (handle AS LONG, prompt AS STRING, seed AS LONG, cfgScale AS DOUBLE) AS STRING

' ── Metrica ROUGE (stessa tokenizzazione di rouge_score con stemmer: minuscolo, solo [a-z0-9], Porter oltre 3 lettere) ───────
' F-measure in [0, 1] fra un testo di riferimento e uno generato. Non richiede nessun modello.
DECLARE FUNCTION ROUGE_N LIB "gmb_torch" ALIAS "gmb_ml_rouge" (reference AS STRING, hypothesis AS STRING, kind AS LONG) AS DOUBLE
FUNCTION ROUGE1(reference AS STRING, hypothesis AS STRING) AS DOUBLE
    RETURN ROUGE_N(reference, hypothesis, 1)
END FUNCTION
FUNCTION ROUGE2(reference AS STRING, hypothesis AS STRING) AS DOUBLE
    RETURN ROUGE_N(reference, hypothesis, 2)
END FUNCTION
FUNCTION ROUGEL(reference AS STRING, hypothesis AS STRING) AS DOUBLE
    RETURN ROUGE_N(reference, hypothesis, 3)
END FUNCTION
