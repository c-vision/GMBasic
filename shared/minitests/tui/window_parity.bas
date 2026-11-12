' Minitest WINDOW — parità 4 backend (statement TUI: DEFINE/ACTIVATE/CLEAR/PRINT/EDIT).
' TAG: exitcode — l'output contiene i FRAME TUI (solo VMC li renderizza): la parita' si verifica sull'exit code.
IMPORT WINDOWS
SUB Main()
  WINDOW DEFINE C FROM 1, 1 TO 10, 40 TITLE "win-parity"
  WINDOW C ACTIVATE
  WINDOW C CLEAR
  WINDOW C PRINT "hello-window"
  WINDOW C EDIT ON
  WINDOW C EDIT OFF
  CPRINT "WINDOW-OK"
END SUB
