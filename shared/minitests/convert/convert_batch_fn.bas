' MiniTest: funzione CONVERT.BATCH(dir, fmt, rec) — forma qualificata con parentesi.
' Contratto canonico = gmb_doc_batch: concatena con separatore "--- FILE: nome ---".
' Verifica separatore + conversione CSV (header tabella "| a b |").
' EXPECT: batch-ok
IMPORT IO
IMPORT CONVERT

SUB Main()
  ON ERROR RESUME NEXT
  KILL "bin/bf_in/a.md"
  MKDIR "bin"
  MKDIR "bin/bf_in"
  ON ERROR GOTO 0
  DIM h AS LONG
  h = IO.FCREATE("bin/bf_in/a.csv")
  IO.FWRITE(h, "a,b" + CHR(10) + "1,2" + CHR(10))
  IO.FCLOSE(h)
  DIM r$ = CONVERT.BATCH("bin/bf_in", "md", 0)
  IF INSTR(r$, "a.csv") > 0 AND INSTR(r$, "| a b |") > 0 THEN PRINT "batch-ok" ELSE PRINT "batch-ko"
END SUB
