' MenuPrinterDemo.bas — modalita' grafica: MENU bar + PRINTERDIALOG.
IMPORT WINDOWS
IMPORT CONTROLS
SCREEN 12
DIM m = 0
DIM pr AS STRING
DIM cp = 1
DIM dx = 0
SUB Main()
    WINDOW THEME "Turbo"
    MENU 40, 30, "File|Modifica|Visualizza|Aiuto" INTO m
    PRINTERDIALOG INTO pr, cp, dx
    ALERT("Stampante: " + pr)
    GO SUB EventLoop()
END SUB
SUB EventLoop()
    DO WHILE WINDOWCLOSED() = 0
        WINDOW PUMP 16
    LOOP
END SUB
