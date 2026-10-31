' FILE: System/SystemFFIPacked.bas
' PACKED TYPE per FFI struct passing

PACKED TYPE PackedPoint
    x AS INTEGER
    y AS INTEGER
END TYPE

DIM p1 AS PackedPoint
p1.x = 10
p1.y = 20
PRINT "FFI PACKED: p1 = "; p1.x; p1.y
PRINT "OK. PACKED TYPE FFI test completato."
END
