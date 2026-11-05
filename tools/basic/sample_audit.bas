' ==============================================================================
' sample_audit.bas — CENSIMENTO dei Sample (shared/samples/**), non dei MiniTest.
'
' Contratto (Guida Test/Sample §7.3): un Sample e' un esempio COMPLETO e realistico che
' INSEGNA un uso concreto (>=2 domini, scenario riconoscibile), senza `' EXPECT:`.
' Non tutti i file sotto shared/samples sono Sample: Modules/** contiene moduli importati
' (nessun entry point), Fixtures/** dati, packages/** artefatti dei pacchetti.
'
' Classificazione emessa:
'   SURFACE   tag `' SURFACE:` in testa (drill di superficie, da riscrivere o accettare)
'   MODULO    sotto Modules/ o file di supporto (include/)
'   FIXTURE   sotto Fixtures/
'   PACKAGE   sotto packages/
'   DIDATTICO >= CAMPIONE_RIGHE righe e >=2 IMPORT (candidato scenario)
'   CORTO     < CAMPIONE_RIGHE righe (candidato a revisione/accorpamento)
'   ENV       categoria dipendente da rete/UI/modelli (oracolo non deterministico)
'
' Uso (dalla root del monorepo, gamba nativa):
'   bash tools/sh/gmb_c99native_run.sh ../tools/basic/sample_audit.bas sa
'   bash tools/sh/gmb_c99native_run.sh ../tools/basic/sample_audit.bas sa -- --root shared/samples
' ==============================================================================
IMPORT IO
IMPORT SYSTEM

DIM g_paths(32768) AS STRING
DIM g_cats(32768) AS STRING
DIM g_lines(32768) AS LONG
DIM g_imports(32768) AS LONG
DIM g_tags(32768) AS STRING
DIM g_n AS LONG

DIM g_out AS STRING

FUNCTION TrimS(s AS STRING) AS STRING
  RETURN RTRIM$(LTRIM$(s))
END FUNCTION

FUNCTION Q(s AS STRING) AS STRING
  RETURN CHR$(34) + s + CHR$(34)
END FUNCTION

FUNCTION EndsWith(s AS STRING, suf AS STRING) AS LONG
  IF LEN(s) >= LEN(suf) THEN
    IF RIGHT$(s, LEN(suf)) = suf THEN RETURN -1
  END IF
  RETURN 0
END FUNCTION

FUNCTION HasIn(list AS STRING, item AS STRING) AS LONG
  DIM all$() AS STRING, i AS LONG
  all$ = SPLIT(list, ",")
  FOR i = 0 TO UBOUND(all$)
    IF all$(i) = item THEN RETURN -1
  NEXT i
  RETURN 0
END FUNCTION

FUNCTION IsEnvCat(cat AS STRING) AS LONG
  RETURN HasIn("Ai,AiLLM,Audio,Browser,Computer,Connectors,Http,Network,cua,graphics,windows,Controls,Console,GameDev,Team,Web,mvc", cat)
END FUNCTION

FUNCTION FirstSeg(rel AS STRING) AS STRING
  DIM i AS LONG
  FOR i = 1 TO LEN(rel)
    IF MID$(rel, i, 1) = "/" THEN RETURN LEFT$(rel, i - 1)
  NEXT i
  RETURN rel
END FUNCTION

FUNCTION HasSlash(rel AS STRING) AS LONG
  IF INSTR(rel, "/") > 0 THEN RETURN -1
  RETURN 0
END FUNCTION

FUNCTION RelOf(full AS STRING, root AS STRING) AS STRING
  DIM b AS STRING
  b = root
  IF RIGHT$(b, 1) <> "/" THEN b = b + "/"
  IF LEFT$(full, LEN(b)) = b THEN RETURN MID$(full, LEN(b) + 1)
  RETURN full
END FUNCTION

' conta gli IMPORT distinti (nome canonico, senza alias)
FUNCTION CountImports(t AS STRING) AS LONG
  DIM ln$() AS STRING, i AS LONG, s AS STRING, nm AS STRING, k AS LONG
  DIM names(512) AS STRING, n AS LONG, j AS LONG, dup AS LONG
  ln$ = SPLIT(t, CHR$(10))
  n = 0
  IF UBOUND(ln$) < 0 THEN RETURN 0
  FOR i = 0 TO UBOUND(ln$)
    s = TrimS(ln$(i))
    IF LEFT$(UCASE$(s), 7) = "IMPORT " THEN
      nm = TrimS(MID$(s, 8))
      ' togli la parte dopo lo spazio (AS alias / selezione)
      k = INSTR(nm, " ")
      IF k > 0 THEN nm = LEFT$(nm, k - 1)
      IF LEN(nm) > 0 THEN
        dup = 0
        FOR j = 0 TO n - 1
          IF names(j) = nm THEN dup = -1
        NEXT j
        IF dup = 0 AND n < 512 THEN
          names(n) = nm
          n = n + 1
        END IF
      END IF
    END IF
  NEXT i
  RETURN n
END FUNCTION

FUNCTION TextHasTag(t AS STRING, tag AS STRING) AS LONG
  DIM ln$() AS STRING, i AS LONG, s AS STRING
  ln$ = SPLIT(t, CHR$(10))
  IF UBOUND(ln$) < 0 THEN RETURN 0
  FOR i = 0 TO UBOUND(ln$)
    s = LTRIM$(ln$(i))
    IF LEFT$(UCASE$(s), LEN(tag) + 2) = "' " + UCASE$(tag) + ":" THEN RETURN -1
  NEXT i
  RETURN 0
END FUNCTION

FUNCTION LineCount(t AS STRING) AS LONG
  DIM ln$() AS STRING
  ln$ = SPLIT(t, CHR$(10))
  IF UBOUND(ln$) < 0 THEN RETURN 0
  RETURN UBOUND(ln$) + 1
END FUNCTION

FUNCTION FirstLine(t AS STRING) AS STRING
  DIM p AS LONG
  p = INSTR(t, CHR$(10))
  IF p = 0 THEN RETURN t
  RETURN LEFT$(t, p - 1)
END FUNCTION

SUB AddTag(idx AS LONG, tag AS STRING)
  IF LEN(g_tags(idx)) = 0 THEN
    g_tags(idx) = tag
  ELSE
    g_tags(idx) = g_tags(idx) + "," + tag
  END IF
END SUB

SUB Classify(full AS STRING, root AS STRING, rel AS STRING)
  DIM t AS STRING, cat AS STRING, ntags AS LONG, imps AS LONG, nl AS LONG
  DIM first AS STRING
  t = IO.READFILE(full)
  IF LEN(t) = 0 THEN t = " "
  cat = FirstSeg(rel)
  first = LTRIM$(FirstLine(t))
  nl = LineCount(t)
  imps = CountImports(t)
  ntags = 0
  IF LEFT$(UCASE$(first), 10) = "' SURFACE:" THEN
    AddTag(g_n, "SURFACE")
    ntags = ntags + 1
  END IF
  IF cat = "Modules" OR cat = "Fixtures" OR cat = "packages" OR INSTR(rel, "include/") > 0 THEN
    IF cat = "Modules" THEN
      AddTag(g_n, "MODULO")
    ELSEIF cat = "Fixtures" THEN
      AddTag(g_n, "FIXTURE")
    ELSEIF cat = "packages" THEN
      AddTag(g_n, "PACKAGE")
    ELSE
      AddTag(g_n, "SUPPORTO")
    END IF
    ntags = ntags + 1
  END IF
  IF TextHasTag(t, "EXPECT") <> 0 THEN
    AddTag(g_n, "EXPECT")
    ntags = ntags + 1
  END IF
  IF nl < 20 THEN
    AddTag(g_n, "CORTO")
    ntags = ntags + 1
  END IF
  IF IsEnvCat(cat) <> 0 THEN
    AddTag(g_n, "ENV")
    ntags = ntags + 1
  END IF
  IF ntags = 0 AND imps >= 2 THEN AddTag(g_n, "DIDATTICO")
  g_paths(g_n) = rel
  g_cats(g_n) = cat
  g_lines(g_n) = nl
  g_imports(g_n) = imps
  g_n = g_n + 1
END SUB

SUB Emit(s AS STRING)
  g_out = g_out + s + CHR$(10)
END SUB

FUNCTION CountTag(tag AS STRING) AS LONG
  DIM i AS LONG, n AS LONG
  FOR i = 0 TO g_n - 1
    IF HasIn(g_tags(i), tag) <> 0 THEN n = n + 1
  NEXT i
  RETURN n
END FUNCTION

SUB Section(title AS STRING, tag AS STRING)
  DIM i AS LONG, n AS LONG, lbl AS STRING
  Emit("== " + title + " ==")
  n = 0
  FOR i = 0 TO g_n - 1
    IF HasIn(g_tags(i), tag) <> 0 THEN
      lbl = g_paths(i)
      DO WHILE LEN(lbl) < 70
        lbl = lbl + " "
      LOOP
      Emit("  " + lbl + " righe=" + LTRIM$(STR$(g_lines(i))) + " import=" + LTRIM$(STR$(g_imports(i))))
      n = n + 1
    END IF
  NEXT i
  Emit("  (" + LTRIM$(STR$(n)) + " file)")
  Emit("")
END SUB

FUNCTION Main() AS INTEGER
  DIM root AS STRING, outPath AS STRING, a AS STRING, i AS LONG
  DIM t AS STRING, ln$() AS STRING, rel AS STRING, cat AS STRING
  DIM cats(512) AS STRING, nc AS LONG, j AS LONG, dup AS LONG
  DIM cnts(512) AS LONG, tmpN AS LONG, tmpS AS STRING, lbl AS STRING
  DIM h AS LONG, wr AS LONG

  SET SHELL ON
  ' il runner esegue il binario dentro tools/basic: la root del monorepo e' due livelli sopra
  IF DIREXISTS(IO.PARENTDIR(IO.PARENTDIR(IO.CWD())) + "/shared/samples") THEN
    root = IO.PARENTDIR(IO.PARENTDIR(IO.CWD())) + "/shared/samples"
    outPath = IO.PARENTDIR(IO.PARENTDIR(IO.CWD())) + "/tools/reports/sample_audit.txt"
  ELSE
    root = "shared/samples"
    outPath = "tools/reports/sample_audit.txt"
  END IF
  i = 1
  DO WHILE i <= COMMANDCOUNT()
    a = COMMAND$(i)
    IF a = "--root" AND i < COMMANDCOUNT() THEN
      i = i + 1
      root = COMMAND$(i)
    END IF
    IF a = "--out" AND i < COMMANDCOUNT() THEN
      i = i + 1
      outPath = COMMAND$(i)
    END IF
    i = i + 1
  LOOP

  t = SYSTEM.SHELLOUT("find " + Q(root) + " -name '*.bas' 2>/dev/null | sort")
  ln$ = SPLIT(t, CHR$(10))
  g_n = 0
  IF UBOUND(ln$) >= 0 THEN
    FOR i = 0 TO UBOUND(ln$)
      IF LEN(TrimS(ln$(i))) > 0 AND g_n < 32768 THEN
        rel = RelOf(TrimS(ln$(i)), root)
        Classify(TrimS(ln$(i)), root, rel)
      END IF
    NEXT i
  END IF

  ' categorie (conteggio)
  nc = 0
  FOR i = 0 TO g_n - 1
    cat = g_cats(i)
    dup = 0
    FOR j = 0 TO nc - 1
      IF cats(j) = cat THEN dup = -1
    NEXT j
    IF dup = 0 AND nc < 512 THEN
      cats(nc) = cat
      nc = nc + 1
    END IF
  NEXT i

  g_out = ""
  Emit("SAMPLE AUDIT — shared/samples/**/*.bas")
  Emit("totale file: " + LTRIM$(STR$(g_n)) + "   categorie: " + LTRIM$(STR$(nc)))
  Emit("")
  Emit("== per categoria ==")
  ' conteggi + ordinamento per numero decrescente, poi nome
  FOR i = 0 TO nc - 1
    cnts(i) = 0
    FOR j = 0 TO g_n - 1
      IF g_cats(j) = cats(i) THEN cnts(i) = cnts(i) + 1
    NEXT j
  NEXT i
  FOR i = 0 TO nc - 2
    FOR j = 0 TO nc - 2 - i
      IF cnts(j) < cnts(j + 1) OR (cnts(j) = cnts(j + 1) AND cats(j) > cats(j + 1)) THEN
        tmpN = cnts(j)
        cnts(j) = cnts(j + 1)
        cnts(j + 1) = tmpN
        tmpS = cats(j)
        cats(j) = cats(j + 1)
        cats(j + 1) = tmpS
      END IF
    NEXT j
  NEXT i
  FOR i = 0 TO nc - 1
    lbl = cats(i)
    DO WHILE LEN(lbl) < 20
      lbl = lbl + " "
    LOOP
    Emit("  " + lbl + LTRIM$(STR$(cnts(i))))
  NEXT i
  Emit("")
  Emit("== per classificazione (un file puo' avere piu' tag) ==")
  Emit("  SURFACE    " + LTRIM$(STR$(CountTag("SURFACE"))))
  Emit("  EXPECT     " + LTRIM$(STR$(CountTag("EXPECT"))))
  Emit("  CORTO      " + LTRIM$(STR$(CountTag("CORTO"))))
  Emit("  ENV        " + LTRIM$(STR$(CountTag("ENV"))))
  Emit("  DIDATTICO  " + LTRIM$(STR$(CountTag("DIDATTICO"))))
  Emit("  MODULO     " + LTRIM$(STR$(CountTag("MODULO"))))
  Emit("  FIXTURE    " + LTRIM$(STR$(CountTag("FIXTURE"))))
  Emit("  PACKAGE    " + LTRIM$(STR$(CountTag("PACKAGE"))))
  Emit("")

  Section("SURFACE (drill di superficie)", "SURFACE")
  Section("EXPECT nei Sample (violazione del contratto)", "EXPECT")
  Section("CORTO (< 20 righe, revisione/accorpamento)", "CORTO")
  Section("ENV (rete/UI/modelli: oracolo non deterministico)", "ENV")
  Section("DIDATTICO (>=2 domini, >= 20 righe)", "DIDATTICO")

  wr = SYSTEM.SHELLOUT("rm -f " + Q(outPath))
  h = IO.FCREATE(outPath)
  wr = IO.FWRITE(h, g_out)
  IO.FCLOSE(h)

  PRINT "totale file: "; g_n; "   categorie: "; nc
  PRINT "scritto "; outPath
  RETURN 0
END FUNCTION
