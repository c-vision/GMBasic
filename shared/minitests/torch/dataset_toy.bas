' MiniTest: L6 pipeline dati testuale toy (char-level) — parita' 4 backend.
' TEXTENCODE/DECODE round-trip + DATASETLOAD su fixture temporanea (righe -> tensore).
' EXPECT: enc: 5|round: 1|ds: 3|cut: 2|bad: 1
IMPORT TORCH
IMPORT IO

SUB Main()
  ' encode: "hello" -> tensore [1,5]
  DIM e AS LONG = TORCH.TEXTENCODE("hello")
  DIM sh() = TORCH.SHAPE(e)
  IF sh(0) = 1 AND sh(1) = 5 THEN PRINT "enc: 5" ELSE PRINT "enc: 0"

  ' round-trip testo
  IF TORCH.TEXTDECODE(e) = "hello" THEN PRINT "round: 1" ELSE PRINT "round: 0"

  ' dataset: fixture con 3 righe
  DIM p AS STRING = "/tmp/gmb_l6_ds.txt"
  DIM h AS LONG = IO.FCREATE(p)
  IO.FWRITE(h, "cat" + CHR(10) + "dog" + CHR(10) + "fox" + CHR(10))
  IO.FCLOSE(h)

  DIM ds AS LONG = TORCH.DATASETLOAD(p)
  DIM sh1() = TORCH.SHAPE(ds)
  IF sh1(0) = 3 THEN PRINT "ds: 3" ELSE PRINT "ds: 0"

  ' cutoff_len 2 -> shape [3,2]
  DIM ds2 AS LONG = TORCH.DATASETLOAD(p, "{""cutoff_len"":2}")
  DIM sh2() = TORCH.SHAPE(ds2)
  IF sh2(0) = 3 AND sh2(1) = 2 THEN PRINT "cut: 2" ELSE PRINT "cut: 0"

  ' file inesistente -> handle <= 0 (errore esplicito)
  DIM bad AS LONG = TORCH.DATASETLOAD("/tmp/gmb_l6_non_esiste.txt")
  IF bad <= 0 THEN PRINT "bad: 1" ELSE PRINT "bad: 0"

  TORCH.MODELFREE(e) : TORCH.MODELFREE(ds) : TORCH.MODELFREE(ds2)
  ON ERROR RESUME NEXT
  KILL p
  ON ERROR GOTO 0
END SUB
