' FILE: Types/TypesNullConditional.bas
' Null-conditional operator ?. — accesso sicuro su membri
'
' Sintassi: expr?.membro — restituisce NIL se expr è NIL invece di errore

IMPORT SYSTEM
IMPORT COLLECTIONS

CLS
PRINT "=== Null-Conditional Operator (?.) ==="
PRINT

' --- ?. su UDT nullable ---
PRINT "--- 1. ?. member access su Contatto? ---"
TYPE Contatto
    Nome AS STRING * 32
    Eta  AS INTEGER
END TYPE

DIM c AS Contatto?
c = NIL
PRINT "c?.Nome (c=NIL): "; c?.Nome
PRINT "c?.Eta  (c=NIL): "; c?.Eta

c = NEW Contatto()
c.Nome = "Mario" : c.Eta = 30
PRINT "c?.Nome (c=ok):  "; c?.Nome
PRINT "c?.Eta  (c=ok):  "; c?.Eta
PRINT

PRINT "=== Fine ==="
END
