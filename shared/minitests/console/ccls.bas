' MiniTest: CCLS nudo (gruppo Console, parità 4 backend).
' CCLS [colore]: clear + background opzionale. Era no-op silenzioso su VM/C
' (consume_all) e errore esplicito su C99Native (2026-09-21: vm_console_11 già
' esistente — verificato wiring; st_ccls reale su C99Native).
' Su stdout redirect nessun escape (guardie isatty/headless): l'oracolo è il marker.
' EXPECT: ccls=ok
IMPORT GRAPHICS

SUB Main()
  CCLS
  CCLS 4
  PRINT "ccls=ok"
END SUB
