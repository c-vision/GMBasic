' KvFormatContract.bas — §9.A M10c: contratto GMB_KV_FORMAT (dtype di STORAGE del KV).
'
' Il KV ha un dtype NATIVO (quello con cui il modello è addestrato). È ammesso memorizzarlo in
' dtype diversi SOLO se supportati/quantizzabili; la famiglia fp4 (fp4/mxfp4/nvfp4) è ammessa
' SOLO se è il dtype nativo del modello — MAI una conversione runtime silenziosa.
' `LLM TENSOR KVFORMAT <model>[, "<dtype>"]` mostra il contratto e VALIDA una richiesta.
IMPORT LLM

SUB Main()
  DIM model AS STRING = "work/modello.gguf"   ' modello GGUF (fornito dall'utente)

  ' 1) contratto KV del modello (dtype nativo + storage ammessi)
  LLM TENSOR KVFORMAT model

  ' 2) valida una richiesta di storage (es. bf16 ammesso; fp4 rifiutato se non nativo)
  LLM TENSOR KVFORMAT model, "bf16"
  LLM TENSOR KVFORMAT model, "fp4"
  LLM TENSOR KVFORMAT model, "q8_0"
  PRINT "done"
END SUB
