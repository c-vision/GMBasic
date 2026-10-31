' EXPECT: 1|2
' Doppia sintassi per TUTTI i controlli: posizionale e a clausole (AT/SIZE/TEXT/ITEMS/WIDTH).
' L'oracolo prova che entrambe le forme sono accettate ed eseguite (i controlli non bloccano).
IMPORT WINDOWS
IMPORT CONTROLS
SCREEN 0
DIM a AS INTEGER, b AS INTEGER
DIM c AS STRING, d AS STRING
DIM voci(2) AS STRING
voci(0) = "A" : voci(1) = "B" : voci(2) = "C"
CHECKBOX 2, 2, 20, "x" INTO a
CHECKBOX AT 2, 4 TEXT "x" INTO b
COMBOBOX 2, 6, 20, voci INTO c
COMBOBOX AT 2, 8 WIDTH 20 ITEMS voci INTO d
PRINT 1
PRINT 2
END
