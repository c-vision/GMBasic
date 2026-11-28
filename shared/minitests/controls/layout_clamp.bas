' Layout manager — LIMITI e QUOTA del delta come proprietà del controllo.
' MINW/MAXW/MINH/MAXH limitano il rect ricalcolato, GROW è la percentuale del
' delta che il controllo assorbe (100 = tutto). Vivono nello STYLE, quindi non
' servono nuove clausole: `STYLE "grow=50 minw=240"`.
' Parità 4 backend (IL / PCode / VM-C / C99Native). Base 640x320 -> 960x480 =
' delta +320 in x, +160 in y. Si confrontano valori NUMERICI (non STR$) così
' l'oracolo non dipende dalla formattazione.
' EXPECT: base-ok|grow-ok|max-ok|min-ok|listbox-ok|fixed-ok|back-ok
IMPORT WINDOWS
IMPORT CONTROLS

DIM gx AS DOUBLE
DIM gy AS DOUBLE
DIM gw AS DOUBLE
DIM gh AS DOUBLE
DIM lbSel AS INTEGER

SUB Rect(nome AS STRING)
    CONTROL GET nome, "left"   INTO gx
    CONTROL GET nome, "top"    INTO gy
    CONTROL GET nome, "width"  INTO gw
    CONTROL GET nome, "height" INTO gh
END SUB

SUB Main()
    WINDOW RESIZE 640, 320

    ' cresce di METÀ del delta: 200 + 320/2 = 360
    BUTTON   40,  40, 200, 40, "G" NAME "bG" EXPAND H STYLE "grow=50"
    ' cresce di tutto ma con un TETTO: 100 + 320 = 420 -> 250
    BUTTON   40, 100, 100, 40, "X" NAME "bX" EXPAND H STYLE "maxw=250"
    ' non cresce (grow=0) ma non scende sotto il minimo leggibile: 100 -> 300
    BUTTON   40, 160, 100, 40, "M" NAME "bM" EXPAND H STYLE "grow=0 minw=300"
    ' nessuna proprietà di layout: resta al rect dichiarato
    BUTTON   40, 220, 120, 40, "F" NAME "bF"
    ' la quota vale anche in verticale (asse Y: 20 + 160/2 = 100)
    LISTBOX  40, 260, 200, 20, "uno,due" INTO lbSel NAME "lbG" EXPAND BOTH STYLE "grow=50"

    CALL Rect("bG")
    IF gx = 40 AND gy = 40 AND gw = 200 THEN PRINT "base-ok" ELSE PRINT "base-no"

    WINDOW RESIZE 960, 480

    CALL Rect("bG")
    IF gx = 40 AND gy = 40 AND gw = 360 THEN PRINT "grow-ok" ELSE PRINT "grow-no"

    CALL Rect("bX")
    IF gx = 40 AND gw = 250 THEN PRINT "max-ok" ELSE PRINT "max-no"

    CALL Rect("bM")
    IF gx = 40 AND gw = 300 THEN PRINT "min-ok" ELSE PRINT "min-no"

    CALL Rect("lbG")
    IF gx = 40 AND gy = 260 AND gw = 360 AND gh = 100 THEN PRINT "listbox-ok" ELSE PRINT "listbox-no"

    CALL Rect("bF")
    IF gx = 40 AND gy = 220 AND gw = 120 THEN PRINT "fixed-ok" ELSE PRINT "fixed-no"

    WINDOW RESIZE 640, 320

    CALL Rect("bG")
    IF gx = 40 AND gw = 200 THEN PRINT "back-ok" ELSE PRINT "back-no"
END SUB
