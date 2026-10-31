' MiniTest — ON ERROR GOTO con etichetta dentro una FUNCTION (parità 4 backend).
'
' Bug IL: il corpo di una procedura con ON ERROR è decomposto in un dispatch a chunk
' (EmitProcedureBodyWithErrorHandling) e `RETURN <valore>` veniva rifiutato in compilazione
' ("RETURN con un valore non supportato a livello top-level"), mentre `END FUNCTION` dentro
' il dispatch restituiva il DEFAULT del tipo (stringa vuota) invece del return-slot: una
' FUNCTION di lettura file (`ReadText`) tornava sempre "".
' Ora il RETURN col valore salva in un local tipizzato + `leave` fuori dai try, e il punto
' di uscita comune (fuori da ogni regione protetta) legge il return-slot per END FUNCTION.
' EXPECT: ok= 99|end=ok:5|caught|-1
IMPORT LLM

FUNCTION NoErr() AS LONG
  ON ERROR GOTO OnBad
  NoErr = 1
  RETURN 99
OnBad:
  RETURN -1
END FUNCTION

FUNCTION EndOnly(n AS LONG) AS STRING
  ON ERROR GOTO OnOops
  EndOnly = "ok:" + LTRIM$(STR$(n))
OnOops:
END FUNCTION

FUNCTION Failer() AS LONG
  ON ERROR GOTO OnFail
  LLM MODEL CONVERT "/non/esiste/checkpoint", "/tmp/gmbasic-onerror-fn.gguf"
  RETURN 99
OnFail:
  PRINT "caught"
  RETURN -1
END FUNCTION

SUB Main()
  PRINT "ok="; NoErr()
  PRINT "end="; EndOnly(5)
  PRINT "fail="; Failer()
END SUB
