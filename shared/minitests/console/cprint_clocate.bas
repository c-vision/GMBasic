' MiniTest: CPRINT e CLOCATE nudi (gruppo Console, parità 4 backend).
' Varianti console-forzate (namespace GRAPHICS): scrivono sulla console anche con
' screen grafico attivo. CPRINT era no-op silenzioso su VM/C (consume_all);
' CLOCATE idem (2026-09-21: vm_console_9/10 reali). C99Native: CPRINT riusa PRINT.
' Su stdout redirect nessun escape (guardie isatty/headless).
' EXPECT: hello|after=ok
IMPORT GRAPHICS

SUB Main()
  CLOCATE 1, 1
  CPRINT "hello"
  PRINT "after=ok"
END SUB
