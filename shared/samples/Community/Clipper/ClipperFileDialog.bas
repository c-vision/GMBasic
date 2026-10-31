IMPORT CONTROLS
' FILE: Samples/Clipper/ClipperFileDialog.bas
' Test FILEDIALOG control

SUB Main()
    DIM file AS STRING
    file = ""
    
    PRINT "Opening file dialog..."
    FILEDIALOG  "*.*", "Select a file" INTO file
    
    IF file <> "" THEN
        PRINT "Selected: "; file
    ELSE
        PRINT "Cancelled"
    END IF
END SUB
