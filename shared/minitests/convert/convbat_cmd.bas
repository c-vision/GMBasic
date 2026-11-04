' MiniTest: comando CONVBAT (forma comando, senza parentesi — vedi AGENTS.md).
' Contratto canonico = gmb_doc_batch: concatena con separatore "--- FILE: nome ---".
' CONVBAT dir, fmt, rec INTO var$ su cartella locale (un .csv → header tabella in r$).
' EXPECT: convbat-ok
IMPORT IO
IMPORT CONVERT

SUB Main()
  ON ERROR RESUME NEXT
  KILL "work/cb_in/a.md"
  MKDIR "work"
  MKDIR "work/cb_in"
  ON ERROR GOTO 0
  DIM h AS LONG
  h = IO.FCREATE("work/cb_in/a.csv")
  IO.FWRITE(h, "a,b" + CHR(10) + "1,2" + CHR(10))
  IO.FCLOSE(h)
  CONVBAT "work/cb_in", "md", 0 INTO r$
  IF INSTR(r$, "a.csv") > 0 AND INSTR(r$, "| a b |") > 0 THEN PRINT "convbat-ok" ELSE PRINT "convbat-ko"
END SUB
