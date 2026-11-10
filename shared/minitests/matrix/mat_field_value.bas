' EXPECT: in= 12|out= 99|sum= 12
' Stage D (F4): campo di CLASSE rank-2 DOUBLE = VALORE matrice (descrittore): allocazione
' nel costruttore, lettura/scrittura dentro (SELF) e fuori, aritmetica sugli elementi.
CLASS GridHolder
  PUBLIC DIM cells(1, 2) AS DOUBLE
  SUB Fill()
    DIM i AS LONG
    DIM j AS LONG
    FOR i = 0 TO 1
      FOR j = 0 TO 2
        cells(i, j) = i * 10 + j
      NEXT j
    NEXT i
  END SUB
END CLASS

SUB Main()
  DIM h AS GridHolder
  h = NEW GridHolder()
  CALL h.Fill()
  PRINT "in="; h.cells(1, 2)
  h.cells(0, 1) = 99
  PRINT "out="; h.cells(0, 1)
  PRINT "sum="; h.cells(0, 0) + h.cells(1, 2)
END SUB
