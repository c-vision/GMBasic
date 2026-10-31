' FILE: Concurrency/GpmV2Recursion.bas
' =================================================================================================
'       G4 (piano ILBackend-GPM): ricorsione sotto la GPM v2
' =================================================================================================
'
' Una FUNCTION ricorsiva may-park (SLEEP nel corpo): la Call annidata crea frame nidificati
' nel driver; il RETURN di ogni livello scrive nella destinazione del chiamante.
'
' Output atteso:
'   FACT-24
'   MAIN-DONE

FUNCTION Fact(N AS LONG) AS LONG
  SLEEP 1
  IF N <= 1 THEN
    RETURN 1
  END IF
  DIM Sotto AS LONG
  Sotto = Fact(N - 1)
  RETURN N * Sotto
END FUNCTION

SUB Main()
  DIM R AS LONG
  R = Fact(4)
  PRINT "FACT-"; R
  PRINT "MAIN-DONE"
END SUB
