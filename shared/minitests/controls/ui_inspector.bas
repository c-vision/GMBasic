' Inspector della UI: `ui.inspector` è una voce del REGISTRO OPZIONI, come i flag
' di accessibilità. Acceso, il frame disegna un pannello in basso a destra con
' l'albero dei controlli (nome, tipo, rect, flag, colonne del flow) e il costo
' dell'ultimo frame. Non è un controllo: non entra nei registri, non prende il
' fuoco, non genera eventi — quindi non cambia il rect dei controlli del programma.
' EXPECT: inspector on = 1|inspector off = 0|rect-ok|frame-ok
IMPORT AI WITH ALIAS
IMPORT WINDOWS
IMPORT CONTROLS

DIM gx AS DOUBLE
DIM gy AS DOUBLE
DIM gw AS DOUBLE
DIM gh AS DOUBLE

SUB Main()
    AI SET OPTION "ui.inspector", "1"
    PRINT "inspector on = "; AI.OPTION("ui.inspector")

    WINDOW RESIZE 640, 320
    BUTTON 10, 10, 20, 2, "a" NAME "iA"

    ' il pannello è un overlay: non deve spostare i controlli del programma
    CONTROL GET "iA", "left" INTO gx
    CONTROL GET "iA", "top"  INTO gy
    CONTROL GET "iA", "width" INTO gw
    CONTROL GET "iA", "height" INTO gh

    AI SET OPTION "ui.inspector", "0"
    PRINT "inspector off = "; AI.OPTION("ui.inspector")

    IF gx = 10 AND gy = 10 AND gw = 20 AND gh = 2 THEN PRINT "rect-ok" ELSE PRINT "rect-no"

    ' il costo del frame si aggiorna: un giro di DOEVENTS non deve rompere nulla
    AI SET OPTION "ui.inspector", "1"
    DOEVENTS
    PRINT "frame-ok"
END SUB
