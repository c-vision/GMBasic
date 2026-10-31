' FILE: Types/TypesDirectCast.bas
' ======================================================================================================================
' DIRECTCAST(object, TypeName) — cast diretto con verifica tipo a runtime.
' Se l'oggetto non e' del tipo richiesto, viene generato un errore TypeMismatch.
' Sintassi: result = DIRECTCAST(expression, TypeName)
' ======================================================================================================================

IMPORT STRINGS
IMPORT COLLECTIONS

SUB Main()
    CLS
    PRINT "=== TypesDirectCast ==="
    PRINT

    PRINT "--- 1. STRINGBUILDER ---"
    DIM sb AS STRINGBUILDER
    sb.Append("Test")
    DIM obj AS ANY
    obj = sb
    DIM sb2 AS STRINGBUILDER
    sb2 = DIRECTCAST(obj, STRINGBUILDER)
    sb2.Append(" OK")
    PRINT "  Risultato: '"; sb2.ToString(); "'"
    PRINT

    PRINT "--- 2. LIST ---"
    DIM lst AS LIST
    lst.Add(10)
    lst.Add(20)
    obj = lst
    DIM lst2 AS LIST
    lst2 = DIRECTCAST(obj, LIST)
    PRINT "  Count:"; lst2.Count
    PRINT

    PRINT "--- 3. REGEX ---"
    DIM reg AS REGEX
    DIM reg2 = DIRECTCAST(reg, REGEX)
    PRINT "  Tipo:"; TYPENAME(reg2)
    PRINT

    PRINT "--- 4. LONG ---"
    obj = 42
    DIM num AS LONG
    num = DIRECTCAST(obj, LONG)
    PRINT "  Valore:"; num
    PRINT

    PRINT "--- 5. INTEGER ---"
    obj = 100
    DIM n AS INTEGER
    n = DIRECTCAST(obj, INTEGER)
    PRINT "  Valore:"; n
    PRINT

    PRINT "Test completato."
END SUB
