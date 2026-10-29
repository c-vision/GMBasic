' gmb_make_nll_corpus.bas — corpus NLL deterministico (da gmb_make_nll_corpus.py).
' Uso: gmb_make_nll_corpus.bas [--root DIR] [--out FILE] [--bytes N]
IMPORT IO
IMPORT GREP
IMPORT CRYPTOGRAPHY

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

FUNCTION IsCode(p AS STRING) AS INTEGER
  DIM low AS STRING
  low = LOWER$(p)
  IF INSTR(low, "native/") = 0 THEN RETURN 0
  IF RIGHT$(low, 2) = ".c" OR RIGHT$(low, 2) = ".h" THEN RETURN 1
  RETURN 0
END FUNCTION

FUNCTION Main() AS INTEGER
  DIM root AS STRING, outp AS STRING, nbytes AS LONG
  DIM d$() AS STRING
  DIM g$() AS STRING
  DIM c$() AS STRING
  DIM files$() AS STRING
  DIM k AS LONG, i AS LONG, nf AS LONG, n AS LONG, h AS LONG, slash AS LONG
  DIM p AS STRING, txt AS STRING, docs AS STRING, code AS STRING, text AS STRING
  DIM ndoc AS LONG, ncode AS LONG, dd AS STRING, hx AS STRING
  root = IO.CWD() : outp = "samples/Fixtures/nll/corpus.txt" : nbytes = 250000
  k = 1
  DO WHILE k <= COMMANDCOUNT()
    IF COMMAND$(k) = "--root" AND k < COMMANDCOUNT() THEN
      k = k + 1 : root = COMMAND$(k)
    ELSEIF COMMAND$(k) = "--out" AND k < COMMANDCOUNT() THEN
      k = k + 1 : outp = COMMAND$(k)
    ELSEIF COMMAND$(k) = "--bytes" AND k < COMMANDCOUNT() THEN
      k = k + 1 : nbytes = VAL(COMMAND$(k))
    END IF
    k = k + 1
  LOOP
  d$ = GREP.FILES(" ", root + "/docs")
  IF DIREXISTS(root + "/Guide") <> 0 THEN
    g$ = GREP.FILES(" ", root + "/Guide")
  ELSE
    REDIM g$(0)
  END IF
  c$ = GREP.FILES(" ", root + "/native")
  REDIM files$(UBOUND(d$) + UBOUND(g$) + UBOUND(c$) + 10)
  nf = 0
  i = 0
  DO WHILE i <= UBOUND(d$)
    files$(nf) = d$(i) : nf = nf + 1 : i = i + 1
  LOOP
  i = 0
  DO WHILE i <= UBOUND(g$)
    files$(nf) = g$(i) : nf = nf + 1 : i = i + 1
  LOOP
  i = 0
  DO WHILE i <= UBOUND(c$)
    IF IsCode(c$(i)) <> 0 THEN files$(nf) = c$(i) : nf = nf + 1
    i = i + 1
  LOOP
  ' ordine GREP deterministico
  docs = "" : code = ""
  i = 0
  DO WHILE i < nf
    txt = IO.READFILE(files$(i))
    IF LEN(TRIM$(txt)) > 0 THEN
      IF IsCode(files$(i)) <> 0 THEN code = code + txt ELSE docs = docs + txt
    END IF
    i = i + 1
  LOOP
  ndoc = INT(nbytes * 0.6)
  ncode = nbytes - ndoc
  IF ndoc > LEN(docs) THEN ndoc = LEN(docs)
  IF ncode > LEN(code) THEN ncode = LEN(code)
  text = MID$(docs, 1, ndoc) + CHR$(10) + CHR$(10) + MID$(code, 1, ncode)
  dd = outp
  i = LEN(dd)
  DO WHILE i >= 1
    IF MID$(dd, i, 1) = "/" THEN slash = i : EXIT DO
    i = i - 1
  LOOP
  IF slash > 1 THEN
    dd = LEFT$(dd, slash - 1)
    IF DIREXISTS(dd) = 0 THEN MKDIR dd
  END IF
  h = IO.FCREATE(outp)
  IO.FWRITE(h, text)
  IO.FCLOSE(h)
  hx = LOWER$(TOHEX(SHA256(text)))
  PRINT "scritto "; outp; ": "; LEN(text); " char (sha256="; LEFT$(hx, 16); ")"
  RETURN 0
END FUNCTION
