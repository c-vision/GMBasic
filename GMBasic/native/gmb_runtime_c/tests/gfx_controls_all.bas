' Smoke test RENDERING: tutti i controlli su pagina grafica (tema chiaro).
' Verifica: GMB_GFX_DUMMY? no: eseguito con gmb_c99native_run.sh, salva un PNG.
IMPORT WINDOWS
IMPORT CONTROLS
IMPORT GRAPHICS
SCREEN 12, 760, 560, 32
WINDOW THEME "light"
FONT "font.ttf", 12
CLS 1
DIM n AS INTEGER, s AS STRING
DIM voci(2) AS STRING
voci(0) = "Mela" : voci(1) = "Pera" : voci(2) = "Uva"
LABEL 3, 2, 20, "Label"
CHECKBOX 3, 5, 20, "Check" INTO n
TOGGLE 3, 8, 20, "Toggle" INTO n
RADIO 3, 11, 20, voci INTO n
COMBOBOX 3, 15, 20, voci INTO s
LISTBOX 3, 19, 20, 4, voci INTO n
TAB 3, 26, 22, voci INTO n
NUMERICUPDOWN 3, 30, 20, 0, 10, 3 INTO n
STARRATING 3, 33, 20, 5, 3
SCROLLBAR 3, 37, 20, 0, 100, 40 INTO n
SPINNER 3, 40, 20, "line"
PROGRESS 3, 43, 20, 60
STATUSBAR 3, 46, 70, "status ok"
TEXTBOX 3, 49, 24, 12 INTO s
BIGTEXT 3, 52, "BIG"
GRID 30, 2, 28, 5, "Nome|Qta", "a|1|b|2" INTO n
TREEVIEW 30, 10, 28, 5, "root/a|root/b|root/c" INTO n
DATALIST 30, 18, 24, 5, voci INTO n
CODEAREA 30, 26, 28, 5, "x = 1|y = 2" INTO s
LOGVIEW 30, 34, 24, 4, "riga1|riga2|riga3" INTO n
HEXVIEW 30, 41, 24, 4, "41424344" INTO n
REORDERLISTBOX 62, 2, 24, 5, voci INTO n





MENU 3, 56, 70, "File|Modifica|Aiuto"
FUNCTIONKEYBAR 3, 60, 90, "Apri|Salva|Aiuto"
DIM f AS INTEGER
FOR f = 1 TO 20 : WINDOW PUMP 16 : NEXT
SAVE SCREEN "work/controls_all_c.png"
SCREEN 0
PRINT "saved"
