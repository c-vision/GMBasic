' EXPECT: A
' COMBOBOX INTO stringa da array variabile: label iniziale = primo item (4 gambe).
IMPORT WINDOWS
IMPORT CONTROLS
SCREEN 0
DIM c AS STRING
DIM voci(2) AS STRING
voci(0) = "A" : voci(1) = "B" : voci(2) = "C"
COMBOBOX 2, 6, 20, voci INTO c
PRINT c
END
