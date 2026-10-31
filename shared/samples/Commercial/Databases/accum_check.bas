SUB Main()
  DIM I AS INTEGER
  DIM Acc AS DOUBLE
  Acc = 0
  FOR I = 1 TO 10
    Acc = Acc + SQR(I) * 2.0
  NEXT I
  PRINT "acc="; Acc
  PRINT "done"
END SUB
