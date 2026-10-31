' ==============================================================================
' SystemUdtIntrospection.bas — introspezione UDT e coercizione SYSTEM (parità 4 backend)
' ==============================================================================
' Copre: SYSTEM.CTYPE, SYSTEM.UDTPACKED, SYSTEM.UDTALIGNED, SYSTEM.VALTYPE,
'        SYSTEM.TYPENAME su un TYPE definito dall'utente.
' ==============================================================================
IMPORT SYSTEM

TYPE Persona
    Nome AS STRING * 20
    Eta AS INTEGER
    Saldo AS DOUBLE
END TYPE

SUB Main()
    DIM p AS Persona
    p.Nome = "Ada"
    p.Eta = 36
    p.Saldo = 1234.5

    PRINT "=== Introspezione UDT ==="
    PRINT "VALTYPE:    "; SYSTEM.VALTYPE(p)
    PRINT "TYPENAME:   "; SYSTEM.TYPENAME(p)
    PRINT "UDTPACKED:  "; SYSTEM.UDTPACKED(p)
    PRINT "UDTALIGNED: "; SYSTEM.UDTALIGNED(p)

    PRINT
    PRINT "=== Coercizione SYSTEM.CTYPE ==="
    PRINT "int(42.9) -> "; SYSTEM.CTYPE(42.9, "INTEGER")
    PRINT "dbl(3.5)  -> "; SYSTEM.CTYPE(3.5, "DOUBLE")
    PRINT "str(42)   -> "; SYSTEM.CTYPE(42, "STRING")
    PRINT "bool(1)   -> "; SYSTEM.CTYPE(1, "BOOLEAN")
    PRINT
    PRINT "Completato."
END SUB
