' FILE: Mouse/MouseGraphicPaint.bas
' ============================================================================
' MOUSE GRAFICO — Disegna al click in uno screen grafico (SCREEN 12 VGA).
'
' Dimostra il MOUSE POLIMORFICO: in uno screen grafico le coordinate di
' MOUSE.X()/MOUSE.Y() sono in PIXEL (non celle come in TUI/SDLConsole).
'
' Uso:
'   - Muovi il mouse: un mirino verde segue il puntatore (polling MOUSE.X/Y).
'   - Click sinistro: disegna un cerchio giallo nella posizione (evento
'     ON MOUSE CLICK + MOUSE.X()/MOUSE.Y() in pixel).
'   - Click destro: cancella (sfondo blu).
'   - Tasto ESC: esce.
' ============================================================================

IMPORT MOUSE

SUB Main()
    DIM Running AS BOOLEAN = TRUE
    SCREEN 12                 ' 640x480 VGA (screen grafico → coordinate PIXEL)
    CLS 1                     ' Sfondo blu
    MOUSE ON
    MOUSE SHOW

    ON MOUSE CLICK GOSUB OnClick
    ON MOUSE DOWN GOSUB OnMouseDown
    ON KEY(27) GOSUB OnExit   ' ESC per uscire

    PRINT "Mouse grafico: muovi, clicca (cerchio), tasto destro (cancella), ESC per uscire."

    DIM ticks = 0
    WHILE Running AND ticks < 200
        ticks = ticks + 1
        ' Mirino verde attorno alla posizione corrente (polling pixel).
        DIM mx AS DOUBLE = MOUSE.X()
        DIM my AS DOUBLE = MOUSE.Y()
        IF mx > 1 AND my > 1 AND mx < 638 AND my < 478 THEN
            PSET (mx, my), 10
            PSET (mx + 1, my), 10
            PSET (mx, my + 1), 10
            PSET (mx - 1, my), 10
            PSET (mx, my - 1), 10
        END IF
        SYNC
        SLEEP 5
    WEND

    MOUSE OFF
    SCREEN 0
    PRINT "Test mouse grafico concluso."
    EXIT SUB

    OnClick:
        DIM cx AS DOUBLE = MOUSE.X()
        DIM cy AS DOUBLE = MOUSE.Y()
        ' Disegna un cerchio giallo (colore 14) nella posizione del click (pixel).
        CIRCLE (cx, cy), 15, 14
        PRINT "Cerchio a (" + STR(cx) + ", " + STR(cy) + ")"
        RETURN

    OnMouseDown:
        ' Click destro (MOUSE.BUTTON(3)) → cancella l'area (sfondo blu).
        IF MOUSE.BUTTON(3) <> 0 THEN
            CLS 1
            PRINT "Schermo pulito."
        END IF
        RETURN

    OnExit:
        Running = FALSE
        RETURN

END SUB
