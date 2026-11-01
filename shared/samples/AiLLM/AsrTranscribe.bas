' AsrTranscribe.bas — speech-to-text Qwen3-ASR (namespace ASR, engine nativo C).
' ASR.RECOGNIZE(model$, wav$[, outPath$]): trascrive un WAV → testo.
'   - 2 argomenti: trascrizione COMPLETA (default).
'   - 3° argomento (file): STREAMING (chunk 2s + cache finestre encoder 8s + prefix rollback):
'     i pezzi vengono scritti IN LIVE sul file mentre il decoder avanza; ritorna comunque il testo.
' Il modello vive su disco esterno (1.8 GB, Qwen/Qwen3-ASR-0.6B); il WAV è una fixture.
' Output live in work/asr_stream.txt (accanto al sample).
IMPORT ASR

SUB Main()
  PRINT "=== ASR.RECOGNIZE streaming — transcript live su work/asr_stream.txt ==="
  PRINT ASR.RECOGNIZE("/Volumes/Armor5TB/LocalLLM/ai/models/Qwen3-ASR-0.6B", "../Fixtures/audio/test_speech.wav", "work/asr_stream.txt")
END SUB
