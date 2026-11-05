' ==============================================================================
' minitests_work_absolute.bas — rende assoluti i path `work/` dei MiniTest.
'
' PERCHE'
' "work/..." e' relativa alla CWD del processo. Gli harness impostano la CWD sulla
' cartella del .bas, quindi oggi gli artefatti nascono accanto al programma; ma un
' eseguibile lanciato a mano da un'altra cartella creerebbe `work/` li'.
'
' MODO NORMALE (default): riscrive i literal di CODICE in
'     GMBWORK$ = PROJECTFOLDER() + "/work"     (dichiarata in testa al programma)
'     "work/x"  ->  GMBWORK$ + "/x"
'     "work"    ->  GMBWORK$
'
' COSA NON CONVERTE (di proposito, e lo dichiara nel report)
'  1. path dentro una stringa JSON (`""work/...""`): non interpolabili;
'  2. path STAMPATI in output (`PRINT ... "work/..."` o `' EXPECT:` con `work/`):
'     l'output diventerebbe dipendente dalla macchina.
'
' MODO --chdir: ai file lasciati relativi aggiunge `CHDIR PROJECTFOLDER()` dopo
' `SUB Main()` (o dopo l'ultimo IMPORT): i path restano relativi, l'output non
' cambia, ma si risolvono sulla cartella del programma.
'
' USO (dalla root del monorepo, gamba nativa):
'   bash tools/sh/gmb_c99native_run.sh ../tools/basic/minitests_work_absolute.bas mwa
'   bash tools/sh/gmb_c99native_run.sh ../tools/basic/minitests_work_absolute.bas mwa -- --apply
'   bash tools/sh/gmb_c99native_run.sh ../tools/basic/minitests_work_absolute.bas mwa -- --chdir --apply
' ==============================================================================
IMPORT IO
IMPORT SYSTEM

DIM g_ls(256) AS LONG
DIM g_le(256) AS LONG
DIM g_lraw(256) AS STRING
DIM g_nlit AS LONG

DIM g_apply AS LONG
DIM g_chdir AS LONG
DIM g_changed AS LONG
DIM g_skipped AS LONG
DIM g_skipLog AS STRING
DIM g_chgLog AS STRING
DIM g_root AS STRING
DIM g_decl AS STRING

FUNCTION TrimS(s AS STRING) AS STRING
  RETURN RTRIM$(LTRIM$(s))
END FUNCTION

FUNCTION Q(s AS STRING) AS STRING
  RETURN CHR$(34) + s + CHR$(34)
END FUNCTION

FUNCTION IsComment(s AS STRING) AS LONG
  IF LEFT$(LTRIM$(s), 1) = "'" THEN RETURN -1
  RETURN 0
END FUNCTION

FUNCTION VarName() AS STRING
  RETURN "GMBWORK$"
END FUNCTION

SUB ScanLiterals(s AS STRING)
  DIM i AS LONG, n AS LONG, start AS LONG, raw AS STRING
  g_nlit = 0
  i = 1
  n = LEN(s)
  DO WHILE i <= n
    IF MID$(s, i, 1) <> CHR$(34) THEN
      i = i + 1
    ELSE
      start = i
      i = i + 1
      DO WHILE i <= n
        IF MID$(s, i, 1) = CHR$(34) THEN
          IF MID$(s, i + 1, 1) = CHR$(34) THEN
            i = i + 2
          ELSE
            i = i + 1
            EXIT DO
          END IF
        ELSE
          i = i + 1
        END IF
      LOOP
      IF g_nlit < 256 THEN
        g_ls(g_nlit) = start
        g_le(g_nlit) = i
        g_lraw(g_nlit) = MID$(s, start + 1, i - start - 2)
        g_nlit = g_nlit + 1
      END IF
    END IF
  LOOP
END SUB

' Riscrive i literal `work`/`work/...` di una riga. Ritorna (nuova, n).
FUNCTION RewriteLine(s AS STRING, BYREF hits AS LONG) AS STRING
  DIM out AS STRING, last AS LONG, i AS LONG, raw AS STRING, rest AS STRING, repl AS STRING
  hits = 0
  IF IsComment(s) <> 0 THEN RETURN s
  ScanLiterals(s)
  out = ""
  last = 1
  FOR i = 0 TO g_nlit - 1
    raw = g_lraw(i)
    repl = ""
    IF raw = "work" THEN
      repl = VarName()
    ELSEIF LEFT$(raw, 5) = "work/" THEN
      rest = MID$(raw, 6)
      IF LEN(rest) = 0 THEN
        repl = VarName() + " + " + Q("/")
      ELSE
        repl = VarName() + " + " + Q("/" + rest)
      END IF
    END IF
    IF LEN(repl) > 0 THEN
      out = out + MID$(s, last, g_ls(i) - last) + repl
      last = g_le(i)
      hits = hits + 1
    END IF
  NEXT i
  out = out + MID$(s, last)
  RETURN out
END FUNCTION

FUNCTION CountLines(ln$()) AS LONG
  IF UBOUND(ln$) < 0 THEN RETURN 0
  RETURN UBOUND(ln$) + 1
END FUNCTION

FUNCTION LineHas(ln$(), key AS STRING) AS LONG
  DIM i AS LONG
  FOR i = 0 TO UBOUND(ln$)
    IF INSTR(ln$(i), key) > 0 THEN RETURN -1
  NEXT i
  RETURN 0
END FUNCTION

FUNCTION MainAnchor(ln$()) AS LONG
  DIM i AS LONG, s AS STRING, mainAt AS LONG, lastImp AS LONG
  mainAt = -1
  lastImp = -1
  FOR i = 0 TO UBOUND(ln$)
    s = TrimS(ln$(i))
    IF mainAt < 0 AND LEFT$(UCASE$(s), 8) = "SUB MAIN" THEN mainAt = i
    IF LEFT$(UCASE$(s), 7) = "IMPORT " THEN lastImp = i
  NEXT i
  IF mainAt >= 0 THEN RETURN mainAt + 1
  IF lastImp >= 0 THEN RETURN lastImp + 1
  RETURN 0
END FUNCTION

SUB NoteSkip(rel AS STRING, why AS STRING)
  g_skipped = g_skipped + 1
  g_skipLog = g_skipLog + "  - " + rel + "  " + why + CHR$(10)
END SUB

' ---- pianificazione di un file ------------------------------------------------
' Esito in g_planOut / g_planHits / g_planWhy (g_planWhy="" => ok)
DIM g_planOut AS STRING
DIM g_planHits AS LONG
DIM g_planWhy AS STRING

SUB PlanFile(full AS STRING)
  DIM t AS STRING, ln$() AS STRING, i AS LONG, k AS LONG
  DIM newLine AS STRING, hits AS LONG, total AS LONG, at AS LONG
  DIM line AS STRING, out2 AS STRING
  g_planOut = ""
  g_planHits = 0
  g_planWhy = ""
  t = IO.READFILE(full)

  IF g_chdir <> 0 THEN
    IF INSTR(t, "CHDIR PROJECTFOLDER()") > 0 THEN
      g_planWhy = "CHDIR PROJECTFOLDER() gia' presente"
      EXIT SUB
    END IF
    IF INSTR(t, VarName()) > 0 THEN
      g_planWhy = "gia' convertito a " + VarName()
      EXIT SUB
    END IF
    ' serve almeno un literal work/ in codice
    ln$ = SPLIT(t, CHR$(10))
    k = 0
    FOR i = 0 TO UBOUND(ln$)
      line = ln$(i)
      IF IsComment(line) = 0 AND INSTR(line, CHR$(34) + "work") > 0 THEN k = k + 1
    NEXT i
    IF k = 0 THEN
      g_planWhy = "nessun path work/ relativo nel codice"
      EXIT SUB
    END IF
    at = MainAnchor(ln$())
    g_planOut = ""
    FOR i = 0 TO UBOUND(ln$)
      IF i = at THEN g_planOut = g_planOut + "  CHDIR PROJECTFOLDER()" + CHR$(10)
      g_planOut = g_planOut + ln$(i) + CHR$(10)
    NEXT i
    g_planHits = 1
    EXIT SUB
  END IF

  ' modo normale: esclusioni
  ln$ = SPLIT(t, CHR$(10))
  FOR i = 0 TO UBOUND(ln$)
    line = ln$(i)
    IF LEFT$(line, 9) = "' EXPECT:" AND INSTR(line, "work/") > 0 THEN
      g_planWhy = "EXPECT stampa work/ (riga " + LTRIM$(STR$(i + 1)) + ")"
      EXIT SUB
    END IF
  NEXT i
  FOR i = 0 TO UBOUND(ln$)
    line = ln$(i)
    IF IsComment(line) = 0 AND INSTR(line, "PRINT") > 0 AND INSTR(line, CHR$(34) + "work") > 0 THEN
      g_planWhy = "PRINT di un path work/ (riga " + LTRIM$(STR$(i + 1)) + ")"
      EXIT SUB
    END IF
  NEXT i
  FOR i = 0 TO UBOUND(ln$)
    IF INSTR(ln$(i), CHR$(34) + CHR$(34) + "work") > 0 THEN
      g_planWhy = "path dentro una stringa JSON (riga " + LTRIM$(STR$(i + 1)) + ")"
      EXIT SUB
    END IF
  NEXT i
  IF INSTR(t, VarName()) > 0 THEN
    g_planWhy = "la variabile " + VarName() + " e' gia' presente"
    EXIT SUB
  END IF

  total = 0
  FOR i = 0 TO UBOUND(ln$)
    newLine = RewriteLine(ln$(i), hits)
    total = total + hits
  NEXT i
  IF total = 0 THEN
    g_planWhy = "nessun literal da convertire"
    EXIT SUB
  END IF

  at = MainAnchor(ln$())
  g_planOut = ""
  FOR i = 0 TO UBOUND(ln$)
    newLine = RewriteLine(ln$(i), hits)
    g_planOut = g_planOut + newLine + CHR$(10)
  NEXT i
  ' inserisce la dichiarazione DOPO la riga di anchor (ricostruita)

  outLn$ = SPLIT(g_planOut, CHR$(10))
  out2 = ""
  FOR i = 0 TO UBOUND(outLn$)
    IF i = at THEN out2 = out2 + g_decl + CHR$(10)
    IF i < UBOUND(outLn$) THEN out2 = out2 + outLn$(i) + CHR$(10)
  NEXT i
  g_planOut = out2
  g_planHits = total
END SUB

SUB WriteFile(full AS STRING, body AS STRING, backupDir AS STRING, rel AS STRING)
  DIM h AS LONG, wr AS LONG
  wr = SYSTEM.SHELLOUT("mkdir -p " + Q(backupDir) + " && cp " + Q(full) + " " + Q(backupDir + "/" + rel))
  wr = SYSTEM.SHELLOUT("rm -f " + Q(full))
  h = IO.FCREATE(full)
  wr = IO.FWRITE(h, body)
  IO.FCLOSE(h)
END SUB

FUNCTION Main() AS INTEGER
  DIM a AS STRING, i AS LONG, t AS STRING, ln$() AS STRING
  DIM full AS STRING, rel AS STRING, stamp AS STRING, backupDir AS STRING

  SET SHELL ON
  g_apply = 0
  g_chdir = 0
  i = 1
  DO WHILE i <= COMMANDCOUNT()
    a = COMMAND$(i)
    IF a = "--apply" THEN g_apply = -1
    IF a = "--chdir" THEN g_chdir = -1
    IF a = "--root" AND i < COMMANDCOUNT() THEN
      i = i + 1
      g_root = COMMAND$(i)
    END IF
    i = i + 1
  LOOP

  IF LEN(g_root) = 0 THEN
    IF DIREXISTS(IO.PARENTDIR(IO.PARENTDIR(IO.CWD())) + "/shared/minitests") THEN
      g_root = IO.PARENTDIR(IO.PARENTDIR(IO.CWD())) + "/shared/minitests"
    ELSE
      g_root = "shared/minitests"
    END IF
  END IF
  IF NOT DIREXISTS(g_root) THEN
    PRINT "cartella minitest non trovata: "; g_root
    RETURN 1
  END IF

  g_decl = "  DIM " + VarName() + " AS STRING = PROJECTFOLDER() + " + Q("/work")

  t = SYSTEM.SHELLOUT("find " + Q(g_root) + " -name '*.bas' 2>/dev/null | sort")
  ln$ = SPLIT(t, CHR$(10))
  g_changed = 0
  g_skipped = 0
  g_skipLog = ""
  g_chgLog = ""
  IF UBOUND(ln$) >= 0 THEN
    FOR i = 0 TO UBOUND(ln$)
      full = TrimS(ln$(i))
      IF LEN(full) > 0 THEN
        t = IO.READFILE(full)
        IF INSTR(t, CHR$(34) + "work") > 0 THEN
          rel = full
          IF LEFT$(rel, LEN(g_root) + 1) = g_root + "/" THEN rel = MID$(rel, LEN(g_root) + 2)
          PlanFile(full)
          IF LEN(g_planWhy) > 0 THEN
            NoteSkip(rel, g_planWhy)
          ELSE
            g_changed = g_changed + 1
            g_chgLog = g_chgLog + "  + " + rel + "  (" + LTRIM$(STR$(g_planHits)) + ")" + CHR$(10)
            IF g_apply <> 0 THEN
              stamp = SYSTEM.SHELLOUT("date +%Y%m%d-%H%M%S")
              backupDir = "/tmp/gmb-backup-" + LTRIM$(stamp)
              WriteFile(full, g_planOut, backupDir, rel)
            END IF
          END IF
        END IF
      END IF
    NEXT i
  END IF

  PRINT "file da modificare: "; g_changed
  PRINT "file lasciati relativi: "; g_skipped
  IF LEN(g_skipLog) > 0 THEN PRINT g_skipLog;
  IF LEN(g_chgLog) > 0 THEN PRINT g_chgLog;
  IF g_apply = 0 THEN
    PRINT ""
    PRINT "(dry-run: rilancia con --apply per scrivere)"
    RETURN 0
  END IF
  PRINT ""
  PRINT "scritti "; g_changed; " file; backup in /tmp/gmb-backup-<stamp>"
  RETURN 0
END FUNCTION
