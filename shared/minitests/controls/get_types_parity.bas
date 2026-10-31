' TAG: interactive
' @GET tipizzato (text/spinner/datepicker/checkbox/listbox) — richiede input: READ.
' EXPECT: nome=AB|qta=3|colore=Blu|attivo=1
SCREEN 12, 900, 600, 32
WINDOW THEME "soft"
FONT "font.ttf", 14
DIM nome AS STRING
DIM qta AS INTEGER
DIM scad AS STRING
DIM colore AS STRING
DIM attivo AS INTEGER
nome = ""
qta = 1
scad = "01/01/2026"
colore = ""
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
READ
SCREEN 0
PRINT "nome="; nome
PRINT "qta="; qta
PRINT "colore="; colore
PRINT "attivo="; attivo
