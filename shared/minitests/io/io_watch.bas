' MiniTest: WATCH e UNWATCH nudi (gruppo IO, parità 4 backend).
' WATCH/UNWATCH erano assenti sul C99Native (errore esplicito anti-silent);
' 2026-09-21: GMB_STMT_IOWATCH/IOUNWATCH + wrapper callback gmb_iow_<Cb>_entry
' (lo sleep-watcher GPM C spollla già via gmb_iowatch_first_changed_fn).
' Si osserva il FILE (non la directory): il watcher gestito (FileSystemWatcher con
' Filter = nome file) rileva la scrittura; quello C rileva mtime/size via stat.
' La callback imposta un flag globale; il main scrive e attende con SLEEP.
' Path assoluti /tmp (la CWD differisce tra le gambe).
' EXPECT: fired=-1
IMPORT IO

GLOBAL Fired AS LONG

SUB OnWatch()
  Fired = -1
END SUB

SUB Main()
  Fired = 0
  DIM h0 AS LONG
  h0 = IO.FCREATE("/tmp/gmb_mt_watch_evt.txt")
  IO.FWRITE(h0, "init")
  IO.FCLOSE(h0)
  WATCH "/tmp/gmb_mt_watch_evt.txt", %FILE_CHANGED, OnWatch
  SLEEP 200
  DIM h AS LONG
  h = IO.FCREATE("/tmp/gmb_mt_watch_evt.txt")
  IO.FWRITE(h, "evt")
  IO.FCLOSE(h)
  DIM i AS LONG
  FOR i = 1 TO 100
    IF Fired <> 0 THEN EXIT FOR
    SLEEP 50
  NEXT
  PRINT "fired="; Fired
  UNWATCH "/tmp/gmb_mt_watch_evt.txt"
  KILL "/tmp/gmb_mt_watch_evt.txt"
END SUB
