' MiniTest MATRIX — variabili omonime di builtin (amax/AMAX, amin/AMIN).
' Una variabile-array DICHIARATA ha la priorita sul builtin omonimo in forma nuda:
' amax(0) e accesso all'elemento, non chiamata ad AMAX (bug MATRIX.ARGMAX/ARGMIN:
' prima il parser risolveva al builtin, poi il binder timbrava l'ID proc, poi il
' codegen PCode emetteva OP_CALL_N senza corpo e la VM loopava al 100% CPU).
' Uno SCALARE omonimo invece non devia le chiamate al builtin (solo gli array).
' Nello stesso scope dell'array omonimo il builtin si chiama QUALIFICATO
' (bug C99Native-AMAX-Dopo-ARGMAX, chiuso da altra sessione il 2026-09-13).
' Anche il nome INTERO del builtin (argmax/ARGMAX) segue la stessa regola.
' EXPECT: argmax:  1 , 1|argmin:  1 , 0|qual:  7 , 1|fullname:  1 , 1|scalar:  4 , 5
IMPORT ARRAYS

SUB Main()
    a = MATRIX.CREATE(2, 2)
    a = MATRIX.SETVALUE(a, 0, 0, 1) : a = MATRIX.SETVALUE(a, 0, 1, 2)
    a = MATRIX.SETVALUE(a, 1, 0, 0) : a = MATRIX.SETVALUE(a, 1, 1, 3)
    DIM amax() = MATRIX.ARGMAX(a)
    PRINT "argmax: "; amax(0); ","; amax(1)
    DIM amin() = MATRIX.ARGMIN(a)
    PRINT "argmin: "; amin(0); ","; amin(1)
    DIM flat() = {4, 1, 7}
    PRINT "qual: "; ARRAYS.AMAX(flat); ","; amax(0)
    DIM argmax() = MATRIX.ARGMAX(a)
    PRINT "fullname: "; argmax(0); ","; argmax(1)
    ShowScalar()
END SUB

SUB ShowScalar()
    DIM amax = 5
    v = {3, 1, 4}
    PRINT "scalar: "; AMAX(v); ","; amax
END SUB

