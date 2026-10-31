' MiniTest: membro di un oggetto JSON — confronto NUMERICO e STRINGA (parità IL/PCode/VM-C).
' Sul C99Native `sec.Timeout = 1234` non compilava (il member read su val emetteva una
' STRINGA in contesto numerico); sull'emitter C# il confronto numerico non era coperto.
' EXPECT: num=ok|str=ok
IMPORT JSON

SUB Main()
  DIM q AS STRING
  q = CHR(34)
  DIM sec AS JSON = JSON.PARSE("{" + q + "Timeout" + q + ": 1234, " + q + "Host" + q + ": " + q + "h1" + q + "}")
  DIM okc AS INTEGER
  okc = (sec.Timeout = 1234)
  PRINT "num="; IIF(okc, "ok", "FAIL")
  okc = (sec.Host = "h1")
  PRINT "str="; IIF(okc, "ok", "FAIL")
END SUB
