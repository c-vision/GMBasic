' LayoutManager.bas — LAYOUT MANAGER dei controlli.
'
' EXPAND / ANCHOR / ALIGN sono PROPRIETÀ del controllo (stessa posizione di
' NAME/TAG, dopo lo STYLE), non un widget e non un contenitore: nessun nuovo
' oggetto nel programma, il controllo resta quello che è.
'
' Sintassi:
'   BUTTON x, y, w, h, "testo" [ON CLICK sub] [NAME "n"]
'          [EXPAND H|V|BOTH|NONE]
'          [ANCHOR LEFT|RIGHT|TOP|BOTTOM|ALL|NONE   ('+' per combinare)]
'          [ALIGN LEFT|CENTER|RIGHT|TOP|MIDDLE|BOTTOM ('+' per combinare)]
'
' Semantica (simmetrica: la stessa formula vale in crescita e in riduzione):
'   - entrambi i bordi di un asse agganciati -> il controllo si ALLARGA del delta;
'   - un solo bordo agganciato              -> il controllo si SPOSTA con quel bordo;
'   - nessun bordo                          -> decide ALIGN (start/center/end);
'   - EXPAND è la scorciatoia che aggancia entrambi i bordi dell'asse;
'   - ANCHOR esplicito ha la precedenza su EXPAND per quell'asse.
' La base del delta è il rect DICHIARATO alla creazione + la dimensione della
' pagina in quel momento. Il reflow è idempotente (ripeterlo non cambia nulla).
'
' Esecuzione headless deterministica: WINDOW RESIZE fissa/porta la pagina alla
' dimensione voluta (come il resize della finestra) e fa scattare il layout.
'
' Valori attesi (base 640x320 -> 960x480; delta +320 in x, +160 in y):
'   bE   EXPAND BOTH          -> x=40  y=40   w=520  h=200*
'   bH   ANCHOR LEFT+RIGHT    -> x=40  y=100  w=520  h=40*
'   bC   ANCHOR RIGHT+BOTTOM  -> x=720 y=380  w=180  h=40*
'   bM   ALIGN CENTER+MIDDLE  -> x=410 y=240  w=120  h=40*
'   bF   (nessun layout)      -> x=40  y=220  w=120  h=40*
'   lbE  EXPAND BOTH          -> x=40  y=260  w=520  h=180
'   (* altezza solo in GRAFICA: nel percorso TEXT il bottone ha altezza propria,
'      per questo la verifica portabile usa LISTBOX per l'asse verticale.)
IMPORT WINDOWS
IMPORT CONTROLS

DIM gx AS DOUBLE
DIM gy AS DOUBLE
DIM gw AS DOUBLE
DIM gh AS DOUBLE
' destinazione INTO del LISTBOX: è l'indice dell'ELEMENTO SELEZIONATO (legato al
' controllo), NON l'handle. Alla creazione la variabile non viene toccata — il valore
' arriva quando l'utente seleziona — quindi qui non si legge. L'handle si ottiene con
' la clausola ID o con CONTROL GET "<nome>", "handle".
DIM lbScelta AS INTEGER

SUB Dump(nome AS STRING)
    CONTROL GET nome, "left"   INTO gx
    CONTROL GET nome, "top"    INTO gy
    CONTROL GET nome, "width"  INTO gw
    CONTROL GET nome, "height" INTO gh
    PRINT nome; " x="; STR$(gx); " y="; STR$(gy); " w="; STR$(gw); " h="; STR$(gh)
END SUB

SUB Main()
    ' 1) Dimensione BASE della pagina: da qui in poi il layout misura il DELTA.
    WINDOW RESIZE 640, 320

    BUTTON   40,  40, 200, 40, "Expand" NAME "bE"  EXPAND BOTH
    BUTTON   40, 100, 200, 40, "H"      NAME "bH"  ANCHOR LEFT+RIGHT
    BUTTON  400, 220, 180, 40, "Corner" NAME "bC"  ANCHOR RIGHT+BOTTOM
    BUTTON  250, 160, 120, 40, "Centro" NAME "bM"  ALIGN CENTER+MIDDLE
    BUTTON   40, 220, 120, 40, "Fisso"  NAME "bF"
    ' LISTBOX in FORMA PIXEL con clausole complete (NAME + EXPAND BOTH): la stessa forma
    ' del parser nativo. INTO è opzionale e lega la variabile all'ELEMENTO SELEZIONATO
    ' (indice), non all'handle: alla creazione la variabile resta com'è, come sul
    ' C99Native (che passa double* var a gmb_control_add_listbox). L'handle del controllo
    ' si legge con la clausola ID o con CONTROL GET "lbE", "handle".
    LISTBOX  40, 260, 200, 20, "uno,due" INTO lbScelta NAME "lbE" EXPAND BOTH

    PRINT "-- prima (base 640x320) --"
    CALL Dump("bE") : CALL Dump("bH") : CALL Dump("bC") : CALL Dump("bM") : CALL Dump("bF") : CALL Dump("lbE")

    ' 2) Resize: il layout segue il delta. Idempotente: ripeterlo non cambia nulla.
    WINDOW RESIZE 960, 480
    WINDOW RESIZE 960, 480

    PRINT "-- dopo (960x480) --"
    CALL Dump("bE") : CALL Dump("bH") : CALL Dump("bC") : CALL Dump("bM") : CALL Dump("bF") : CALL Dump("lbE")

    ' 3) Simmetria: tornando alla base i rect tornano quelli dichiarati.
    WINDOW RESIZE 640, 320

    PRINT "-- ritorno (640x320) --"
    CALL Dump("bE") : CALL Dump("bH") : CALL Dump("bC") : CALL Dump("bM") : CALL Dump("bF") : CALL Dump("lbE")
END SUB
