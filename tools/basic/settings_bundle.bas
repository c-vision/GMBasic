' settings_bundle.bas — settings MINIMI accanto a un programma compilato.
' Port BASIC di tools/data/settings_bundle.py (stessa regola di CommonLib/ProgramBundle.cs,
' RuntimeSections): dal system globale + l'app del programma si emette UN solo file con le
' sole sezioni che il RUNTIME legge.
'
' Sezioni conservate (fonte canonica: common/CommonLib/ProgramBundle.cs):
'   paths                 percorsi dati (GmbiPaths / pr_load)
'   consoleConfiguration  font e geometria console (gmb_console.c, gmb_settings.c)
'   engine                opzioni del motore (gmb_opts.c / GmbEnv)
' Tutto il resto (compiler, importFolders, defaultNamespaces, libraryFolders…) serve a
' COMPILARE e non si distribuisce: un binario vecchio non deve cambiare comportamento
' quando cambiano i default del compilatore.
'
' L'app vince CHIAVE PER CHIAVE sulle sezioni comuni; liste e scalari dell'app sostituiscono
' quelli globali.
'
' PERCHE' NIENTE JSON.PARSE/STRINGIFY. Il round-trip del namespace JSON perde i BOOLEANI:
' JSON.PARSE di {"x": true} seguito da STRINGIFY restituisce {"x": -1} (gamba IL, misurato
' 2026-10-29; l'assegnazione o("t") = TRUE invece li conserva, vedi il MiniTest
' json/bool_assign.bas). Qui i valori si copiano GREZZI dal testo: booleani, numeri e stringhe
' restano identici al sorgente, senza dipendere da quel round-trip.
'
' Uso (dalla root del monorepo, path assoluti come nello script chiamante):
'   tools/basic/settings_bundle.bas --system <system.settings.json> --app <app.settings.json> --out <file>
'
' Exit: 0 scritto o gia' aggiornato, 2 errore d'uso, 3 niente da scrivere.
' Il JSONC (// e /* */) viene ripulito fuori dalle stringhe, come il .py.
IMPORT IO

DIM g_q AS STRING
DIM g_k(400) AS STRING
DIM g_v(400) AS STRING
DIM g_n AS INTEGER

FUNCTION IsWs(c AS STRING) AS INTEGER
  IF c = " " OR c = CHR$(9) OR c = CHR$(10) OR c = CHR$(13) THEN RETURN -1
  RETURN 0
END FUNCTION

' Posizione della virgoletta che chiude la stringa aperta in i (gestisce \").
FUNCTION StrEnd(t AS STRING, i AS LONG) AS LONG
  DIM j AS LONG
  DIM c AS STRING
  j = i + 1
  DO WHILE j <= LEN(t)
    c = MID$(t, j, 1)
    IF c = "\" THEN
      j = j + 2
    ELSEIF c = g_q THEN
      RETURN j
    ELSE
      j = j + 1
    END IF
  LOOP
  RETURN LEN(t)
END FUNCTION

FUNCTION SkipWs(t AS STRING, i AS LONG) AS LONG
  DO WHILE i <= LEN(t)
    IF IsWs(MID$(t, i, 1)) THEN
      i = i + 1
    ELSE
      EXIT DO
    END IF
  LOOP
  RETURN i
END FUNCTION

' Ultima posizione del valore che inizia in i: stringa, contenitore bilanciato o scalare.
FUNCTION ValEnd(t AS STRING, i AS LONG) AS LONG
  DIM c AS STRING
  DIM j AS LONG, depth AS LONG
  DIM inStr AS INTEGER
  c = MID$(t, i, 1)
  IF c = g_q THEN RETURN StrEnd(t, i)
  IF c = "{" OR c = "[" THEN
    depth = 0 : j = i : inStr = 0
    DO WHILE j <= LEN(t)
      c = MID$(t, j, 1)
      IF inStr THEN
        IF c = "\" THEN
          j = j + 2
        ELSE
          IF c = g_q THEN inStr = 0
          j = j + 1
        END IF
      ELSEIF c = g_q THEN
        inStr = 1 : j = j + 1
      ELSEIF c = "{" OR c = "[" THEN
        depth = depth + 1 : j = j + 1
      ELSEIF c = "}" OR c = "]" THEN
        depth = depth - 1 : j = j + 1
        IF depth = 0 THEN RETURN j - 1
      ELSE
        j = j + 1
      END IF
    LOOP
    RETURN LEN(t)
  END IF
  j = i
  DO WHILE j <= LEN(t)
    c = MID$(t, j, 1)
    IF c = "," OR c = "}" OR c = "]" OR c = CHR$(10) OR c = CHR$(13) THEN RETURN j - 1
    j = j + 1
  LOOP
  RETURN j - 1
END FUNCTION

' Valore GREZZO del campo <name> nell'oggetto il cui "{" sta in <open>; "" se assente.
FUNCTION ObjGet(t AS STRING, open AS LONG, name AS STRING) AS STRING
  DIM i AS LONG, kStart AS LONG, kEnd AS LONG, ve AS LONG
  DIM kRaw AS STRING
  i = open + 1
  DO WHILE i <= LEN(t)
    i = SkipWs(t, i)
    IF i > LEN(t) THEN EXIT DO
    IF MID$(t, i, 1) = "}" THEN EXIT DO
    IF MID$(t, i, 1) = "," THEN
      i = i + 1
      i = SkipWs(t, i)
    END IF
    IF MID$(t, i, 1) <> g_q THEN EXIT DO
    kStart = i
    kEnd = StrEnd(t, i)
    kRaw = MID$(t, kStart + 1, kEnd - kStart - 1)
    i = SkipWs(t, kEnd + 1)
    IF i <= LEN(t) AND MID$(t, i, 1) = ":" THEN i = i + 1
    i = SkipWs(t, i)
    ve = ValEnd(t, i)
    IF UPPER$(kRaw) = UPPER$(name) THEN RETURN MID$(t, i, ve - i + 1)
    i = ve + 1
  LOOP
  RETURN ""
END FUNCTION

' Riempie g_k/g_v/g_n con le coppie (chiave, valore grezzo) dell'oggetto a pos <open>.
SUB ObjPairs(t AS STRING, open AS LONG)
  DIM i AS LONG, kStart AS LONG, kEnd AS LONG, ve AS LONG
  g_n = 0
  i = open + 1
  DO WHILE i <= LEN(t)
    i = SkipWs(t, i)
    IF i > LEN(t) THEN EXIT DO
    IF MID$(t, i, 1) = "}" THEN EXIT DO
    IF MID$(t, i, 1) = "," THEN
      i = i + 1
      i = SkipWs(t, i)
    END IF
    IF MID$(t, i, 1) <> g_q THEN EXIT DO
    kStart = i
    kEnd = StrEnd(t, i)
    i = SkipWs(t, kEnd + 1)
    IF i <= LEN(t) AND MID$(t, i, 1) = ":" THEN i = i + 1
    i = SkipWs(t, i)
    ve = ValEnd(t, i)
    IF g_n < 400 THEN
      g_n = g_n + 1
      g_k(g_n) = MID$(t, kStart + 1, kEnd - kStart - 1)
      g_v(g_n) = MID$(t, i, ve - i + 1)
    END IF
    i = ve + 1
  LOOP
END SUB

' Testo del file con i commenti JSONC rimossi; "" se non c'e'.
FUNCTION StripComments(t AS STRING) AS STRING
  DIM out AS STRING
  DIM c AS STRING
  DIM i AS LONG, n AS LONG, j AS LONG
  DIM inStr AS INTEGER
  out = "" : i = 1 : n = LEN(t) : inStr = 0
  DO WHILE i <= n
    c = MID$(t, i, 1)
    IF inStr THEN
      IF c = "\" AND i + 1 <= n THEN
        out = out + c + MID$(t, i + 1, 1)
        i = i + 2
      ELSE
        out = out + c
        IF c = g_q THEN inStr = 0
        i = i + 1
      END IF
    ELSEIF c = g_q THEN
      inStr = 1
      out = out + c
      i = i + 1
    ELSEIF c = "/" AND i + 1 <= n AND MID$(t, i + 1, 1) = "/" THEN
      DO WHILE i <= n AND MID$(t, i, 1) <> CHR$(10)
        i = i + 1
      LOOP
    ELSEIF c = "/" AND i + 1 <= n AND MID$(t, i + 1, 1) = "*" THEN
      j = INSTR(i + 2, t, "*/")
      IF j = 0 THEN
        i = n + 1
      ELSE
        i = j + 2
      END IF
    ELSE
      out = out + c
      i = i + 1
    END IF
  LOOP
  RETURN out
END FUNCTION

' Fuori dalle stringhe gli spazi si compattano a UNO: i commenti rimossi lasciano
' indentazione e righe vuote (\n    \n) che finirebbero dentro i valori grezzi.
FUNCTION CompactWs(t AS STRING) AS STRING
  DIM out AS STRING
  DIM c AS STRING, prev AS STRING
  DIM i AS LONG, n AS LONG
  DIM inStr AS INTEGER
  out = "" : i = 1 : n = LEN(t) : inStr = 0 : prev = ""
  DO WHILE i <= n
    c = MID$(t, i, 1)
    IF inStr THEN
      out = out + c
      IF c = "\" AND i + 1 <= n THEN
        out = out + MID$(t, i + 1, 1)
        i = i + 2
      ELSE
        IF c = g_q THEN inStr = 0
        i = i + 1
      END IF
    ELSEIF c = g_q THEN
      inStr = 1
      out = out + c
      i = i + 1
    ELSEIF IsWs(c) THEN
      IF LEN(out) > 0 AND prev <> " " THEN out = out + " "
      i = i + 1
    ELSE
      out = out + c
      i = i + 1
    END IF
    prev = c
  LOOP
  RETURN out
END FUNCTION

FUNCTION LoadText(path AS STRING) AS STRING
  IF LEN(path) = 0 THEN RETURN ""
  IF NOT FILEEXISTS(path) THEN RETURN ""
  RETURN CompactWs(StripComments(IO.READFILE(path)))
END FUNCTION

FUNCTION DirOf(path AS STRING) AS STRING
  DIM p AS LONG
  DIM i AS LONG
  p = 0
  FOR i = 1 TO LEN(path)
    IF MID$(path, i, 1) = "/" THEN p = i
  NEXT i
  IF p <= 1 THEN RETURN ""
  RETURN LEFT$(path, p - 1)
END FUNCTION

' Fonde due OGGETTI: chiavi del system nell'ordine, sovrascritte dall'app (che tiene la
' posizione delle chiavi gia' presenti e appende le nuove) — come dict(s) + update(a).
FUNCTION MergeObjects(sysText AS STRING, appText AS STRING) AS STRING
  DIM m AS DICT OF STRING, STRING = NEW DICT OF STRING, STRING
  DIM i AS INTEGER
  DIM out AS STRING, n AS INTEGER
  DIM k AS STRING

  ObjPairs(sysText, 1)
  FOR i = 1 TO g_n
    m.Set(g_k(i), g_v(i))
  NEXT i
  ObjPairs(appText, 1)
  FOR i = 1 TO g_n
    m.Set(g_k(i), g_v(i))
  NEXT i

  out = "" : n = 0
  FOR EACH k, v IN m
    IF n > 0 THEN out = out + ","
    out = out + CHR$(10) + "    " + g_q + k + g_q + ": " + v
    n = n + 1
  NEXT
  IF n = 0 THEN RETURN "{}"
  RETURN "{" + out + CHR$(10) + "  }"
END FUNCTION

FUNCTION Main() AS INTEGER
  DIM sysPath AS STRING, appPath AS STRING, outPath AS STRING
  DIM argText AS STRING
  DIM i AS INTEGER, argc AS INTEGER
  DIM sysText AS STRING, appText AS STRING
  DIM hasSys AS INTEGER, hasApp AS INTEGER
  DIM js AS STRING, ja AS STRING
  DIM secText AS STRING, out AS STRING
  DIM secs(3) AS STRING
  DIM s AS INTEGER, ns AS INTEGER
  DIM existing AS STRING, dir AS STRING
  DIM h AS INTEGER
  DIM rootSys AS LONG, rootApp AS LONG

  g_q = CHR$(34)
  secs(0) = "paths"
  secs(1) = "consoleConfiguration"
  secs(2) = "engine"

  argc = COMMANDCOUNT()
  FOR i = 1 TO argc
    argText = COMMAND$(i)
    IF argText = "--system" AND i < argc THEN
      i = i + 1 : sysPath = COMMAND$(i)
    ELSEIF argText = "--app" AND i < argc THEN
      i = i + 1 : appPath = COMMAND$(i)
    ELSEIF argText = "--out" AND i < argc THEN
      i = i + 1 : outPath = COMMAND$(i)
    END IF
  NEXT i
  IF LEN(outPath) = 0 THEN
    PRINT "uso: settings_bundle.bas --system <file> --app <file> --out <file>"
    RETURN 2
  END IF

  sysText = LoadText(sysPath)
  appText = LoadText(appPath)
  hasSys = 0 : hasApp = 0
  rootSys = INSTR(1, sysText, "{")
  rootApp = INSTR(1, appText, "{")
  IF rootSys > 0 THEN hasSys = 1
  IF rootApp > 0 THEN hasApp = 1
  IF hasSys = 0 AND hasApp = 0 THEN RETURN 3

  out = "{" + CHR$(10)
  ns = 0
  FOR s = 0 TO 2
    js = "" : ja = ""
    IF hasSys = 1 THEN js = ObjGet(sysText, rootSys, secs(s))
    IF hasApp = 1 THEN ja = ObjGet(appText, rootApp, secs(s))
    IF js = "null" THEN js = ""
    IF ja = "null" THEN ja = ""
    IF LEN(js) > 0 OR LEN(ja) > 0 THEN
      IF LEN(js) > 0 AND LEN(ja) > 0 AND LEFT$(js, 1) = "{" AND LEFT$(ja, 1) = "{" THEN
        secText = MergeObjects(js, ja)
      ELSEIF LEN(ja) > 0 THEN
        secText = ja
      ELSE
        secText = js
      END IF
      ns = ns + 1
      IF ns > 1 THEN out = out + "," + CHR$(10)
      out = out + "  " + g_q + secs(s) + g_q + ": " + secText
    END IF
  NEXT s
  out = out + CHR$(10) + "}" + CHR$(10)

  IF FILEEXISTS(outPath) THEN
    existing = IO.READFILE(outPath)
    IF existing = out THEN RETURN 0
  END IF
  dir = DirOf(outPath)
  IF LEN(dir) > 0 AND NOT DIREXISTS(dir) THEN MKDIR dir
  h = IO.FCREATE(outPath)
  IF h = 0 THEN
    PRINT "settings_bundle: impossibile scrivere "; outPath
    RETURN 2
  END IF
  IO.FWRITE(h, out)
  IO.FCLOSE(h)
  RETURN 0
END FUNCTION
