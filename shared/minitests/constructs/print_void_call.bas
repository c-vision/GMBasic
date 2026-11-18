' MiniTest CONSTRUCTS — PRINT di una SUB (procedura VOID): la chiamata si ESEGUE (e il suo
' output compare), ma l'argomento non produce un valore da stampare.
'
' Prima: IL emetteva `box System.Void` → IL MALFORMATO ("stack underflow" su
' PrintFormatValue) e InvalidProgramException all'esecuzione — anche dentro TRY; C99
' stampava il `__ret` della SUB (0). Ora tutte e 4 le gambe: niente valore.
'
' EXPECT: 1:|[F]|2:|[F]|ab|3:|catturato  11|fine
SUB F()
    PRINT "[F]"
END SUB

SUB Scoppia()
    ERROR 11, "x"
END SUB

SUB Main()
    PRINT "1:"
    PRINT F()
    PRINT "2:"
    PRINT "a"; F(); "b"
    PRINT "3:"
    TRY
        PRINT "parziale"; Scoppia()
    CATCH e AS ERR
        PRINT "catturato "; e.Number
    END TRY
    PRINT "fine"
END SUB
