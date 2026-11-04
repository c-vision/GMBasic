' MiniTest: ASR.RECOGNIZE(model$, wav$, outPath$) — STREAMING (chunk 2s + cache encoder + rollback).
' HEAVY: carica un modello ~1.8 GB e richiede decine di secondi (TAG: heavy → escluso di default).
' Oracolo: stessa trascrizione deterministica del MiniTest base; il 3° argomento e' un file di
' transcript scritto IN LIVE, ma il valore ritornato resta il testo finale completo.
' EXPECT: Hello. This is a test of the Vox Troll speech-to-text system.
' TAG: heavy
IMPORT ASR

SUB Main()
  PRINT ASR.RECOGNIZE("/Volumes/Armor5TB/LocalLLM/ai/models/Qwen3-ASR-0.6B", "../../samples/Fixtures/audio/test_speech.wav", "work/asr_stream.txt")
END SUB
