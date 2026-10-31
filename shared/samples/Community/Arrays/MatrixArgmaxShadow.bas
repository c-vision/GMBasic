' Sample MATRIX — variabili omonime di builtin (ARGMAX/ARGMIN con amax/amin).
' Una variabile-array dichiarata ha la priorita sul builtin omonimo: amax(0) legge
' l'elemento, non chiama AMAX. Verde su 4 backend (IL/PCodeVM/VM-C/C99Native,
' parity4 PASS).
IMPORT MATRIX
IMPORT ARRAYS

SUB Main()
    ' Matrice 3x3 con massimo (9 in [1,1]) e minimo (0 in [1,2]) unici e noti.
    DIM m = MATRIX.CREATE(3, 3)
    DIM r0() = {4, 1, 7}
    DIM r1() = {2, 9, 0}
    DIM r2() = {5, 3, 6}
    FOR j = 0 TO 2
        m = MATRIX.SETVALUE(m, 0, j, r0(j))
        m = MATRIX.SETVALUE(m, 1, j, r1(j))
        m = MATRIX.SETVALUE(m, 2, j, r2(j))
    NEXT j

    ' Posizione del massimo e del minimo in variabili omonime dei builtin.
    DIM amax() = MATRIX.ARGMAX(m)
    DIM amin() = MATRIX.ARGMIN(m)
    PRINT "max at: "; amax(0); ","; amax(1); " value: "; MATRIX.GETVALUE(m, amax(0), amax(1))
    PRINT "min at: "; amin(0); ","; amin(1); " value: "; MATRIX.GETVALUE(m, amin(0), amin(1))

    ' Controllo incrociato: AMAX/AMIN sugli stessi dati. Nello stesso scope che
    ' dichiara l'array omonimo il builtin si chiama in forma QUALIFICATA
    ' (ARRAYS.AMAX): la forma nuda AMAX(flat) accederebbe all'array amax.
    flat = {4, 1, 7, 2, 9, 0, 5, 3, 6}
    PRINT "flat max: "; ARRAYS.AMAX(flat); " flat min: "; ARRAYS.AMIN(flat)

    ' Lo scalare omonimo non devia il builtin.
    DIM peak = 99
    PRINT "peak: "; peak; " builtin still: "; ARRAYS.AMAX(flat)
END SUB

