' Minimal PACKED TYPE test
PACKED TYPE PackedPoint
    x AS INTEGER
    y AS INTEGER
END TYPE

DIM p AS PackedPoint
p.x = 10
p.y = 20
PRINT p.x; p.y
END
