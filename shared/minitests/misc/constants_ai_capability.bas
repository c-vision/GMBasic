' MiniTest: costanti AI capability (%CHAT/%OCR/%AUDIO/%EMBEDDING/%RERANKER/%TTS)
'   e modalita' reranker (%RERANK_OFF/%RERANK_KEYWORD/%RERANK_ONNX).
'   Verifica i valori INTEGER delle costanti.
'   VALUES: CHAT=0 OCR=1 AUDIO=2 EMBEDDING=3 RERANKER=4 TTS=5
'           RERANK_OFF=0 RERANK_KEYWORD=1 RERANK_ONNX=2
' EXPECT: chat= 0|ocr= 1|audio= 2|embed= 3|reranker= 4|tts= 5|roff= 0|rkw= 1|ronx= 2
SUB Main
  PRINT "chat="; %CHAT
  PRINT "ocr="; %OCR
  PRINT "audio="; %AUDIO
  PRINT "embed="; %EMBEDDING
  PRINT "reranker="; %RERANKER
  PRINT "tts="; %TTS
  PRINT "roff="; %RERANK_OFF
  PRINT "rkw="; %RERANK_KEYWORD
  PRINT "ronx="; %RERANK_ONNX
END SUB
