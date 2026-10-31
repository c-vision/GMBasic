' EXPECT: ciao-modulo
' MiniTest: il codice di livello modulo (DIM + assegnazione) è eseguito ed è visibile in SUB Main.
DIM MSG AS STRING
MSG = "ciao-modulo"
SUB Main
  PRINT MSG
END SUB
