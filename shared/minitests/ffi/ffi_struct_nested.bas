' EXPECT: o.I.X= 3.5|o.Y= 7|r.I.X= 4.5|r.Y= 8
' Struct annidate FFI (2026-09-29): un campo di uno UDT-per-valore FFI il cui tipo e' A SUA
' VOLTA un altro TYPE semplice — dopo flattening ricorsivo (I.X + Y = 2 foglie totali) e'
' ABI-identico a un UDT piatto con 2 campi (stesso limite 1-2 campi FOGLIA, indipendente
' dalla profondita' di nesting). Copre UDT annidato SIA come PARAMETRO SIA come RITORNO
' nella stessa chiamata (roundtrip: vec_nested_roundtrip incrementa I.X di 1 e Y di 1).
TYPE Inner
    X AS DOUBLE
END TYPE
TYPE Outer
    I AS Inner
    Y AS LONG
END TYPE

DECLARE FUNCTION VecNestedRoundtrip LIB "libvec_module" ALIAS "vec_nested_roundtrip" (o AS Outer) AS Outer

SUB Main()
    DIM o AS Outer
    o.I.X = 3.5
    o.Y = 7
    PRINT "o.I.X="; o.I.X
    PRINT "o.Y="; o.Y

    DIM r AS Outer
    r = VecNestedRoundtrip(o)
    PRINT "r.I.X="; r.I.X
    PRINT "r.Y="; r.Y
END SUB
