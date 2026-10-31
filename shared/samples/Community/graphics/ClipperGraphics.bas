' ClipperGraphics.bas — form Clipper @SAY/@GET/READ in grafica PIXEL, governato da READ.
' Campi pixel (text/spinner/checkbox/listbox/combobox) con lo stile/tema; valori nelle var.
IMPORT WINDOWS
SCREEN 12
DIM nome AS STRING
DIM eta AS DOUBLE
DIM attivo AS DOUBLE
DIM colore AS STRING
DIM scelta AS STRING
SUB Main()
    WINDOW THEME "Turbo"
    nome = "Mario Rossi"
    eta = 30
    attivo = 1
    colore = "Verde"
    scelta = "B"
    @ 2, 2 SAY "Nome:"
    @ 2, 12 GET nome SIZE 22
    @ 4, 2 SAY "Eta:"
    @ 4, 12 GET eta TYPE "spinner" MIN 0 MAX 120 STEP 1 SIZE 10
    @ 6, 2 SAY "Attivo:"
    @ 6, 12 GET attivo TYPE "checkbox" CAPTION "Abilita"
    @ 8, 2 SAY "Colore:"
    @ 8, 12 GET colore TYPE "colorpicker" SIZE 15
    @ 13, 2 SAY "Scelta:"
    @ 13, 12 GET scelta TYPE "combobox" ITEMS {"A","B","C"} SIZE 15
    READ
    GO SUB EventLoop()
END SUB
SUB EventLoop()
    DO WHILE WINDOWCLOSED() = 0
        WINDOW PUMP 16
    LOOP
END SUB
