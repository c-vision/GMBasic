' NativeBackend smoke test — compilato in eseguibile nativo (--aot)
PRINT "Hello from Native AOT!"
PRINT "3 + 4 = "; 3 + 4
DIM s AS STRING = "basic native string"
PRINT "str: "; s
FOR i = 1 TO 3
    PRINT "iter "; i
NEXT i
PRINT "NATIVE OK"
