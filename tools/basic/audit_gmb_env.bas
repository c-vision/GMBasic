' audit_gmb_env.bas — audit delle variabili d'ambiente (niente Python).
' Copre GMB_*, GMBASIC_*, LLM*, IMGDIFF_*, GAI_*, ISAM_* e le variabili di SISTEMA
' (HOME, TMPDIR, PATH, ...): queste ultime finiscono nella classe "sistema", separata,
' perché non sono parametri del motore e non ricadono nella regola che vieta le env.
' Per ogni nome: classe (funzionamento/diagnostica/sistema), numero letture e primi 8 lettori.
' Inoltre separa le letture gia' passate dal ponte sancionato registro->env (`gmb_opt_env`,
' `GmbEnv.Get`) da quelle ancora su `getenv`/`GetEnvironmentVariable` diretti: e' la misura
' di quanto manca alla migrazione (colonne ponte/diretto/via).
' Output: tools/reports/gmb_env_audit.tsv + riepilogo. Uso (da GMBasic/): eseguire tools/basic/audit_gmb_env.bas
IMPORT IO
IMPORT GREP

DIM g_names(5000) AS STRING
DIM g_counts(5000) AS LONG
DIM g_locs(5000) AS STRING
DIM g_ponte(5000) AS LONG
DIM g_diretto(5000) AS LONG
DIM g_n AS LONG

FUNCTION IsDebug(name AS STRING) AS INTEGER
  DIM u AS STRING
  DIM pats(11) AS STRING
  DIM i AS INTEGER
  u = UPPER(name)
  pats(0) = "DBG" : pats(1) = "DEBUG" : pats(2) = "DUMP" : pats(3) = "TRACE"
  pats(4) = "_LOG" : pats(5) = "TIME" : pats(6) = "STATS" : pats(7) = "VERBOSE"
  pats(8) = "PROBE" : pats(9) = "DIAG" : pats(10) = "AUDIT"
  ' TIME è nel pattern per i timer di debug, ma un TIMEOUT è un parametro di funzionamento.
  IF INSTR(u, "TIMEOUT") > 0 THEN
    RETURN 0
  END IF
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

' Nomi delle variabili standard di sistema/terminale/terze parti: fuori perimetro.
FUNCTION SysCount() AS LONG
  RETURN 14
END FUNCTION

FUNCTION SysName(i AS LONG) AS STRING
  IF i = 0 THEN RETURN "HOME"
  IF i = 1 THEN RETURN "TMPDIR"
  IF i = 2 THEN RETURN "TEMP"
  IF i = 3 THEN RETURN "PATH"
  IF i = 4 THEN RETURN "TERM"
  IF i = 5 THEN RETURN "LINES"
  IF i = 6 THEN RETURN "COLUMNS"
  IF i = 7 THEN RETURN "NO_COLOR"
  IF i = 8 THEN RETURN "COLORTERM"
  IF i = 9 THEN RETURN "FORCE_COLOR"
  IF i = 10 THEN RETURN "SDL_VIDEODRIVER"
  IF i = 11 THEN RETURN "DYLD_LIBRARY_PATH"
  IF i = 12 THEN RETURN "HF_TOKEN"
  RETURN "HUGGINGFACE_TOKEN"
END FUNCTION

' La CWD non è affidabile (il runner la sposta nella cartella del sorgente): risalgo fino
' alla root del monorepo, riconosciuta dalla presenza di GMBasic/ e GMBasicSharp/.
FUNCTION FindRoot() AS STRING
  DIM d AS STRING
  DIM i AS LONG
  d = IO.CWD()
  FOR i = 0 TO 5
    IF IO.DIREXISTS(d + "/GMBasic") AND IO.DIREXISTS(d + "/GMBasicSharp") THEN
      RETURN d
    END IF
    d = IO.PARENTDIR(d)
  NEXT i
  RETURN ""
END FUNCTION

FUNCTION IsSystem(name AS STRING) AS INTEGER
  DIM u AS STRING
  DIM i AS LONG
  u = UPPER(name)
  FOR i = 0 TO SysCount() - 1
    IF u = SysName(i) THEN
      RETURN 1
    END IF
  NEXT i
  IF LEFT$(u, 3) = "LC_" OR LEFT$(u, 5) = "GREP_" OR LEFT$(u, 4) = "PCRE" THEN
    RETURN 1
  END IF
  RETURN 0
END FUNCTION

SUB AddHit(name AS STRING, loc AS STRING, ponte AS INTEGER)
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
      IF ponte = 1 THEN
        g_ponte(i) = g_ponte(i) + 1
      ELSE
        g_diretto(i) = g_diretto(i) + 1
      END IF
      found = 1
      EXIT FOR
    END IF
  NEXT i
  IF found = 0 THEN
    g_names(g_n) = name
    g_counts(g_n) = 1
    g_locs(g_n) = loc
    IF ponte = 1 THEN
      g_ponte(g_n) = 1
    ELSE
      g_diretto(g_n) = 1
    END IF
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
  DIM roots(3) AS STRING
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
  DIM nsys AS LONG
  DIM pat AS STRING
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
  DIM srcmode AS STRING
  DIM tp AS LONG
  DIM td AS LONG
  DIM np AS LONG
  DIM nd AS LONG

  broot = FindRoot()
  IF LEN(broot) = 0 THEN
    broot = IO.PARENTDIR(IO.CWD())
  END IF
  root = broot
  ' Solo codice: il C, il C# e gli harness/tool in tools/.
  roots(0) = broot + "/GMBasic"
  roots(1) = broot + "/GMBasicSharp"
  roots(2) = broot + "/tools"
  nroots = 3
  tsv = broot + "/tools/reports/gmb_env_audit.tsv"
  IF NOT IO.DIREXISTS(broot + "/tools/reports") THEN MKDIR broot + "/tools/reports"
  g_n = 0
  ' Prefiltro: si aprono solo i file che contengono una delle chiamate di lettura. Così
  ' la copertura non dipende più dai prefissi dei nomi, e la scansione resta veloce.
  ' Niente parentesi nel pattern: nel codice esiste anche la forma `getenv ("...")`.
  pat = "getenv|GetEnvironmentVariable|GmbEnv[.]Get|gmb_opt_env"
  FOR ri = 0 TO nroots - 1
  files$ = GREP.FILES(pat, roots(ri))
  nf = UBOUND(files$) + 1
  FOR fi = 0 TO nf - 1
    path = files$(fi)
    IF INSTR(path, "/.git/") = 0 AND INSTR(path, "/build/") = 0 AND INSTR(path, "/_deps/") = 0 AND INSTR(path, "/node_modules/") = 0 AND INSTR(path, "/migration/") = 0 AND INSTR(path, "/out/") = 0 AND INSTR(path, "/bin/") = 0 THEN
      IF ValidExt(path) THEN
        rel = MID$(path, LEN(broot) + 2)
        OPEN path FOR INPUT AS #1
        lineno = 0
        DO UNTIL EOF(1)
          LINE INPUT #1, ln
          lineno = lineno + 1
          ' Il nome è un identificatore in MAIUSCOLO fra virgolette, subito dopo la parentesi
          ' di una chiamata getenv / GetEnvironmentVariable / <...>GmbEnv.Get. Nessun prefisso
          ' richiesto: la classificazione avviene dopo, per nome.
          qs = INSTR(ln, CHR$(34))
          DO WHILE qs > 0
            p = qs + 1
            name = ""
            DO WHILE p <= LEN(ln)
              c = MID$(ln, p, 1)
              IF (c >= "A" AND c <= "Z") OR (c >= "0" AND c <= "9") OR c = "_" THEN
                name = name + c
                p = p + 1
              ELSE
                EXIT DO
              END IF
            LOOP
            IF LEN(name) > 2 AND p <= LEN(ln) AND MID$(ln, p, 1) = CHR$(34) THEN
              q = qs - 1
              DO WHILE q > 0 AND MID$(ln, q, 1) = " " : q = q - 1 : LOOP
              IF q > 0 AND MID$(ln, q, 1) = "(" THEN
                q = q - 1
                DO WHILE q > 0 AND MID$(ln, q, 1) = " " : q = q - 1 : LOOP
                we = q
                DO WHILE q > 0
                  c = MID$(ln, q, 1)
                  IF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR (c >= "0" AND c <= "9") OR c = "_" OR c = "." THEN
                    q = q - 1
                  ELSE
                    EXIT DO
                  END IF
                LOOP
                word = MID$(ln, q + 1, we - q)
                IF RIGHT$(word, 11) = "gmb_opt_env" THEN
                  AddHit(name, rel + ":" + LTRIM$(STR$(lineno)), 1)
                ELSEIF RIGHT$(word, 6) = "getenv" OR RIGHT$(word, 22) = "GetEnvironmentVariable" THEN
                  AddHit(name, rel + ":" + LTRIM$(STR$(lineno)), 0)
                ELSEIF RIGHT$(word, 4) = ".Get" AND (INSTR(word, "GmbEnv") > 0 OR INSTR(word, "GmbasicEnv") > 0) THEN
                  AddHit(name, rel + ":" + LTRIM$(STR$(lineno)), 1)
                END IF
              END IF
            END IF
            qs = INSTR(qs + 1, ln, CHR$(34))
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
      tmpc = g_ponte(i) : g_ponte(i) = g_ponte(besti) : g_ponte(besti) = tmpc
      tmpc = g_diretto(i) : g_diretto(i) = g_diretto(besti) : g_diretto(besti) = tmpc
    END IF
  NEXT i

  nwin = 0 : ndbg = 0 : nsys = 0
  OPEN tsv FOR OUTPUT AS #1
  PRINT #1, "nome" + CHR$(9) + "classe" + CHR$(9) + "n_letture" + CHR$(9) + "ponte" + CHR$(9) + "diretto" + CHR$(9) + "via" + CHR$(9) + "lettori(file:line,...)"
  FOR i = 0 TO g_n - 1
    IF IsSystem(g_names(i)) THEN
      nsys = nsys + 1
      name = "sistema"
    ELSEIF IsDebug(g_names(i)) THEN
      ndbg = ndbg + 1
      name = "diagnostica"
    ELSE
      nwin = nwin + 1
      name = "funzionamento"
    END IF
    IF g_ponte(i) = 0 THEN
      srcmode = "diretto"
    ELSEIF g_diretto(i) = 0 THEN
      srcmode = "ponte"
    ELSE
      srcmode = "misto"
    END IF
    tp = tp + g_ponte(i)
    td = td + g_diretto(i)
    IF g_diretto(i) = 0 THEN
      np = np + 1
    END IF
    IF g_ponte(i) = 0 THEN
      nd = nd + 1
    END IF
    loc = g_locs(i)
    IF g_counts(i) > 8 THEN loc = loc + ",…(+" + LTRIM$(STR$(g_counts(i) - 8)) + ")"
    PRINT #1, g_names(i) + CHR$(9) + name + CHR$(9) + LTRIM$(STR$(g_counts(i))) + CHR$(9) + LTRIM$(STR$(g_ponte(i))) + CHR$(9) + LTRIM$(STR$(g_diretto(i))) + CHR$(9) + srcmode + CHR$(9) + loc
  NEXT i
  CLOSE #1

  PRINT "root: "; broot
  PRINT "env distinte: "; g_n; "  | funzionamento: "; nwin; "  | diagnostica: "; ndbg; "  | sistema: "; nsys
  PRINT "letture: "; tp + td; "  | dal ponte (gmb_opt_env/GmbEnv.Get): "; tp; " su "; np; " nomi  | ancora dirette: "; td; " su "; nd; " nomi"
  PRINT "TSV: "; tsv
  PRINT ""
  PRINT "Funzionamento più letti:"
  FOR k = 0 TO g_n - 1 : used(k) = 0 : NEXT k
  FOR j = 1 TO 20
    best = -1 : besti = -1 : bestn = ""
    FOR i = 0 TO g_n - 1
      IF used(i) = 0 AND NOT IsDebug(g_names(i)) AND NOT IsSystem(g_names(i)) THEN
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
