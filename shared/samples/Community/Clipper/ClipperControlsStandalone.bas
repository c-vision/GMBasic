IMPORT CONTROLS
' FILE: Samples/Clipper/ClipperControlsStandalone.bas
' Test controlli standalone: CHECKBOX, LISTBOX, COMBOBOX

SUB Main()
    PRINT "=== Standalone Controls Test ==="
    
    ' CHECKBOX standalone
    DIM ok AS BOOLEAN
    ok = FALSE
    PRINT "Premi Enter/Space per cambiare, Esc per confermare..."
    CHECKBOX  3, 3, "Accetto i termini" INTO ok
    PRINT "Checkbox: "; ok
    
    ' LISTBOX standalone
    DIM colore AS STRING
    colore = ""
    DIM colori AS ARRAY
    colori = {"Rosso", "Verde", "Blu", "Giallo", "Nero"}
    LISTBOX  6, 3, 15, 5, colori INTO colore
    PRINT "Colore scelto: "; colore
    
    ' COMBOBOX standalone
    DIM scelta AS STRING
    DIM opzioni AS ARRAY
    opzioni = {"Opzione A", "Opzione B", "Opzione C"}
    COMBOBOX  13, 3, 15, opzioni INTO scelta
    PRINT "Opzione scelta: "; scelta
    
    PRINT
    PRINT "=== Test completato ==="
END SUB
