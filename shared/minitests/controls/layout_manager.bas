' Layout manager dei controlli — EXPAND / ANCHOR / ALIGN come PROPRIETÀ del controllo.
' Parità 4 backend (IL / PCode / VM-C / C99Native). Headless deterministico: WINDOW RESIZE
' porta la pagina alla dimensione voluta e fa scattare il reflow (idempotente).
' Base 640x320 -> 960x480 = delta +320 in x, +160 in y.
' Si confrontano i valori NUMERICI (non STR$) così l'oracolo non dipende dalla formattazione;
' l'altezza dei BUTTON nel percorso TEXT è propria del widget (2 celle), quindi per l'asse
' verticale si verifica il LISTBOX.
' EXPECT: base-ok|grow-ok|corner-ok|center-ok|listbox-ok|back-ok
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

    BUTTON   40,  40, 200, 40, "E" NAME "bE" EXPAND BOTH
    BUTTON   40, 100, 200, 40, "H" NAME "bH" ANCHOR LEFT+RIGHT
    BUTTON  400, 220, 180, 40, "C" NAME "bC" ANCHOR RIGHT+BOTTOM
    BUTTON  250, 160, 120, 40, "M" NAME "bM" ALIGN CENTER+MIDDLE
    BUTTON   40, 220, 120, 40, "F" NAME "bF"
    LISTBOX  40, 260, 200, 20, "uno,due" INTO lbSel NAME "lbE" EXPAND BOTH

    CALL Rect("bE")
    IF gx = 40 AND gy = 40 AND gw = 200 THEN PRINT "base-ok" ELSE PRINT "base-no"

    WINDOW RESIZE 960, 480

    CALL Rect("bE")
    IF gx = 40 AND gy = 40 AND gw = 520 THEN PRINT "grow-ok" ELSE PRINT "grow-no"

    CALL Rect("bC")
    IF gx = 720 AND gy = 380 AND gw = 180 THEN PRINT "corner-ok" ELSE PRINT "corner-no"

    CALL Rect("bM")
    IF gx = 410 AND gy = 240 AND gw = 120 THEN PRINT "center-ok" ELSE PRINT "center-no"

    CALL Rect("lbE")
    IF gx = 40 AND gy = 260 AND gw = 520 AND gh = 180 THEN PRINT "listbox-ok" ELSE PRINT "listbox-no"

    WINDOW RESIZE 640, 320

    CALL Rect("bE")
    IF gx = 40 AND gy = 40 AND gw = 200 THEN PRINT "back-ok" ELSE PRINT "back-no"
END SUB
