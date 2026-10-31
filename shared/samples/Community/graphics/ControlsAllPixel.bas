' ControlsAllPixel.bas — tutti i controlli (famiglia unica) disegnati in una pagina grafica.
' Il renderer pixel dei controlli TUI-first (draw_px_fn) li mostra sopra la pagina.
IMPORT WINDOWS
IMPORT CONTROLS
IMPORT GRAPHICS

SCREEN 12
FONT "font.ttf", 12
CLS 1
COLOR 15, 1
LINE (0, 0)-(639, 22), 4, "BF"
DRAW TEXT "CONTROLS — TUTTI I CONTROLLI IN UNA PAGINA GRAFICA", 10, 4

DIM n AS INTEGER, sel AS INTEGER, s AS STRING
DIM voci(2) AS STRING
voci(0) = "Mela" : voci(1) = "Pera" : voci(2) = "Uva"
DIM righe(2) AS STRING
righe(0) = "a|1" : righe(1) = "b|2" : righe(2) = "c|3"

LABEL 3, 2, 10, "Label"
CHECKBOX 3, 14, 10, "Check" INTO sel
TOGGLE 3, 26, 10, "Toggle" INTO sel
COMBOBOX 3, 40, 10, voci INTO s
RADIO 3, 54, 10, voci INTO s
SPINNER 3, 68, 10, "spinner"
NUMERICUPDOWN 5, 2, 10, 0, 10, 1 INTO n
STARRATING 5, 14, 10, 5, 3 INTO n
SCROLLBAR 5, 26, 10, 0, 10, 20, 5 INTO n
PROGRESS 5, 40, 12, n
TEXTBOX 5, 56, 12, 1 INTO s
LISTBOX 8, 2, 16, 5, voci INTO sel
TAB 8, 22, 16, voci INTO sel
DATALIST 8, 42, 14, 5, voci INTO sel
GRID 8, 60, 18, 5, "Nome|Val", righe INTO sel
TREEVIEW 15, 2, 16, 5, "a/b|c/d" INTO sel
LOGVIEW 15, 22, 18, 5, "riga1|riga2|riga3" INTO sel
HEXVIEW 15, 44, 16, 5, "41424344" INTO sel
BIGTEXT 15, 62, "BIG"
CODEAREA 22, 2, 16, 5, "x = 1|y = 2" INTO s
REORDERLISTBOX 22, 22, 18, 5, voci INTO sel
ALERTBOX 22, 44, 18, 4, "Attenzione"
CONFIRMATIONBOX 22, 64, 14, 4, "Sicuro?" INTO sel
FILEEXPLORER 28, 2, 20, 6, "." INTO s
DASHBOARD 28, 24, 16, 4, "A|10|B|20" INTO sel
COLORPICKER 28, 42, 10, 3 INTO sel
DATEPICKER 28, 54, "dd/mm/yyyy" INTO s
TBROWSE 28, 68, 12, 3, {{"C1", 10}, {"C2", 10}}, {{"a", "1"}, {"b", "2"}} INTO sel
STATUSBAR 30, 2, 60, "pronto"
MENU 32, 2, 40, "File|Modifica|Vista" INTO sel
BUTTON 200, 120, 90, 24, "OK" ON CLICK OnOk
CHECKBOX 200, 160, 160, "Check pixel" INTO sel
TOGGLE 200, 190, 160, "Toggle pixel" INTO sel
PROGRESS 200, 220, 160, n

DIM i AS INTEGER
FOR i = 1 TO 12
    WINDOW PUMP 16
NEXT i
SAVE SCREEN "work/controls_all.png"
FOR i = 1 TO 3
    WINDOW PUMP 16
NEXT i
PRINT "ok"
END
SUB OnOk
END SUB
