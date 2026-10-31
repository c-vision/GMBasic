' EXPECT: x=5
' @GET con WHEN .F.: il campo non entra nel READ (nessun fuoco, nessun tasto), quindi
' il READ non ha campi da mostrare e ritorna subito. La variabile resta INVARIATA: prima
' veniva comunque riscritta col testo iniziale vuoto → 0 (e PCode andava in timeout).
IMPORT CONSOLE
SUB Main()
  DIM x AS INTEGER
  x = 5
  @ 1, 1 GET x WHEN 0 = 1
  READ
  PRINT "x=" + LTRIM(STR(x))
END SUB
