' surface_coverage.bas — copertura MiniTest/sample/guide della superficie canonica
' (da surface_coverage.py). Opzioni: --missing mt|sm|gd, --ns NS, --per-ns,
' --root DIR, --guides DIR, --compare ALTRO_REPO.
IMPORT IO
IMPORT GREP

DIM g_name(3000) AS STRING
DIM g_ns(3000) AS STRING
DIM g_kind(3000) AS STRING
DIM g_seen(3000) AS LONG
DIM g_mask(3000) AS LONG
DIM g_dict AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER
DIM g_n AS INTEGER

FUNCTION FindSurface(root AS STRING) AS STRING
  DIM cand AS STRING, up AS LONG, dirbase AS STRING
  dirbase = root
  FOR up = 1 TO 3
    cand = dirbase + "/shared/spec/registry/surface.csv"
    IF FILEEXISTS(cand) THEN RETURN cand
    cand = dirbase + "/GMBasicSharp/common/Registry/surface.csv"
    IF FILEEXISTS(cand) THEN RETURN cand
    cand = dirbase + "/common/Registry/surface.csv"
    IF FILEEXISTS(cand) THEN RETURN cand
    dirbase = IO.PARENTDIR(dirbase)
  NEXT up
  RETURN ""
END FUNCTION

SUB AddName(kind AS STRING, ns AS STRING, full AS STRING)
  DIM i AS LONG, p AS LONG, b AS STRING
  b = full
  IF LEFT$(b, 1) = "%" THEN b = MID$(b, 2, LEN(b) - 1)
  p = 0
  FOR i = 1 TO LEN(b)
    IF MID$(b, i, 1) = "." THEN p = i
  NEXT i
  IF p > 0 THEN b = MID$(b, p + 1, LEN(b) - p)
  b = UPPER$(b)
  IF b IN g_dict THEN EXIT SUB
  g_n = g_n + 1
  g_name(g_n) = b
  g_ns(g_n) = ns
  g_kind(g_n) = kind
  g_dict(b) = g_n
END SUB

FUNCTION LoadSurface(root AS STRING) AS INTEGER
  DIM surf AS STRING, lines$() AS STRING, ln AS STRING, f$() AS STRING
  DIM i AS LONG
  g_n = 0
  g_dict = NEW DICT OF STRING, INTEGER
  surf = FindSurface(root)
  IF LEN(surf) = 0 THEN RETURN 0
  lines$ = IO.READLINES(surf)
  FOR i = 0 TO UBOUND(lines$)
    ln = TRIM$(lines$(i))
    IF LEN(ln) > 0 AND LEFT$(ln, 1) <> "#" THEN
      f$ = SPLIT(ln, ",")
      IF UBOUND(f$) >= 2 THEN
        IF UBOUND(f$) >= 3 THEN
          AddName(f$(0), f$(1), f$(2))
        ELSE
          AddName("", "", f$(2))
        END IF
      END IF
    END IF
  NEXT i
  RETURN 1
END FUNCTION

FUNCTION ScanCorpus(dir AS STRING, ext AS STRING, stripComments AS INTEGER) AS LONG
  DIM files$() AS STRING, s AS STRING, w AS STRING, c AS STRING
  DIM f AS LONG, i AS LONG, st AS LONG, hit AS LONG
  DIM inStr AS INTEGER, inComment AS INTEGER
  FOR i = 1 TO g_n : g_seen(i) = 0 : NEXT i
  files$ = GREP.FILES(" ", dir)
  FOR f = 0 TO UBOUND(files$)
    IF RIGHT$(UPPER$(files$(f)), LEN(ext)) = UPPER$(ext) THEN
      IF INSTR(files$(f), "/work/") = 0 AND INSTR(files$(f), "/bin/") = 0 AND INSTR(files$(f), "/obj/") = 0 AND INSTR(files$(f), "/node_modules/") = 0 THEN
        s = IO.READFILE(files$(f))
        i = 1 : inStr = 0 : inComment = 0
        DO WHILE i <= LEN(s)
          c = MID$(s, i, 1)
          IF inComment THEN
            IF c = CHR$(10) THEN inComment = 0
            i = i + 1
          ELSEIF inStr THEN
            IF c = CHR$(34) THEN inStr = 0
            i = i + 1
          ELSEIF stripComments = 1 AND c = CHR$(39) THEN
            inComment = 1 : i = i + 1
          ELSEIF c = CHR$(34) THEN
            inStr = 1 : i = i + 1
          ELSEIF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR c = "_" THEN
            st = i
            DO WHILE i <= LEN(s)
              c = MID$(s, i, 1)
              IF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR (c >= "0" AND c <= "9") OR c = "_" THEN
                i = i + 1
              ELSE
                EXIT DO
              END IF
            LOOP
            w = UPPER$(MID$(s, st, i - st))
            IF w IN g_dict THEN g_seen(g_dict(w)) = 1
          ELSE
            i = i + 1
          END IF
        LOOP
        DIM up AS STRING
        up = UPPER$(s)
        DIM jj AS LONG
        FOR jj = 1 TO g_n
          IF INSTR(g_name(jj), " ") > 0 THEN
            IF INSTR(up, g_name(jj)) > 0 THEN g_seen(jj) = 1
          END IF
          IF g_name(jj) = "SAVESCREEN" AND INSTR(up, "SAVE SCREEN") > 0 THEN g_seen(jj) = 1
          IF g_name(jj) = "RESTORESCREEN" AND INSTR(up, "RESTORE SCREEN") > 0 THEN g_seen(jj) = 1
        NEXT jj
      END IF
    END IF
  NEXT f
  hit = 0
  FOR i = 1 TO g_n : hit = hit + g_seen(i) : NEXT i
  RETURN hit
END FUNCTION

SUB PrintMissing(label AS STRING, nsFilter AS STRING)
  DIM i AS LONG, shown AS LONG
  PRINT "=== senza "; label; " ==="
  shown = 0
  FOR i = 1 TO g_n
    IF g_seen(i) = 0 THEN
      IF LEN(nsFilter) = 0 OR UPPER$(g_ns(i)) = UPPER$(nsFilter) THEN
        PRINT g_ns(i); " ("; g_kind(i); ":"; g_name(i); ")"
        shown = shown + 1
      END IF
    END IF
  NEXT i
  PRINT "totale: "; shown
END SUB

SUB PerNamespace()
  DIM pns(500) AS STRING, tot(500) AS LONG, mt(500) AS LONG, sm(500) AS LONG, gd(500) AS LONG
  DIM np AS LONG, i AS LONG, j AS LONG, found AS INTEGER
  np = 0
  FOR i = 1 TO g_n
    found = 0
    FOR j = 0 TO np - 1
      IF pns(j) = g_ns(i) THEN found = 1 : EXIT FOR
    NEXT j
    IF found = 0 THEN pns(np) = g_ns(i) : np = np + 1
  NEXT i
  FOR i = 1 TO g_n
    FOR j = 0 TO np - 1
      IF pns(j) = g_ns(i) THEN
        tot(j) = tot(j) + 1
        IF (g_mask(i) AND 1) <> 0 THEN mt(j) = mt(j) + 1
        IF (g_mask(i) AND 2) <> 0 THEN sm(j) = sm(j) + 1
        IF (g_mask(i) AND 4) <> 0 THEN gd(j) = gd(j) + 1
      END IF
    NEXT j
  NEXT i
  PRINT "namespace             tot  noMT  noSM  noGD"
  FOR j = 0 TO np - 1
    PRINT pns(j); " "; tot(j); " "; tot(j) - mt(j); " "; tot(j) - sm(j); " "; tot(j) - gd(j)
  NEXT j
END SUB

FUNCTION Main() AS INTEGER
  DIM root AS STRING, guides AS STRING, surf AS STRING, nsFilter AS STRING
  DIM mtDir AS STRING, smDir AS STRING, other AS STRING
  DIM opt AS STRING, arg AS STRING, i AS LONG, up AS LONG
  DIM hm AS LONG, hs AS LONG, hg AS LONG
  DIM mine$() AS STRING, oname$() AS STRING, ons$() AS STRING, okind$() AS STRING
  DIM cmp AS STRING, only AS LONG, o AS LONG, j AS LONG
  root = IO.CWD() : guides = "/Users/gaetano/Documents/AiWiki/01-Projects/GMBasicMonoRepo/Guide" : other = "" : nsFilter = ""
  i = 1
  DO WHILE i <= COMMANDCOUNT()
    opt = COMMAND$(i)
    IF opt = "--root" AND i < COMMANDCOUNT() THEN i = i + 1 : root = COMMAND$(i)
    IF opt = "--guides" AND i < COMMANDCOUNT() THEN i = i + 1 : guides = COMMAND$(i)
    IF opt = "--compare" AND i < COMMANDCOUNT() THEN i = i + 1 : other = COMMAND$(i)
    IF opt = "--ns" AND i < COMMANDCOUNT() THEN i = i + 1 : nsFilter = COMMAND$(i)
    i = i + 1
  LOOP
  IF NOT DIREXISTS(guides) THEN guides = "/Users/gaetano/Documents/AiWiki/01-Projects/GMBasic/Guide"
  IF LoadSurface(root) = 0 THEN
    PRINT "registry non trovato da "; root
    RETURN 2
  END IF
  mtDir = root + "/shared/minitests"
  smDir = root + "/shared/samples"
  IF DIREXISTS(root + "/minitests") THEN mtDir = root + "/minitests"
  IF DIREXISTS(root + "/samples") THEN smDir = root + "/samples"
  hm = ScanCorpus(mtDir, ".bas", 1)
  FOR i = 1 TO g_n
    IF g_seen(i) = 1 THEN g_mask(i) = g_mask(i) + 1
  NEXT i
  hs = ScanCorpus(smDir, ".bas", 0)
  FOR i = 1 TO g_n
    IF g_seen(i) = 1 THEN g_mask(i) = g_mask(i) + 2
  NEXT i
  hg = ScanCorpus(guides, ".md", 0)
  FOR i = 1 TO g_n
    IF g_seen(i) = 1 THEN g_mask(i) = g_mask(i) + 4
  NEXT i
  PRINT "repo: "; root
  PRINT "superficie canonica: "; g_n
  PRINT "  con MiniTest: "; hm; " / "; g_n; " (mancano "; g_n - hm; ")"
  PRINT "  nei sample:   "; hs; " / "; g_n; " (mancano "; g_n - hs; ")"
  PRINT "  nelle guide:  "; hg; " / "; g_n; " (mancano "; g_n - hg; ")"
  ' --missing: ricalcola il corpus scelto in g_seen
  i = 1
  DO WHILE i <= COMMANDCOUNT()
    IF COMMAND$(i) = "--missing" AND i < COMMANDCOUNT() THEN
      arg = COMMAND$(i + 1)
      IF arg = "mt" THEN hm = ScanCorpus(mtDir, ".bas", 1) : PrintMissing("MiniTest", nsFilter)
      IF arg = "sm" THEN hs = ScanCorpus(smDir, ".bas", 0) : PrintMissing("sample", nsFilter)
      IF arg = "gd" THEN hg = ScanCorpus(guides, ".md", 0) : PrintMissing("guide", nsFilter)
      RETURN 0
    END IF
    i = i + 1
  LOOP
  ' --per-ns: serve una scansione combinata per namespace; rilancio i tre corpus
  i = 1
  DO WHILE i <= COMMANDCOUNT()
    IF COMMAND$(i) = "--per-ns" THEN
      PerNamespace()
    END IF
    i = i + 1
  LOOP
  IF LEN(other) > 0 THEN
    REDIM mine$(g_n)
    FOR i = 1 TO g_n : mine$(i) = UPPER$(g_ns(i)) + "." + g_name(i) : NEXT i
    DIM nm AS LONG
    nm = g_n
    IF LoadSurface(other) = 0 THEN
      PRINT "registry confronto non trovato: "; other
      RETURN 2
    END IF
    FOR i = 1 TO nm
      only = 1
      FOR j = 1 TO g_n
        IF mine$(i) = UPPER$(g_ns(j)) + "." + g_name(j) THEN only = 0 : EXIT FOR
      NEXT j
      IF only = 1 THEN PRINT "solo in "; root; ": "; mine$(i)
    NEXT i
    FOR i = 1 TO g_n
      only = 1
      FOR j = 1 TO nm
        IF mine$(j) = UPPER$(g_ns(i)) + "." + g_name(i) THEN only = 0 : EXIT FOR
      NEXT j
      IF only = 1 THEN PRINT "solo in "; other; ": "; g_ns(i); "."; g_name(i)
    NEXT i
  END IF
  RETURN 0
END FUNCTION
