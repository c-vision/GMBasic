IMPORT CONTROLS
' FILE: Samples/Clipper/ClipperPrinterDialog.bas
' Test PRINTERDIALOG control

SUB Main()
    DIM printer AS STRING
    DIM copies AS INTEGER
    DIM duplex AS BOOLEAN
    
    printer = ""
    copies = 1
    duplex = FALSE
    
    PRINT "Opening printer dialog..."
    PRINTERDIALOG "Print Setup" INTO printer, copies, duplex
    
    IF printer <> "" THEN
        PRINT "Printer: "; printer
        PRINT "Copies: "; copies
        PRINT "Duplex: "; duplex
    ELSE
        PRINT "Cancelled"
    END IF
END SUB
