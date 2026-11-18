' MiniTest: ASR.RECOGNIZE(model$, wav$) — speech-to-text Qwen3-ASR (engine nativo C, namespace ASR).
' HEAVY: carica un modello ~1.8 GB e richiede decine di secondi (TAG: heavy → escluso di default).
' Oracolo: trascrizione deterministica (greedy) di un WAV noto.
' EXPECT: Hello. This is a test of the Vox Troll speech-to-text system
' TAG: heavy
IMPORT ASR

SUB Main()
  PRINT ASR.RECOGNIZE("%MODELS%/Qwen3-ASR-0.6B", "../../samples/Fixtures/audio/test_speech.wav")
END SUB
