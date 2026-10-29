' audit_gmb_env.bas — audit delle variabili d'ambiente GMB_* (niente Python).
' Per ogni nome: classe (funzionamento/diagnostica), numero letture e primi 8 lettori.
' Output: docs/gmb_env_audit.tsv + riepilogo. Uso (da GMBasic/): eseguire tools/basic/audit_gmb_env.bas
IMPORT IO
IMPORT GREP

DIM g_names(5000) AS STRING
DIM g_counts(5000) AS LONG
DIM g_locs(5000) AS STRING
DIM g_n AS LONG

FUNCTION IsDebug(name AS STRING) AS INTEGER
  DIM u AS STRING
  DIM pats(11) AS STRING
  DIM i AS INTEGER
  u = UPPER(name)
  pats(0) = "DBG" : pats(1) = "DEBUG" : pats(2) = "DUMP" : pats(3) = "TRACE"
  pats(4) = "_LOG" : pats(5) = "TIME" : pats(6) = "STATS" : pats(7) = "VERBOSE"
  pats(8) = "PROBE" : pats(9) = "DIAG" : pats(10) = "AUDIT"
  FOR i = 0 TO 10
    IF INSTR(u, pats(i)) > 0 THEN
      RETURN 1
    END IF
  NEXT i
  IF INSTR(u, "_TEST") > 0 OR INSTR(u, "MINI_ONLY") > 0 OR INSTR(u, "_CHECK") > 0 THEN
    RETURN 1
  END IF
  IF INSTR(u, "_INFO") > 0 OR INSTR(u, "SIGNPOST") > 0 THEN
    RETURN 1
  END IF
  RETURN 0
END FUNCTION

SUB AddHit(name AS STRING, loc AS STRING)
  DIM i AS LONG
  DIM found AS INTEGER
  found = 0
  FOR i = 0 TO g_n - 1
    IF g_names(i) = name THEN
      IF g_counts(i) < 8 THEN
        IF LEN(g_locs(i)) > 0 THEN g_locs(i) = g_locs(i) + ","
        g_locs(i) = g_locs(i) + loc
      END IF
      g_counts(i) = g_counts(i) + 1
      found = 1
      EXIT FOR
    END IF
  NEXT i
  IF found = 0 THEN
    g_names(g_n) = name
    g_counts(g_n) = 1
    g_locs(g_n) = loc
    g_n = g_n + 1
  END IF
END SUB

FUNCTION ValidExt(path AS STRING) AS INTEGER
  DIM u AS STRING
  u = UPPER(path)
  IF RIGHT$(u, 2) = ".C" OR RIGHT$(u, 2) = ".H" OR RIGHT$(u, 3) = ".CS" THEN
    RETURN 1
  END IF
  IF RIGHT$(u, 3) = ".CC" OR RIGHT$(u, 4) = ".CPP" OR RIGHT$(u, 2) = ".M" THEN
    RETURN 1
  END IF
  IF RIGHT$(u, 3) = ".MM" OR RIGHT$(u, 4) = ".HPP" THEN
    RETURN 1
  END IF
  RETURN 0
END FUNCTION

FUNCTION Main() AS INTEGER
  DIM root AS STRING
  DIM broot AS STRING
  DIM roots(2) AS STRING
  DIM nroots AS LONG
  DIM ri AS LONG
  DIM tsv AS STRING
  DIM files$()
  DIM fi AS LONG
  DIM nf AS LONG
  DIM path AS STRING
  DIM rel AS STRING
  DIM ln AS STRING
  DIM u AS STRING
  DIM name AS STRING
  DIM word AS STRING
  DIM loc AS STRING
  DIM c AS STRING
  DIM st AS LONG
  DIM p AS LONG
  DIM q AS LONG
  DIM qs AS LONG
  DIM we AS LONG
  DIM lineno AS LONG
  DIM nwin AS LONG
  DIM ndbg AS LONG
  DIM nlines AS LONG
  DIM i AS LONG
  DIM j AS LONG
  DIM k AS LONG
  DIM best AS LONG
  DIM besti AS LONG
  DIM bestn AS STRING
  DIM used(5000) AS INTEGER
  DIM tmpn AS STRING
  DIM tmpc AS LONG
  DIM tmpl AS STRING
  DIM fh AS INTEGER

  root = IO.PARENTDIR(IO.CWD())
  broot = root
  nroots = 1
  roots(0) = root
  IF IO.DIREXISTS(IO.PARENTDIR(root) + "/GMBasicSharp") THEN
    broot = IO.PARENTDIR(root)
    nroots = 2
    roots(1) = broot + "/GMBasicSharp"
  END IF
  tsv = broot + "/docs/gmb_env_audit.tsv"
  IF NOT IO.DIREXISTS(broot + "/docs") THEN MKDIR broot + "/docs"
  g_n = 0
  FOR ri = 0 TO nroots - 1
  files$ = GREP.FILES("GMB_", roots(ri))
  nf = UBOUND(files$) + 1
  FOR fi = 0 TO nf - 1
    path = files$(fi)
    IF INSTR(path, "/.git/") = 0 AND INSTR(path, "/build/") = 0 AND INSTR(path, "/_deps/") = 0 AND INSTR(path, "/node_modules/") = 0 AND INSTR(path, "/migration/") = 0 THEN
      IF ValidExt(path) THEN
        rel = MID$(path, LEN(broot) + 2)
        OPEN path FOR INPUT AS #1
        lineno = 0
        DO UNTIL EOF(1)
          LINE INPUT #1, ln
          lineno = lineno + 1
          st = INSTR(ln, "GMB_")
          DO WHILE st > 0
            qs = st - 1
            IF qs >= 1 AND MID$(ln, qs, 1) = CHR$(34) THEN
              p = st + 4
              name = "GMB_"
              DO WHILE p <= LEN(ln)
                c = MID$(ln, p, 1)
                IF (c >= "A" AND c <= "Z") OR (c >= "0" AND c <= "9") OR c = "_" THEN
                  name = name + c
                  p = p + 1
                ELSE
                  EXIT DO
                END IF
              LOOP
              IF LEN(name) > 4 THEN
                q = qs - 1
                DO WHILE q > 0 AND MID$(ln, q, 1) = " " : q = q - 1 : LOOP
                IF q > 0 AND MID$(ln, q, 1) = "(" THEN
                  q = q - 1
                  DO WHILE q > 0 AND MID$(ln, q, 1) = " " : q = q - 1 : LOOP
                  we = q
                  DO WHILE q > 0
                    c = MID$(ln, q, 1)
                    IF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR (c >= "0" AND c <= "9") OR c = "_" THEN
                      q = q - 1
                    ELSE
                      EXIT DO
                    END IF
                  LOOP
                  word = MID$(ln, q + 1, we - q)
                  IF RIGHT$(word, 6) = "getenv" OR RIGHT$(word, 22) = "GetEnvironmentVariable" THEN
                    AddHit(name, rel + ":" + LTRIM$(STR$(lineno)))
                  ELSEIF LEFT$(word, 3) = "Env" OR LEFT$(word, 6) = "GmbEnv" OR LEFT$(word, 10) = "GmbasicEnv" THEN
                    AddHit(name, rel + ":" + LTRIM$(STR$(lineno)))
                  END IF
                END IF
              END IF
            END IF
            st = INSTR(st + 4, ln, "GMB_")
          LOOP
        LOOP
        CLOSE #1
      END IF
    END IF
  NEXT fi
  NEXT ri

  ' ordinamento alfabetico per nome (selection sort)
  FOR i = 0 TO g_n - 1
    besti = i
    FOR j = i + 1 TO g_n - 1
      IF g_names(j) < g_names(besti) THEN besti = j
    NEXT j
    IF besti <> i THEN
      tmpn = g_names(i) : g_names(i) = g_names(besti) : g_names(besti) = tmpn
      tmpc = g_counts(i) : g_counts(i) = g_counts(besti) : g_counts(besti) = tmpc
      tmpl = g_locs(i) : g_locs(i) = g_locs(besti) : g_locs(besti) = tmpl
    END IF
  NEXT i

  nwin = 0 : ndbg = 0
  OPEN tsv FOR OUTPUT AS #1
  PRINT #1, "nome" + CHR$(9) + "classe" + CHR$(9) + "n_letture" + CHR$(9) + "lettori(file:line,...)"
  FOR i = 0 TO g_n - 1
    IF IsDebug(g_names(i)) THEN
      ndbg = ndbg + 1
      name = "diagnostica"
    ELSE
      nwin = nwin + 1
      name = "funzionamento"
    END IF
    loc = g_locs(i)
    IF g_counts(i) > 8 THEN loc = loc + ",…(+" + LTRIM$(STR$(g_counts(i) - 8)) + ")"
    PRINT #1, g_names(i) + CHR$(9) + name + CHR$(9) + LTRIM$(STR$(g_counts(i))) + CHR$(9) + loc
  NEXT i
  CLOSE #1

  PRINT "env GMB_* distinte: "; g_n; "  | funzionamento: "; nwin; "  | diagnostica: "; ndbg
  PRINT "TSV: "; tsv
  PRINT ""
  PRINT "Funzionamento più letti:"
  FOR k = 0 TO g_n - 1 : used(k) = 0 : NEXT k
  FOR j = 1 TO 20
    best = -1 : besti = -1 : bestn = ""
    FOR i = 0 TO g_n - 1
      IF used(i) = 0 AND NOT IsDebug(g_names(i)) THEN
        IF g_counts(i) > best OR (g_counts(i) = best AND (besti < 0 OR g_names(i) > bestn)) THEN
          best = g_counts(i) : besti = i : bestn = g_names(i)
        END IF
      END IF
    NEXT i
    IF besti >= 0 THEN
      used(besti) = 1
      PRINT "  "; RIGHT$("    " + LTRIM$(STR$(best)), 4); "  "; g_names(besti)
    END IF
  NEXT j
  RETURN 0
END FUNCTION
