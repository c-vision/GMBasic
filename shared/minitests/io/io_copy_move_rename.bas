' MiniTest: COPY, MOVE, RENAME in forma nuda (gruppo IO, parità 4 backend).
' MOVE era stub sulla VM/C (scartava gli argomenti) e assente su C99Native
' (2026-09-21: runtime condiviso gmb_file_move + emettitori). COPY/RENAME già reali 5/5.
' Path assoluti /tmp (la CWD differisce tra le gambe). FILEEXISTS: -1 = esiste (Bit).
' EXPECT: copied=-1|moved=-1|renamed=-1|clean= 0
IMPORT IO

SUB Main()
  DIM h AS LONG
  h = IO.FCREATE("/tmp/gmb_mt_cmr_src.txt")
  IO.FWRITE(h, "cmr")
  IO.FCLOSE(h)
  COPY "/tmp/gmb_mt_cmr_src.txt" TO "/tmp/gmb_mt_cmr_cpy.txt"
  PRINT "copied="; IO.FILEEXISTS("/tmp/gmb_mt_cmr_cpy.txt")
  MOVE "/tmp/gmb_mt_cmr_cpy.txt" TO "/tmp/gmb_mt_cmr_mov.txt"
  PRINT "moved="; IO.FILEEXISTS("/tmp/gmb_mt_cmr_mov.txt")
  RENAME "/tmp/gmb_mt_cmr_mov.txt" AS "/tmp/gmb_mt_cmr_ren.txt"
  PRINT "renamed="; IO.FILEEXISTS("/tmp/gmb_mt_cmr_ren.txt")
  KILL "/tmp/gmb_mt_cmr_src.txt"
  KILL "/tmp/gmb_mt_cmr_ren.txt"
  PRINT "clean="; IO.FILEEXISTS("/tmp/gmb_mt_cmr_ren.txt")
END SUB
