' WidgetsDemo.bas — widget pixel (checkbox/radio/track/dropdown/progress) col tema.
' Il valore di ogni widget vive nella variabile indicata con INTO.
IMPORT WINDOWS
IMPORT CONTROLS
SCREEN 12
DIM ok = 1
DIM opt = 1
DIM vol = 40
DIM lang = 0
DIM prog = 65

SUB Main()
    WINDOW THEME "Monokai"
    CHECKBOX 40, 30, "Abilita notifiche" INTO ok
    RADIO 40, 70, "Basso|Medio|Alto" INTO opt
    SLIDER 40, 160, 300, 0, 100 INTO vol
    COMBOBOX 40, 210, 300, "Italiano|English|Deutsch" INTO lang
    PROGRESS 40, 260, 300 INTO prog
    GO SUB EventLoop()
END SUB

SUB EventLoop()
    DO WHILE WINDOWCLOSED() = 0
        WINDOW PUMP 16
    LOOP
END SUB
