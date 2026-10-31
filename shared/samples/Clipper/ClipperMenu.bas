IMPORT CONTROLS
' FILE: Samples/Clipper/ClipperMenu.bas
' Test MENU standalone

SUB Main()
    PRINT "=== Menu Test ==="
    
    DIM scelta AS INTEGER
    DIM voci(3) AS STRING
    voci(0) = "Nuovo" : voci(1) = "Apri" : voci(2) = "Salva" : voci(3) = "Esci"
    
    MENU  3, 3, voci INTO scelta
    
    PRINT "Hai scelto l'opzione: "; scelta
    IF scelta >= 0 AND scelta < 4 THEN
        PRINT "Voce: "; voci(scelta)
    END IF
END SUB
