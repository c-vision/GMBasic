' MiniTest PRINT.LONG — interi grandi in G15 (parità 4 backend).
' Canone (dal fix VM-C-Print-LONG-Grande e allineamento IL): OGNI numero — letterale,
' variabile LONG/INTEGER, espressione — è reso come double in "%.15g" → notazione
' scientifica oltre ~15 cifre significative. Prima emitter C# stampava le cifre intere
' (1700000000000000) per variabili/espressioni, divergendo da IL/PCode/VM-C/C99Native.
' Oracolo: canon G15 (IL allineato a PCode/VM-C/C99Native — vedi (solved) VM-C-Print-LONG).
' EXPECT: x= 1.7E+15|y= 100|lit16 1.23456789012346E+15|sum 1.7E+15
SUB Main()
    DIM x AS LONG
    x = 1700000000000000
    PRINT "x="; x
    DIM y AS INTEGER
    y = 100
    PRINT "y="; y
    PRINT "lit16"; 1234567890123456
    PRINT "sum"; x + 1
END SUB
