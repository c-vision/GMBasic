' FormButtonDemo.bas — bottone NON-modale con callback GPM (C99Native).
'
' Modello GPM (AGENTS §0ter):
'  - Il loop eventi gira in un GOROUTINE dedicato (GO SUB EventLoop): Main resta
'    libero e il processo vive finche' il loop e' attivo.
'  - WINDOW PUMP 16 = FORM_POLL (non bloccante) + PARK GPM (gmb_yield_ticks):
'    il worker NON si blocca (niente gmb_sleep_ms) e non c'e' busy-spin.
'  - La callback del bottone (OnClick) e' invocata INLINE sul task GPM del loop:
'    mai dal thread SDL/present.
IMPORT WINDOWS
IMPORT CONTROLS
SCREEN 12
DIM clicks = 0

SUB Main()
    BUTTON 100, 80, 220, 44, "Cliccami" ON CLICK OnClick
    GO SUB EventLoop()
END SUB

SUB EventLoop()
    DO WHILE WINDOWCLOSED() = 0
        WINDOW PUMP 16
    LOOP
    PRINT "fine loop, click="; clicks
END SUB

SUB OnClick()
    clicks = clicks + 1
    PRINT "CLICK n="; clicks
    WINDOW CLOSE
END SUB
