' ControlsMore.bas — modalita' grafica: SLIDER / RADIO / COMBOBOX / DATEPICKER (pixel, tema).
IMPORT WINDOWS
IMPORT CONTROLS
SCREEN 12
DIM v = 40
DIM r = 1
DIM c = 0
DIM d AS STRING
SUB Main()
    WINDOW THEME "Turbo"
    SLIDER 40, 30, 220, 0, 100, 5 INTO v
    RADIO 40, 70, "Uno|Due|Tre" INTO r
    COMBOBOX 40, 160, 220, "Alfa|Beta|Gamma" INTO c
    DATEPICKER 40, 210 INTO d
    GO SUB EventLoop()
END SUB
SUB EventLoop()
    DO WHILE WINDOWCLOSED() = 0
        WINDOW PUMP 16
    LOOP
END SUB
