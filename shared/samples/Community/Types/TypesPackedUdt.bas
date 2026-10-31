' FILE: Types/TypesPackedUdt.bas
' Test PACKED TYPE layout per FFI interop

PACKED TYPE PackedPoint
    x AS INTEGER
    y AS INTEGER
END TYPE

TYPE UnpackedPoint
    x AS INTEGER
    y AS INTEGER
END TYPE

PACKED TYPE PackedRecord
    id AS LONG
    flag AS BIT
    value AS DOUBLE
END TYPE

' Stessi campi, senza PACKED: il layout è a allineamento naturale (24 byte contro 17).
TYPE UnpackedRecord
    id AS LONG
    flag AS BIT
    value AS DOUBLE
END TYPE

DIM p AS PackedPoint
p.x = 10
p.y = 20
PRINT "PACKED TYPE: p.x = "; p.x; ", p.y = "; p.y

DIM up AS UnpackedPoint
up.x = 30
up.y = 40
PRINT "UNPACKED TYPE: up.x = "; up.x; ", up.y = "; up.y

DIM r AS PackedRecord
r.id = 42
r.flag = TRUE
r.value = 3.14159
PRINT "PACKED RECORD: id="; r.id; " flag="; r.flag; " value="; r.value

' UDTPACKED/UDTALIGNED sono PROPRIETÀ compile-time del TYPE (valori numerici): il primo
' conta i CAMPI, il secondo vale 1. NON serializzano nulla (la dimensione in byte del
' layout è la funzione SIZEOF, che segue il canone TypeLayout su tutti i backend).
PRINT "UDTPACKED(p) = "; UDTPACKED(p); " campi"
PRINT "UDTALIGNED(p) = "; UDTALIGNED(p)

' SIZEOF = byte del layout statico: PACKED = somma dei campi, ordinario = allineamento
' naturale di ogni campo e taglia finale multipla del massimo allineamento.
DIM sz AS LONG
sz = SIZEOF(PackedPoint)
DIM sz2 AS LONG
sz2 = SIZEOF(UnpackedPoint)
DIM sz3 AS LONG
sz3 = SIZEOF(PackedRecord)
DIM sz4 AS LONG
sz4 = SIZEOF(UnpackedRecord)
' Atteso: 4 / 4 / 17 / 24 (TYPE con due INTEGER ha lo stesso layout packed e non packed;
' con LONG+BIT+DOUBLE invece il PACKED TYPE recupera i 7 byte di padding).
PRINT "SIZEOF PackedPoint: "; sz
PRINT "SIZEOF UnpackedPoint: "; sz2
PRINT "SIZEOF PackedRecord: "; sz3
PRINT "SIZEOF UnpackedRecord: "; sz4

PRINT "OK. PACKED TYPE test completato."
END
