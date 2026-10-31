' EXPECT: AC|EF
' IF ... THEN PRINT ...; ELSE PRINT ...; su UNA riga: il PRINT del ramo THEN non deve
' inghiottire ELSE come argomento (parser C condiviso: C99Native/VM-C).
SUB Main()
  IF 1 THEN PRINT "A"; ELSE PRINT "B";
  PRINT "C"
  IF 0 THEN PRINT "D"; ELSE PRINT "E";
  PRINT "F"
END SUB
