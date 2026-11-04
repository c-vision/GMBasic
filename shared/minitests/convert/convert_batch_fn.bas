' MiniTest: funzione CONVERT.BATCH(dir, fmt, rec) — forma qualificata con parentesi.
' Contratto canonico = gmb_doc_batch: concatena con separatore "--- FILE: nome ---".
' Verifica separatore + conversione CSV (header tabella "| a b |").
' EXPECT: batch-ok
IMPORT IO
IMPORT CONVERT

SUB Main()
  DIM GMBWORK$ AS STRING = PROJECTFOLDER() + "/work"
  ON ERROR RESUME NEXT
  KILL GMBWORK$ + "/bf_in/a.md"
  MKDIR GMBWORK$
  MKDIR GMBWORK$ + "/bf_in"
  ON ERROR GOTO 0
  DIM h AS LONG
  h = IO.FCREATE(GMBWORK$ + "/bf_in/a.csv")
  IO.FWRITE(h, "a,b" + CHR(10) + "1,2" + CHR(10))
  IO.FCLOSE(h)
  DIM r$ = CONVERT.BATCH(GMBWORK$ + "/bf_in", "md", 0)
  IF INSTR(r$, "a.csv") > 0 AND INSTR(r$, "| a b |") > 0 THEN PRINT "batch-ok" ELSE PRINT "batch-ko"
END SUB
