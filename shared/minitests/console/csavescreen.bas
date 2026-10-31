' MiniTest: CSAVESCREEN/CRESTORESCREEN nudi (gruppo Console, parità 4 backend).
' Snapshot nominati come SAVE/RESTORE SCREEN. Su emitter C# il nome era ignorato
' (alternate buffer); su C99Native errore esplicito (2026-09-21: named su entrambi).
' Nessun output: l'oracolo è il marker finale.
' EXPECT: csave=ok
IMPORT GRAPHICS

SUB Main()
  CSAVESCREEN "cprobe"
  CRESTORESCREEN "cprobe"
  PRINT "csave=ok"
END SUB
