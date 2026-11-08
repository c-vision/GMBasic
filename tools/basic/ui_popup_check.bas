' ui_popup_check.bas — GATE: aprire e chiudere un POPUP deve lasciare la pagina
' pixel-identica. Un controllo con popup (dropdown, menu, context menu, calendario)
' dipinge FUORI dal proprio rettangolo: se l'area non lo include, chiudendolo
' restano i pixel della lista (o manca il pezzo riaperto).
' Lo script tools/sh/ui_popup_check.sh esegue questo file e confronta le immagini
' con gmbimgdiff (pop0 = pagina pulita, pop3 = dopo due cicli apri/chiudi).
IMPORT CONTROLS
IMPORT GRAPHICS

SCREEN 12, 640, 420, 32
WINDOW THEME "glass"
DIM n AS INTEGER, i AS INTEGER
DIM s AS STRING
CLS 0

PANEL 20, 20, 600, 380, "PROVA POPUP" NAME "p_p"
LABEL 40, 62, 560, "questo testo sta SOTTO la lista: se resta sporco, il gate e' rosso" NAME "p_l"
LISTBOX 40, 92, 560, 96, "uno|due|tre|quattro" INTO n NAME "p_b"
COMBOBOX 40, 240, 280, "alpha|beta|gamma|delta" INTO s NAME "p_c"
CONTEXTMENU 40, 300, 280, "Copy|Paste|Delete" NAME "p_m"

FOR i = 1 TO 12 : DOEVENTS : NEXT i
SAVE SCREEN "work/pop0.png"

' ciclo 1: dropdown
CONTROL SET "p_c", "open", 1
FOR i = 1 TO 12 : DOEVENTS : NEXT i
SAVE SCREEN "work/pop1.png"
CONTROL SET "p_c", "open", 0
FOR i = 1 TO 12 : DOEVENTS : NEXT i

' ciclo 2: context menu
CONTROL SET "p_m", "open", 1
FOR i = 1 TO 12 : DOEVENTS : NEXT i
SAVE SCREEN "work/pop2.png"
CONTROL SET "p_m", "open", 0
FOR i = 1 TO 12 : DOEVENTS : NEXT i
SAVE SCREEN "work/pop3.png"

SCREEN 0
PRINT "POPUP-CHECK-OK"
