' TouchInput.bas — touch e long-press sul motore grafico unico.
' Un dito che tocca e alza e' un click completo; il dito fermo oltre la soglia
' (500 ms) e' un long-press, anch'esso chiuso (nessun controllo resta premuto).
' In esecuzione reale il dito arriva da SDL_FINGER* (tablet/touchscreen); qui il
' testo mostra lo stato dei due contatori alimentati dai controlli.
IMPORT WINDOWS
IMPORT CONTROLS

SCREEN 12, 640, 320, 32
DIM taps AS INTEGER
DIM holds AS INTEGER

SUB OnTap()
    taps = taps + 1
    DRAW TEXT "taps: " + STR$(taps), 60, 210
END SUB

SUB OnHold()
    holds = holds + 1
    DRAW TEXT "holds: " + STR$(holds), 60, 250
END SUB

SUB Main()
    WINDOW THEME "light"
    WINDOW SHOW
    BUTTON 60, 50, 200, 44, "Tap" ON CLICK OnTap STYLE "rounded radius=8"
    BUTTON 60, 110, 200, 44, "Hold" ON CLICK OnHold STYLE "rounded radius=8"
    DO WHILE WINDOWCLOSED() = 0
        WINDOW PUMP 16
    LOOP
END SUB
