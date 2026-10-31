' MiniTest: ON WINDOW <EVENTO> OF <handle> (evento finestra per handle) — Fase 5.
' EXPECT: win-ok
IMPORT WINDOWS
SCREEN 0
SUB OnClose()
END SUB
SUB Main()
    ON WINDOW CLOSE OF 1 GOSUB OnClose
    ON WINDOW MINIMIZE OF 1 GOSUB OnClose
    PRINT "win-ok"
END SUB
