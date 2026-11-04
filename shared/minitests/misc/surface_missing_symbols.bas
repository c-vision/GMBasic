' MiniTest compile/arity per i simboli di superficie senza test dedicato.
' Chiamate in IF 0: verifica risoluzione firme senza effetti runtime.
' EXPECT: surface-symbols-ok
IMPORT CONTROLS
IMPORT LLM
IMPORT WINDOWS
IMPORT CONVERT

SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  IF 0 THEN
    DIM d AS DOUBLE
    DIM s AS STRING
    d = CTRLGETPROP("x", "prop")
    s = CTRLGETPROPS("x", "prop")
    d = LLM.LORASAVE(GMBWORK$ + "/a", GMBWORK$ + "/b")
    d = LLM.LORALOAD(GMBWORK$ + "/a", GMBWORK$ + "/b")
    d = WINDOWCLOSED()
    WEB2MD "http://localhost/" INTO s
  END IF
  PRINT "surface-symbols-ok"
END SUB
