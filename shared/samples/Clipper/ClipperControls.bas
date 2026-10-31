' FILE: Samples/Clipper/ClipperControls.bas
' Test @...GET con tutti i controlli e validazione

SUB Main()
    PRINT "=== Clipper Controls Test ==="
    
    DIM nome AS STRING
    nome = "Default"
    
    DIM eta AS INTEGER
    eta = 25
    
    DIM attivo AS BOOLEAN
    attivo = TRUE
    
    DIM colore AS STRING
    colore = "Rosso"
    
    DIM scelta AS STRING
    
    ' ListBox con scelta
    @ 3, 3 SAY "Scegli colore:"
    @ 4, 3 GET colore TYPE "listbox" ITEMS {"Rosso", "Verde", "Blu", "Giallo"} SIZE 15, 4
    
    ' Text input con PICTURE uppercase
    @ 3, 25 SAY "Nome:"
    @ 4, 25 GET nome PICTURE "@!" SIZE 15
    
    ' Spinner con MIN/MAX
    @ 7, 3 SAY "Eta (18-99):"
    @ 8, 3 GET eta TYPE "spinner" MIN 18 MAX 99 STEP 1 SIZE 10
    
    ' Checkbox
    @ 7, 25 SAY "Attivo:"
    @ 8, 25 GET attivo TYPE "checkbox" CAPTION "Abilita"
    
    ' ComboBox
    @ 11, 3 SAY "Seleziona:"
    @ 12, 3 GET scelta TYPE "combobox" ITEMS {"Opzione A", "Opzione B", "Opzione C"} SIZE 15
    
    READ
    
    PRINT
    PRINT "=== RISULTATI ==="
    PRINT "Nome: "; nome
    PRINT "Eta: "; eta
    PRINT "Attivo: "; attivo
    PRINT "Colore: "; colore
    PRINT "Scelta: "; scelta
END SUB
