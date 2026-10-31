' MiniTest — TYPENAME/VALTYPE degli ARRAY (fisso e dinamico), parità 4 backend.
' IL usa T[] → "ARRAY"; VM/C distingue l'array dalla lista via il flag is_array
' dell'oggetto lista (settato da CREATE_ARRAY). Prima `DIM d() = [...]` era una lista
' senza marker → TYPENAME "LIST`1" su VM/C.
' EXPECT: fixed|ARRAY|A|dyn|ARRAY|A
IMPORT SYSTEM

SUB Main()
    DIM a(3) AS INTEGER
    DIM d() AS STRING = ["x", "y"]
    PRINT "fixed|"; TYPENAME(a); "|"; SYSTEM.VALTYPE(a)
    PRINT "dyn|"; TYPENAME(d); "|"; SYSTEM.VALTYPE(d)
END SUB
