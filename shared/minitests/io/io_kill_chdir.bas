' MiniTest: comandi IO KILL (cancella file) e IO CHDIR (cambia directory).
' Sul VM/C erano instradati al modulo console (CHDIR stampava escape, KILL non cancellava):
' ora vanno a vm_sys_io_chdir_kill (gmb_fs_chdir / gmb_file_kill, canone PCode
' Directory.SetCurrentDirectory / File.Delete).
' EXPECT: created=-1|killed= 0|chdir=ok
IMPORT IO

SUB Main()
  DIM h AS LONG
  h = IO.FCREATE("probe_io_kill.tmp")
  IO.FCLOSE(h)
  PRINT "created="; IO.FILEEXISTS("probe_io_kill.tmp")
  KILL "probe_io_kill.tmp"
  PRINT "killed="; IO.FILEEXISTS("probe_io_kill.tmp")
  DIM d AS STRING
  d = IO.CWD()
  CHDIR(d)
  PRINT "chdir=ok"
END SUB
