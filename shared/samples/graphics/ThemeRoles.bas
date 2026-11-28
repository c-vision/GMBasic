' ThemeRoles.bas — RUOLI SEMANTICI del tema + LIMITI del layout.
'
' Il programma dichiara COSA è un controllo, non che colore ha: il tema decide i
' colori, con la cascata di un design system
'
'     override espliciti (fg=/bg=/focuscolor=/selcolor=)
'        >  ruolo + stato  >  tema
'
' Ruoli: default, primary, secondary, accent, success, warning, danger, info, muted
' (alias: main, sub, ok, warn, error/destructive, note, disabled/dim).
' Nello STYLE:  STYLE "role=danger"        STYLE "role=primary contrast=7"
' `contrast=` è il rapporto minimo di contrasto richiesto per l'inchiostro del
' ruolo (default WCAG AA 4.5; `contrast=off` lo disattiva). Il colore NON viene
' congelato: è risolto dal tema corrente a ogni disegno, quindi un cambio tema
' (`WINDOW THEME`) non lascia colori stantii.
'
' Limiti del layout (stesse proprietà di EXPAND/ANCHOR/ALIGN, sempre nello STYLE):
'   MINW= MINH= MAXW= MAXH=   limiti in unità del controllo (0 = nessun limite)
'   GROW=NN                    percentuale del delta assorbita (100 = tutta)
' Due pannelli affiancati che crescono entrambi "tutto" si sovrappongono: con
' GROW=50 ciascuno prende la metà della crescita.
'
' Esecuzione headless deterministica: WINDOW RESIZE porta la pagina alla
' dimensione voluta e fa scattare il layout (idempotente).
'
' Valori attesi (base 640x320 -> 960x480; delta +320 in x, +160 in y):
'   bSave  EXPAND H grow=50             -> w = 160 + 160 = 320
'   bDel   ANCHOR RIGHT (role=danger)   -> x = 210 + 320 = 530, w invariata
'   bInfo  EXPAND H maxw=300 (info)     -> w = 160 + 320 = 480 -> 300
'   bMuted EXPAND H grow=0 minw=240     -> w = 100 -> 240
IMPORT WINDOWS
IMPORT CONTROLS

DIM gx AS DOUBLE
DIM gy AS DOUBLE
DIM gw AS DOUBLE
DIM gh AS DOUBLE

SUB Dump(nome AS STRING)
    CONTROL GET nome, "left"   INTO gx
    CONTROL GET nome, "top"    INTO gy
    CONTROL GET nome, "width"  INTO gw
    CONTROL GET nome, "height" INTO gh
    PRINT nome; " x="; STR$(gx); " y="; STR$(gy); " w="; STR$(gw); " h="; STR$(gh)
END SUB

SUB Main()
    ' Il tema si può cambiare anche a runtime: i ruoli seguono.
    WINDOW THEME "midnight"

    ' 1) Dimensione BASE della pagina: il layout misura il DELTA da qui.
    WINDOW RESIZE 640, 320

    BUTTON   40,  40, 160, 40, "Salva"   NAME "bSave"  EXPAND H STYLE "role=primary grow=50"
    BUTTON  400,  40, 160, 40, "Elimina" NAME "bDel"   ANCHOR RIGHT STYLE "role=danger"
    BUTTON   40, 100, 160, 40, "Info"    NAME "bInfo"  EXPAND H STYLE "role=info maxw=300"
    BUTTON   40, 160, 100, 40, "Tenue"   NAME "bMuted" EXPAND H STYLE "role=muted grow=0 minw=240"
    ' Un controllo senza ruolo e senza layout: resta esattamente dov'è.
    BUTTON   40, 220, 120, 40, "Fisso"   NAME "bNone"

    PRINT "-- prima (base 640x320) --"
    CALL Dump("bSave") : CALL Dump("bDel") : CALL Dump("bInfo") : CALL Dump("bMuted") : CALL Dump("bNone")

    ' 2) Resize: quota del delta e limiti si applicano insieme.
    WINDOW RESIZE 960, 480

    PRINT "-- dopo (960x480) --"
    CALL Dump("bSave") : CALL Dump("bDel") : CALL Dump("bInfo") : CALL Dump("bMuted") : CALL Dump("bNone")

    ' 3) Un altro tema: i controlli con ruolo restano leggibili senza toccare nulla.
    WINDOW THEME "light"

    PRINT "-- tema chiaro (960x480) --"
    CALL Dump("bSave") : CALL Dump("bDel") : CALL Dump("bInfo") : CALL Dump("bMuted") : CALL Dump("bNone")

    ' 4) Simmetria: tornando alla base i rect tornano quelli dichiarati.
    WINDOW RESIZE 640, 320
    WINDOW THEME "turbovision"

    PRINT "-- ritorno (640x320) --"
    CALL Dump("bSave") : CALL Dump("bDel") : CALL Dump("bInfo") : CALL Dump("bMuted") : CALL Dump("bNone")
END SUB
