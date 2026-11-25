' MiniTest: FOR EACH su `d.Keys` / `d.Values` di un DICT (con e senza parentesi).
' REGRESSIONE C99Native (2026-11-24): il parser consegna `d.Keys` come CALL col membro nel
' nome e l'emettitore cadeva in "FOR EACH: collezione non supportata (atteso nome di array)"
' (o "'d.Keys' non è un array"), mentre IL/PCode/VM-C iteravano le chiavi. Ora la sorgente
' viene riscritta sull'iterazione DICT (`_keys_`/`_values_`) senza toccare l'AST.
' EXPECT: keys=a b |values= 1   2  |nkeys= 2
SUB Main()
  DIM d AS DICT
  DIM n AS LONG
  d("a") = 1
  d("b") = 2

  PRINT "keys=";
  FOR EACH k IN d.Keys
    PRINT k; " ";
    n = n + 1
  NEXT
  PRINT "|values=";
  FOR EACH v IN d.Values()
    PRINT v; " ";
  NEXT
  PRINT "|nkeys="; n
END SUB
