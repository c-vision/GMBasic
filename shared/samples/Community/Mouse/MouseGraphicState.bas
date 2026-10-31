' FILE: Mouse/MouseGraphicState.bas
' ============================================================================
' MOUSE GRAFICO — Stato del mouse in uno screen grafico (SCREEN 13 SVGA).
'
' Dimostra le funzioni di STATO del mouse polimorfico in uno screen grafico:
'   - MOUSE.X()/MOUSE.Y()      → posizione in PIXEL
'   - MOUSE.PRESENT()          → mouse presente
'   - MOUSE.LEFT()/RIGHT()     → tasti premuti (stato continuo)
'   - MOUSE.MOVE()             → il mouse si sta muovendo
'   - MOUSE.WHEEL()            → delta della rotella
'
' Uso: muovi il mouse e premi i tasti; i valori vengono stampati a schermo.
' Tasto ESC: esce.
' ============================================================================

IMPORT MOUSE

SUB Main()
    DIM Running AS BOOLEAN = TRUE
    SCREEN 13                 ' 320x200 SVGA (256 colori, coordinate PIXEL)
    CLS 0
    MOUSE ON
    MOUSE SHOW

    ON KEY(27) GOSUB OnExit
    ON MOUSE MOVE GOSUB OnMove

    DIM lastX = -1, lastY = -1

    PRINT "MOUSE.X()/Y() in pixel. Premi i tasti e muovi il mouse. ESC per uscire."

    DIM ticks = 0
    WHILE Running AND ticks < 200
        ticks = ticks + 1
        DIM px AS DOUBLE = MOUSE.X()
        DIM py AS DOUBLE = MOUSE.Y()
        DIM present AS DOUBLE = MOUSE.PRESENT()
        DIM left AS DOUBLE = MOUSE.LEFT()
        DIM right AS DOUBLE = MOUSE.RIGHT()
        DIM middle AS DOUBLE = MOUSE.MIDDLE()
        DIM moving AS DOUBLE = MOUSE.MOVE()
        DIM wheel AS DOUBLE = MOUSE.WHEEL()

        ' Stampa lo stato solo quando cambia.
        IF px <> lastX OR py <> lastY THEN
            CLS 0
            LOCATE 2, 2
            PRINT "X=" + STR(px) + " Y=" + STR(py)
            LOCATE 3, 2
            PRINT "Present=" + STR(present) + " Move=" + STR(moving) + " Wheel=" + STR(wheel)
            LOCATE 4, 2
            PRINT "Left=" + STR(left) + " Right=" + STR(right) + " Middle=" + STR(middle)
            ' Punto bianco dove sta il mouse.
            IF px >= 0 AND py >= 0 THEN PSET (px, py), 15
            lastX = px
            lastY = py
        END IF
        SYNC
        SLEEP 10
    WEND

    MOUSE OFF
    SCREEN 0
    PRINT "Test stato mouse grafico concluso."
    EXIT SUB

    OnMove:
        ' Evento ON MOUSE MOVE: aggiorna il punto bianco (polling già lo fa).
        RETURN

    OnExit:
        Running = FALSE
        RETURN

END SUB
