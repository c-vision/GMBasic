' create_gold_fixture.bas — fixture gold GSM8K/MATH (da create_gold_fixture.py).
' Uso: create_gold_fixture.bas <input.jsonl> <output.json> [dataset]
IMPORT IO
IMPORT JSON
IMPORT STRINGS

FUNCTION TrimSp(s AS STRING) AS STRING
  DIM a AS LONG, b AS LONG
  a = 1 : b = LEN(s)
  DO WHILE a <= b AND MID$(s, a, 1) = " " : a = a + 1 : LOOP
  DO WHILE b >= a AND MID$(s, b, 1) = " " : b = b - 1 : LOOP
  IF b < a THEN RETURN ""
  RETURN MID$(s, a, b - a + 1)
END FUNCTION

FUNCTION ExtractAnswer(s AS STRING) AS STRING
  DIM p AS LONG, t AS STRING, e AS LONG
  p = INSTR(s, "####")
  IF p > 0 THEN
    t = MID$(s, p + 4, LEN(s) - p - 3)
    RETURN TrimSp(t)
  END IF
  p = INSTR(s, "\boxed{")
  IF p > 0 THEN
    t = MID$(s, p + 7, LEN(s) - p - 6)
    e = INSTR(t, "}")
    IF e > 0 THEN t = LEFT$(t, e - 1)
    RETURN TrimSp(t)
  END IF
  ' fallback: ultimo numero (intero/decimale, segno incluso)
  DIM i AS LONG, st AS LONG, best AS STRING, c AS STRING, cur AS STRING, nd AS LONG
  i = 1 : st = 0 : best = "" : cur = "" : nd = 0
  DO WHILE i <= LEN(s)
    c = MID$(s, i, 1)
    IF (c >= "0" AND c <= "9") OR (c = "." AND st > 0) OR ((c = "-" OR c = "+") AND st = 0 AND i < LEN(s) AND MID$(s, i + 1, 1) >= "0" AND MID$(s, i + 1, 1) <= "9") THEN
      IF st = 0 THEN
        st = i
        cur = ""
        nd = 0
      END IF
      cur = cur + c
      IF c >= "0" AND c <= "9" THEN nd = nd + 1
    ELSE
      IF st > 0 THEN
        IF nd > 0 THEN best = cur
        st = 0 : cur = "" : nd = 0
      END IF
    END IF
    i = i + 1
  LOOP
  IF st > 0 AND nd > 0 THEN best = cur
  IF LEN(best) > 0 THEN RETURN best
  RETURN TrimSp(s)
END FUNCTION

function HasDigit(s AS STRING) AS INTEGER
  DIM i AS LONG, c AS STRING
  i = 1
  DO WHILE i <= LEN(s)
    c = MID$(s, i, 1)
    IF c >= "0" AND c <= "9" THEN RETURN 1
    i = i + 1
  LOOP
  RETURN 0
END FUNCTION

FUNCTION IsNumText(s AS STRING) AS INTEGER
  DIM i AS LONG, c AS STRING, dots AS LONG, digits AS LONG
  IF LEN(s) = 0 THEN RETURN 0
  dots = 0
  i = 1
  IF MID$(s, 1, 1) = "-" OR MID$(s, 1, 1) = "+" THEN i = 2
  DO WHILE i <= LEN(s)
    c = MID$(s, i, 1)
    IF c = "." THEN
      dots = dots + 1
      IF dots > 1 THEN RETURN 0
    ELSEIF c < "0" OR c > "9" THEN
      RETURN 0
    ELSE
      digits = digits + 1
    END IF
    i = i + 1
  LOOP
  IF digits = 0 THEN RETURN 0
  RETURN 1
END FUNCTION

FUNCTION NormalizeAnswer(s AS STRING) AS STRING
  DIM t AS STRING, d AS DOUBLE, u AS STRING
  t = LOWER$(ExtractAnswer(s))
  t = REPLACE(t, ",", "")
  t = REPLACE(t, " ", "")
  IF LEFT$(t, 1) = "." THEN t = "0" + t
  IF LEFT$(t, 2) = "-." THEN t = "-0" + MID$(t, 2)
  IF LEFT$(t, 2) = "+." THEN t = "+0" + MID$(t, 2)
  IF IsNumText(t) <> 0 THEN
    d = VAL(t)
    u = LOWER$(LTRIM$(STR$(d)))
    IF INSTR(u, ".") = 0 AND INSTR(u, "e") = 0 THEN u = u + ".0"
    RETURN u
  END IF
  RETURN t
END FUNCTION

FUNCTION Main() AS INTEGER
  DIM inp AS STRING, outp AS STRING, ds AS STRING
  DIM lines$() AS STRING, line AS STRING, o AS JSON, q AS STRING, a AS STRING, txt AS STRING
  DIM gt AS STRING, n AS LONG, i AS LONG, buf AS STRING, comma AS STRING
  IF COMMANDCOUNT() < 2 THEN
    PRINT "uso: create_gold_fixture.bas <input.jsonl> <output.json> [dataset]"
    RETURN 2
  END IF
  inp = COMMAND$(1) : outp = COMMAND$(2)
  IF COMMANDCOUNT() >= 3 THEN
    ds = COMMAND$(3)
  ELSE
    IF INSTR(LOWER$(inp), "gsm8k") > 0 THEN
      ds = "gsm8k"
    ELSE
      ds = "math500"
    END IF
  END IF
  txt = IO.READFILE(inp)
  lines$ = SPLIT(txt, CHR$(10))
  buf = "{" + CHR$(10) + "  " + JSON.STRINGIFY("dataset") + ": " + JSON.STRINGIFY(ds) + "," + CHR$(10)
  ' n_samples calcolato dopo; accumulo sample in un buffer separato
  DIM samples AS STRING
  samples = ""
  n = 0
  i = 0
  DO WHILE i <= UBOUND(lines$)
    line = TrimSp(lines$(i))
    IF LEN(line) > 0 THEN
      o = JSON.PARSE(line)
      q = "" : a = ""
      IF LEN(JSON.EXTRACT(o, "question")) > 0 THEN
        q = JSON.EXTRACT(o, "question")
        a = JSON.EXTRACT(o, "answer")
      ELSEIF LEN(JSON.EXTRACT(o, "problem")) > 0 THEN
        q = JSON.EXTRACT(o, "problem")
        a = JSON.EXTRACT(o, "answer")
      END IF
      IF LEN(q) > 0 THEN
        gt = NormalizeAnswer(a)
        IF n > 0 THEN samples = samples + "," + CHR$(10)
        samples = samples + "    {" + CHR$(10)
        samples = samples + "      " + JSON.STRINGIFY("question") + ": " + JSON.STRINGIFY(q) + "," + CHR$(10)
        samples = samples + "      " + JSON.STRINGIFY("ground_truth") + ": " + JSON.STRINGIFY(gt) + "," + CHR$(10)
        samples = samples + "      " + JSON.STRINGIFY("raw_ground_truth") + ": " + JSON.STRINGIFY(a) + CHR$(10)
        samples = samples + "    }"
        n = n + 1
      END IF
    END IF
    i = i + 1
  LOOP
  buf = buf + "  " + JSON.STRINGIFY("n_samples") + ": " + LTRIM$(STR$(n)) + "," + CHR$(10)
  buf = buf + "  " + JSON.STRINGIFY("samples") + ": [" + CHR$(10)
  buf = buf + samples + CHR$(10) + "  ]" + CHR$(10) + "}" + CHR$(10)
  DIM h AS LONG
  h = IO.FCREATE(outp)
  IO.FWRITE(h, buf)
  IO.FCLOSE(h)
  PRINT "Creato "; outp; " con "; n; " sample"
  RETURN 0
END FUNCTION
