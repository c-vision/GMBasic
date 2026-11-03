' MiniTest NAMESPACE ASR — unica funzione: RECOGNIZE(model$, wav$[, out$]).
' Parità di SUPERFICIE (compilazione/link della firma a 2 e 3 argomenti) sui 4 backend:
' il percorso reale carica Qwen3-ASR-0.6B (1.8 GB) e non è eseguibile in un test
' automatico qui (modello su disco esterno, motore ASR non caricabile dal runner).
' I MiniTest heavy llm/asr_transcribe.bas e llm/asr_stream.bas coprono il percorso reale.
' EXPECT: superficie=ok
IMPORT ASR

SUB Main()
  PRINT "superficie=ok"
  IF 0 THEN
    _x = ASR.RECOGNIZE("", "")
    _y = ASR.RECOGNIZE("", "", "")
  END IF
END SUB
