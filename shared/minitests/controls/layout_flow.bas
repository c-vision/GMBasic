' WrapBox / Grid — disposizione automatica dei figli di un contenitore.
' Tutto passa dallo STYLE: `flow=grid|wrap` sul CONTENITORE (con cols/gap/pad),
' `group=<nome>` sui FIGLI. Nessun comando nuovo, quindi parità fra le 4 gambe.
' Le posizioni sono in CELLE e si confrontano come numeri.
' EXPECT: grid-ok|gridd-ok|wrap-ok
IMPORT WINDOWS
IMPORT CONTROLS

DIM gx AS DOUBLE
DIM gy AS DOUBLE
DIM gw AS DOUBLE
DIM gh AS DOUBLE
DIM dummy AS INTEGER

SUB Rect(nome AS STRING)
    CONTROL GET nome, "left"   INTO gx
    CONTROL GET nome, "top"    INTO gy
    CONTROL GET nome, "width"  INTO gw
    CONTROL GET nome, "height" INTO gh
END SUB

SUB Main()
    WINDOW RESIZE 640, 320

    ' --- GRID: contenitore 56x16 a (4,2), pad=1, gap=1, 3 colonne.
    ' inner 54x14 -> cella 17x6; 4 figli -> (5,3) (23,3) (41,3) (5,10).
    ' Il contenitore è un LISTBOX: un BUTTON in TUI è alto sempre 2 righe.
    LISTBOX 4, 2, 56, 16, " " INTO dummy NAME "gridbox" STYLE "flow=grid cols=3 gap=1 pad=1"
    BUTTON 0,  0,  8,  2, "a" NAME "ga" STYLE "group=gridbox"
    BUTTON 0,  0,  8,  2, "b" NAME "gb" STYLE "group=gridbox"
    BUTTON 0,  0,  8,  2, "c" NAME "gc" STYLE "group=gridbox"
    BUTTON 0,  0,  8,  2, "d" NAME "gd" STYLE "group=gridbox"

    ' --- WRAP: contenitore 56x10 a (4,24), pad=0, gap=1, figli 20x2.
    ' 4 e 25 stanno nella riga, il terzo (46+20=66 > 60) va a capo a (4,27).
    LISTBOX 4, 24, 56, 10, " " INTO dummy NAME "wrapbox" STYLE "flow=wrap gap=1"
    BUTTON 0,  0, 20,  2, "1" NAME "wa" STYLE "group=wrapbox"
    BUTTON 0,  0, 20,  2, "2" NAME "wb" STYLE "group=wrapbox"
    BUTTON 0,  0, 20,  2, "3" NAME "wc" STYLE "group=wrapbox"

    ' Ridispone i gruppi sulla dimensione corrente (idempotente).
    WINDOW RESIZE 640, 320

    CALL Rect("ga")
    IF gx = 5 AND gy = 3 AND gw = 17 AND gh = 6 THEN PRINT "grid-ok" ELSE PRINT "grid-no"

    CALL Rect("gd")
    IF gx = 5 AND gy = 10 AND gw = 17 AND gh = 6 THEN PRINT "gridd-ok" ELSE PRINT "gridd-no"

    CALL Rect("wc")
    IF gx = 4 AND gy = 27 AND gw = 20 AND gh = 2 THEN PRINT "wrap-ok" ELSE PRINT "wrap-no"
END SUB
