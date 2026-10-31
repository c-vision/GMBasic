' FILE: Exceptions/ExceptionsCustom.bas
' Custom exception types: THROW NEW ClassName(args) + CATCH ex AS ClassName
'
' MODIFIED: wrapped in SUB Main for GPM compatibility.

IMPORT SYSTEM
IMPORT COLLECTIONS

' --- Define custom exception class ---
CLASS ValidationError
    PUBLIC DIM FieldName AS STRING
    PUBLIC DIM FieldValue AS STRING
    PUBLIC DIM Message AS STRING

    PUBLIC SUB NEW(fld AS STRING, val AS STRING, msg AS STRING)
        SELF.FieldName = fld
        SELF.FieldValue = val
        SELF.Message = msg
    END SUB
END CLASS

' --- Second custom exception class ---
CLASS DivideByZeroError
    PUBLIC DIM Expression AS STRING
    PUBLIC SUB NEW(expr AS STRING)
        SELF.Expression = expr
    END SUB
END CLASS

' Main entry point
SUB Main()
    CLS
    PRINT "=== Custom Exception Types ==="
    PRINT

    ' --- THROW and CATCH by type ---
    PRINT "--- 1. THROW NEW + CATCH by type ---"
    TRY
        THROW NEW ValidationError("email", "invalid", "Email format is invalid")
    CATCH ex AS ValidationError
        PRINT "Caught ValidationError!"
        PRINT "  Field:  "; ex.FieldName
        PRINT "  Value:  "; ex.FieldValue
        PRINT "  Msg:    "; ex.Message
    END TRY
    PRINT

    ' --- Multiple CATCH clauses ---
    PRINT "--- 2. Multiple CATCH clauses ---"
    FOR i = 1 TO 3
        TRY
            IF i = 1 THEN
                THROW NEW ValidationError("age", "-5", "Negative age")
            ELSEIF i = 2 THEN
                THROW NEW DivideByZeroError("x / 0")
            ELSE
                THROW 999
            END IF
        CATCH ex AS ValidationError
            PRINT "ValError: "; ex.Message
        CATCH ex AS DivideByZeroError
            PRINT "DivZero:  "; ex.Expression
        CATCH ex AS ERR
            PRINT "Generic:  "; ex.Description
        END TRY
    NEXT i
    PRINT

    PRINT "=== Fine ==="
END SUB
