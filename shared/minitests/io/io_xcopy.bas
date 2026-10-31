' MiniTest: XCOPY nudo (gruppo IO, parità 4 backend).
' XCOPY era stub sulla VM/C (stesso handler di MOVE) e sul C99Native forzava i flag
' a recursive=1, overwrite=1 (2026-09-21: gmb_fo_30 + flag veri file_pos/cond_mutex).
' Senza flag: copia i file, salta le sottodirectory. Con TRUE, TRUE: anche quelle.
' Path assoluti /tmp (la CWD differisce tra le gambe). FILEEXISTS: -1 = esiste.
' EXPECT: fa=-1|fb=-1|fc= 0|ra=-1|rb=-1|rc=-1
IMPORT IO

SUB Main()
  MKDIR "/tmp/gmb_mt_xc_src"
  MKDIR "/tmp/gmb_mt_xc_src/sub"
  DIM h1 AS LONG
  h1 = IO.FCREATE("/tmp/gmb_mt_xc_src/a.txt")
  IO.FWRITE(h1, "a")
  IO.FCLOSE(h1)
  DIM h2 AS LONG
  h2 = IO.FCREATE("/tmp/gmb_mt_xc_src/b.txt")
  IO.FWRITE(h2, "b")
  IO.FCLOSE(h2)
  DIM h3 AS LONG
  h3 = IO.FCREATE("/tmp/gmb_mt_xc_src/sub/c.txt")
  IO.FWRITE(h3, "c")
  IO.FCLOSE(h3)
  XCOPY "/tmp/gmb_mt_xc_src" TO "/tmp/gmb_mt_xc_flat"
  PRINT "fa="; IO.FILEEXISTS("/tmp/gmb_mt_xc_flat/a.txt")
  PRINT "fb="; IO.FILEEXISTS("/tmp/gmb_mt_xc_flat/b.txt")
  PRINT "fc="; IO.FILEEXISTS("/tmp/gmb_mt_xc_flat/sub/c.txt")
  XCOPY "/tmp/gmb_mt_xc_src" TO "/tmp/gmb_mt_xc_rec", TRUE, TRUE
  PRINT "ra="; IO.FILEEXISTS("/tmp/gmb_mt_xc_rec/a.txt")
  PRINT "rb="; IO.FILEEXISTS("/tmp/gmb_mt_xc_rec/b.txt")
  PRINT "rc="; IO.FILEEXISTS("/tmp/gmb_mt_xc_rec/sub/c.txt")
  KILL "/tmp/gmb_mt_xc_src/a.txt"
  KILL "/tmp/gmb_mt_xc_src/b.txt"
  KILL "/tmp/gmb_mt_xc_src/sub/c.txt"
  KILL "/tmp/gmb_mt_xc_flat/a.txt"
  KILL "/tmp/gmb_mt_xc_flat/b.txt"
  KILL "/tmp/gmb_mt_xc_rec/a.txt"
  KILL "/tmp/gmb_mt_xc_rec/b.txt"
  KILL "/tmp/gmb_mt_xc_rec/sub/c.txt"
  RMDIR "/tmp/gmb_mt_xc_src/sub"
  RMDIR "/tmp/gmb_mt_xc_src"
  RMDIR "/tmp/gmb_mt_xc_flat"
  RMDIR "/tmp/gmb_mt_xc_rec/sub"
  RMDIR "/tmp/gmb_mt_xc_rec"
END SUB
