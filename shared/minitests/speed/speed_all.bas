' MiniTest NAMESPACE SPEED — tutti i simboli della superficie SPEED.
'
' Superficie (shared/spec/registry/surface.csv): BEGINGROUPCOMMIT, ENDGROUPCOMMIT,
' EOF, FLUSHDURABILITY, INFO, SELECTED, SETDURABILITY, SETMANY, VALIDATE (4 ariità).
' In più i COMANDI del motore (OPEN/USE/PUT/GO/SKIP/CLOSE DATABASES), che il registro
' espone come comandi globali (ns vuoto) ma appartengono al namespace.
'
' Scopo: verificare che i 4 backend supportino la STESSA versione del namespace, non
' un'applicazione. L'oracolo è deterministico (nessuna dipendenza da orologio/modelli).
' EXPECT: info=-1|setdur=Full|flush=-1|begin=-1|end=-1|val0=-1|val1=-1|val2=-1|val3=-1|sel= 0|selx= 0|eof_top= 0|eof_end=-1|setmany= 2
IMPORT IO
IMPORT SPEED

TYPE Riga
  ID AS LONG
  Nome AS STRING * 16
END TYPE

SUB Main()
  ON ERROR RESUME NEXT
  RMDIR "bin/speed_all_db", TRUE
  ON ERROR GOTO 0
  MKDIR "bin"

  SPEED OPEN "bin/speed_all_db" ALIAS V
  SPEED USE "T" FROM V OF Riga ALIAS T INDEXED BY ID

  DIM r AS Riga
  r.ID = 1 : r.Nome = "Alpha" : SPEED PUT T, r
  r.ID = 2 : r.Nome = "Beta" : SPEED PUT T, r

  ' ── ispezione/configurazione ─────────────────────────────────────────────
  PRINT "info="; (LEN(SPEED.INFO()) > 0)
  PRINT "setdur="; SPEED.SETDURABILITY("FULL")
  PRINT "flush="; SPEED.FLUSHDURABILITY()
  PRINT "begin="; SPEED.BEGINGROUPCOMMIT()
  PRINT "end="; SPEED.ENDGROUPCOMMIT()

  ' ── VALIDATE: tutte le ariità dichiarate (0, alias, handle, alias+handle) ──
  PRINT "val0="; SPEED.VALIDATE()
  PRINT "val1="; SPEED.VALIDATE("V")
  PRINT "val2="; SPEED.VALIDATE(-1)
  PRINT "val3="; SPEED.VALIDATE("V", -1)

  ' ── SELECTED: area corrente e alias inesistente (0 = nessuna area DBF/ISAM) ─
  PRINT "sel="; SPEED.SELECTED()
  PRINT "selx="; SPEED.SELECTED("NESSUNA")

  ' ── EOF: cursore SPEED (BIT -1/0) ────────────────────────────────────────
  SPEED GO T, TOP
  PRINT "eof_top="; SPEED.EOF(T)
  DIM i AS INTEGER
  FOR i = 1 TO 3
    SPEED SKIP T
  NEXT i
  PRINT "eof_end="; SPEED.EOF(T)

  ' ── SETMANY: inserimento batch (ritorna il numero inserito) ──────────────
  DIM batch AS LIST OF Riga
  batch = NEW LIST OF Riga {}
  DIM a AS Riga
  a.ID = 3 : a.Nome = "Gamma" : batch.Add(a)
  a.ID = 4 : a.Nome = "Delta" : batch.Add(a)
  PRINT "setmany="; SPEED.SETMANY(T, batch)

  SPEED CLOSE DATABASES
END SUB
