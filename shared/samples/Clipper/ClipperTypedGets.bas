' ClipperTypedGets.bas — @GET con tutti i TYPE su widget form (grafica).
' Tab/Enter navigano, frecce modificano spinner/listbox, spazio toggla la checkbox.
SCREEN 12, 900, 600, 32
WINDOW THEME "soft"
FONT "font.ttf", 14
DIM nome AS STRING
nome = ""
DIM qta AS INTEGER
qta = 1
DIM scad AS STRING
scad = "01/01/2026"
DIM colore AS STRING
colore = ""
DIM attivo AS INTEGER
attivo = 0
@ 3, 4 SAY "Nome:"
@ 3, 14 GET nome PICTURE "@!" SIZE 16
@ 6, 4 SAY "Qta:"
@ 6, 14 GET qta TYPE "Spinner" RANGE 0, 99 STEP 1 SIZE 8
@ 9, 4 SAY "Scadenza:"
@ 9, 14 GET scad TYPE "DatePicker" SIZE 14
@ 12, 4 SAY "Colore:"
@ 12, 14 GET colore TYPE "listbox" ITEMS {"Rosso","Verde","Blu"} SIZE 14, 4
@ 15, 4 SAY "Attivo:"
@ 15, 14 GET attivo TYPE "checkbox" CAPTION "Si" SIZE 6
@ 18, 4 SAY "TAB=next  ENTER=conferma  ESC=annulla"
READ
SAVE SCREEN "work/clipper_typed_gets.png"
SCREEN 0
PRINT "nome="; nome
PRINT "qta="; qta
PRINT "scad="; scad
PRINT "colore="; colore
PRINT "attivo="; attivo
