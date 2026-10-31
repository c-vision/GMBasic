' EXPECT: scaled= 20|add= 42|prime= 1
' FFI drop-in: la dylib sta SOLO in libs/ (minitests/ffi/libs/libffidropin.dylib),
' NON accanto al programma → la risoluzione passa dal walk-up `libs/` (F1/F2).
IMPORT FFIDROPIN

SUB Main()
    PRINT "scaled="; dropin_scale(4, 5)
    PRINT "add="; dropin_add(21, 21)
    PRINT "prime="; dropin_prime(17)
END SUB
