' MiniTest: AI SET OPTION / AI.Option — registro opzioni generico (round-trip).
' Comando (scrittura, senza parentesi): AI SET OPTION <nome>, <valore>
' Funzione (lettura, con parentesi):    AI.Option(<nome>)
' EXPECT: set=ok|get=work/modeldir|rt=OK
IMPORT AI WITH ALIAS
SUB Main()
  CHDIR PROJECTFOLDER()
  AI SET OPTION "moe_swap_dir", "work/modeldir"
  PRINT "set=ok"
  PRINT "get="; AI.Option("moe_swap_dir")
  IF AI.Option("moe_swap_dir") = "work/modeldir" THEN
    PRINT "rt=OK"
  ELSE
    PRINT "rt=FAIL"
  END IF
END SUB
