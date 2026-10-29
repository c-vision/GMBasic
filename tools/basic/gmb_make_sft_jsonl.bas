' gmb_make_sft_jsonl.bas — JSONL SFT dai doc (da gmb_make_sft_jsonl.py).
' Uso: gmb_make_sft_jsonl.bas [--root DIR] [--out FILE] [--n N] [--pc N] [--cc N]
IMPORT IO
IMPORT GREP
IMPORT JSON

SUB SortStr(a$())
  DIM i AS LONG, j AS LONG, n AS LONG, t AS STRING
  n = UBOUND(a$) : i = 0
  DO WHILE i <= n
    j = i + 1
    DO WHILE j <= n
      IF a$(j) < a$(i) THEN
        t = a$(i) : a$(i) = a$(j) : a$(j) = t
      END IF
      j = j + 1
    LOOP
    i = i + 1
  LOOP
END SUB

FUNCTION Clean(s AS STRING) AS STRING
  DIM t AS STRING
  t = REPLACE(s, CHR$(92), " ")
  t = REPLACE(t, CHR$(34), "'")
  t = REPLACE(t, CHR$(10), " ")
  t = REPLACE(t, CHR$(13), " ")
  RETURN t
END FUNCTION

FUNCTION Main() AS INTEGER
  DIM root AS STRING, outp AS STRING, tgt AS LONG, pc AS LONG, cc AS LONG
  DIM d$() AS STRING
  DIM g$() AS STRING
  DIM files$() AS STRING
  DIM k AS LONG, i AS LONG, nf AS LONG, nt AS LONG, n AS LONG
  DIM t AS STRING, p AS STRING, c AS STRING, buf AS STRING, h AS LONG
  root = IO.CWD() : outp = "/tmp/gmb_r5.jsonl" : tgt = 20000 : pc = 128 : cc = 64
  k = 1
  DO WHILE k <= COMMANDCOUNT()
    IF COMMAND$(k) = "--root" AND k < COMMANDCOUNT() THEN
      k = k + 1 : root = COMMAND$(k)
    ELSEIF COMMAND$(k) = "--out" AND k < COMMANDCOUNT() THEN
      k = k + 1 : outp = COMMAND$(k)
    ELSEIF COMMAND$(k) = "--n" AND k < COMMANDCOUNT() THEN
      k = k + 1 : tgt = VAL(COMMAND$(k))
    ELSEIF COMMAND$(k) = "--pc" AND k < COMMANDCOUNT() THEN
      k = k + 1 : pc = VAL(COMMAND$(k))
    ELSEIF COMMAND$(k) = "--cc" AND k < COMMANDCOUNT() THEN
      k = k + 1 : cc = VAL(COMMAND$(k))
    END IF
    k = k + 1
  LOOP
  IF pc < 1 THEN pc = 1
  IF cc < 1 THEN cc = 1
  d$ = GREP.FILES(" ", root + "/docs")
  IF DIREXISTS(root + "/Guide") <> 0 THEN g$ = GREP.FILES(" ", root + "/Guide") ELSE REDIM g$(0)
  nt = UBOUND(d$) + UBOUND(g$) + 1
  REDIM files$(nt)
  nf = 0
  i = 0
  DO WHILE i <= UBOUND(d$)
    files$(nf) = d$(i) : nf = nf + 1 : i = i + 1
  LOOP
  i = 0
  DO WHILE i <= UBOUND(g$)
    files$(nf) = g$(i) : nf = nf + 1 : i = i + 1
  LOOP
  IF nf > 1 THEN SortStr(files$())
  h = IO.FCREATE(outp)
  n = 0
  i = 0
  DO WHILE i < nf
    t = IO.READFILE(files$(i))
    IF LEN(t) > 300 THEN
      DIM st AS LONG, step AS LONG
      st = 1 : step = INT(pc / 2)
      IF step < 1 THEN step = 1
      DO WHILE st + pc + cc - 1 <= LEN(t)
        p = Clean(MID$(t, st, pc))
        c = Clean(MID$(t, st + pc, cc))
        IF LEN(c) >= 24 THEN
          buf = "{" + JSON.STRINGIFY("prompt") + ": " + JSON.STRINGIFY(p) + ", " + JSON.STRINGIFY("completion") + ": " + JSON.STRINGIFY(c) + "}" + CHR$(10)
          IO.FWRITE(h, buf)
          n = n + 1
          IF n >= tgt THEN
            IO.FCLOSE(h)
            PRINT "[sft] doc="; nf; " esempi="; n; " -> "; outp
            RETURN 0
          END IF
        END IF
        st = st + step
      LOOP
    END IF
    i = i + 1
  LOOP
  IO.FCLOSE(h)
  PRINT "[sft] doc="; nf; " esempi="; n; " -> "; outp
  RETURN 0
END FUNCTION
