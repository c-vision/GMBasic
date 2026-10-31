' EXPECT: a= 2  b= 3  lines= 2  first=riga1 scalari= 0
' DIM n AS LONG, a$() AS STRING, ... : gli array dinamici di stringhe SUCCESSIVI a uno
' scalare restavano non dichiarati nel parser C (`a$() AS STRING` lasciava i token AS
' non consumati) → SPLIT/READLINES fallivano con "SPLIT richiede un target DIM ...()
' AS STRING dinamico". Copre anche la conversione lista→array di IO.READLINES.
IMPORT IO

SUB Main()
  DIM n AS LONG, a$() AS STRING, m AS LONG, b$() AS STRING, ls$() AS STRING
  DIM path AS STRING, h AS LONG

  a$ = SPLIT("uno,due", ",")
  b$ = SPLIT("tre,quattro,cinque", ",")

  path = "/tmp/gmb_mt_dim_dynstr.txt"
  h = IO.FCREATE(path)
  IO.FWRITE(h, "riga1" + CHR$(10) + "riga2" + CHR$(10))
  IO.FCLOSE(h)
  ls$ = IO.READLINES(path)

  PRINT "a="; UBOUND(a$) + 1; " b="; UBOUND(b$) + 1; " lines="; UBOUND(ls$) + 1; " first="; ls$(0); " scalari="; n + m
END SUB
