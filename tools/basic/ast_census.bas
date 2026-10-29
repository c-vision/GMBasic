' ast_census.bas — copertura dei COSTRUTTI DI LINGUAGGIO (AST) sui .bas.
' Equivalente del vecchio tools/ast_census.py, senza Python.
'
' Metodo: per ogni file .bas (minitests/samples) si tolgono i commenti e si costruisce
' l'insieme delle parole; ogni costrutto ha alternative di parole (|) e/o un matcher
' speciale (letterali/riga). Un costrutto e' coperto se almeno un file lo contiene.
'
' Uso: gmb ast_census.bas [--source all|minitests|samples] [--missing] [--covered]
'                          [--only NOME|CATEGORIA,...]
IMPORT IO
IMPORT GREP

DIM g_n AS LONG
DIM g_cn(200) AS STRING
DIM g_cc(200) AS STRING
DIM g_cw(200) AS STRING
DIM g_cx(200) AS LONG
DIM g_cov(200) AS LONG
DIM g_hit(200) AS INTEGER
DIM g_sel(200) AS INTEGER
DIM g_spec(40) AS INTEGER
DIM g_words AS DICT OF STRING, INTEGER = NEW DICT OF STRING, INTEGER

FUNCTION IsWord(c AS STRING) AS INTEGER
  IF c >= "A" AND c <= "Z" THEN RETURN -1
  IF c >= "a" AND c <= "z" THEN RETURN -1
  IF c >= "0" AND c <= "9" THEN RETURN -1
  IF c = "_" THEN RETURN -1
  RETURN 0
END FUNCTION

FUNCTION IsAlpha(c AS STRING) AS INTEGER
  IF c >= "A" AND c <= "Z" THEN RETURN -1
  IF c >= "a" AND c <= "z" THEN RETURN -1
  RETURN 0
END FUNCTION

SUB AddC(n AS STRING, cat AS STRING, w AS STRING, x AS LONG)
  g_n = g_n + 1
  g_cn(g_n) = n : g_cc(g_n) = cat : g_cw(g_n) = w : g_cx(g_n) = x
END SUB

SUB Catalog()
  AddC("If/Then/Else/ElseIf", "branching", "IF|ELSEIF|THEN", -1)
  AddC("Select/Case", "branching", "CASE", -1)
  AddC("Match", "branching", "MATCH", -1)
  AddC("With/EndWith", "branching", "WITH", -1)
  AddC("Using/EndUsing", "branching", "USING", -1)
  AddC("For/Next", "loops", "FOR|NEXT", -1)
  AddC("ForEach", "loops", "FOREACH", 20)
  AddC("While", "loops", "WHILE", -1)
  AddC("Do/Loop", "loops", "DO|LOOP", -1)
  AddC("Repeat/Until", "loops", "REPEAT|UNTIL", -1)
  AddC("Exit", "loop-flow", "EXIT", -1)
  AddC("Continue", "loop-flow", "CONTINUE", -1)
  AddC("Loop/LoopWhile/LoopUntil", "loops", "", 23)
  AddC("Goto/label", "control", "GOTO", 15)
  AddC("Gosub/Return", "control", "GOSUB|RETURN", -1)
  AddC("OnError/Resume", "control", "RESUME", 27)
  AddC("Try/Catch/Finally", "control", "TRY|CATCH|FINALLY", -1)
  AddC("Throw/Rethrow/Error", "control", "THROW|RETHROW|ERROR", -1)
  AddC("Dim/Declaration/Const", "declarations", "DIM|CONST|REDIM", -1)
  AddC("Sub/EndSub", "procedures", "SUB", -1)
  AddC("Function/EndFunction", "procedures", "FUNCTION", -1)
  AddC("Procedure", "procedures", "PROCEDURE", -1)
  AddC("Generator/Yield", "procedures", "GENERATOR|YIELD", -1)
  AddC("Delegate", "procedures", "DELEGATE", -1)
  AddC("Declare/Lib (FFI)", "procedures", "DECLARE|LIB", -1)
  AddC("Local/Static", "procedures", "LOCAL|STATIC", -1)
  AddC("Class/EndClass", "types", "CLASS", -1)
  AddC("Interface/EndInterface", "types", "INTERFACE", -1)
  AddC("Type/EndType (UDT)", "types", "TYPE", -1)
  AddC("Constructor", "types", "CONSTRUCTOR", -1)
  AddC("Destructor", "types", "DESTRUCTOR", -1)
  AddC("Property/EndProperty", "types", "PROPERTY", -1)
  AddC("Operator/EndOperator", "types", "OPERATOR", -1)
  AddC("Event", "types", "EVENT", -1)
  AddC("Enum/EndEnum", "types", "ENUM", -1)
  AddC("ArrayLiteral", "expr-literal", "", 0)
  AddC("DictLiteral", "expr-literal", "", 1)
  AddC("ListLiteral", "expr-literal", "", 2)
  AddC("SetComprehension", "expr-literal", "", 3)
  AddC("DictComprehension", "expr-literal", "", 4)
  AddC("ListComprehension", "expr-literal", "", 5)
  AddC("TupleLiteral", "expr-literal", "", 6)
  AddC("Lambda/Arrow", "expr", "LAMBDA", 7)
  AddC("New/NewExpr", "expr", "NEW", -1)
  AddC("Ternary/IIF", "expr", "IIF", 8)
  AddC("NullConditionalAccess", "expr", "", 9)
  AddC("ArraySlice", "expr", "", 10)
  AddC("InterpolatedString", "expr", "", 11)
  AddC("SwitchExpr", "expr", "SWITCH", -1)
  AddC("Await", "expr", "AWAIT", -1)
  AddC("SafeCast/Is", "expr", "", 12)
  AddC("TypeMember/FieldAccess", "expr", "", 13)
  AddC("IndexedAccess", "expr", "", 2)
  AddC("Data/Read/Restore", "statements", "DATA|READ|RESTORE", -1)
  AddC("Swap", "statements", "SWAP", -1)
  AddC("Let/Assign", "statements", "LET", 14)
  AddC("Stop/End", "statements", "STOP|END", -1)
  AddC("Run", "statements", "RUN", -1)
  AddC("Randomize", "statements", "RANDOMIZE", -1)
  AddC("Macro", "statements", "MACRO", -1)
  AddC("Label", "statements", "", 15)
  AddC("SystemShell", "statements", "SHELL", -1)
  AddC("ModuleImport/Require", "statements", "IMPORT|REQUIRE", -1)
  AddC("OnKey", "events", "", 25)
  AddC("OnSpriteCollision", "events", "", 22)
  AddC("GoProcedure/GoSub", "concurrency", "", 21)
  AddC("WaitGroup/Wait", "concurrency", "WAIT", -1)
  AddC("Channel/Send/Receive", "concurrency", "CHANNEL|SEND|RECEIVE", -1)
  AddC("Mutex/Lock", "concurrency", "MUTEX|LOCK|UNLOCK", -1)
  AddC("Cond/Signal/Broadcast", "concurrency", "COND|SIGNAL|BROADCAST", -1)
  AddC("Once", "concurrency", "ONCE", -1)
  AddC("Defer", "concurrency", "DEFER", -1)
  AddC("Sleep", "concurrency", "SLEEP", -1)
  AddC("SimpleEvent", "concurrency", "EVENT", -1)
  AddC("GoSelect", "concurrency", "", 26)
END SUB

FUNCTION WordIn(w AS STRING, set AS STRING) AS INTEGER
  IF INSTR("|" + set + "|", "|" + w + "|") > 0 THEN RETURN -1
  RETURN 0
END FUNCTION

' trova la parola word all'indice p (confini); ritorna posizione dopo la parola o 0
FUNCTION FindWord(u AS STRING, word AS STRING, p AS LONG) AS LONG
  DIM lw AS LONG, q AS LONG, c AS STRING
  lw = LEN(word)
  DO WHILE p <= LEN(u)
    p = INSTR(p, u, word)
    IF p = 0 THEN RETURN 0
    IF (p = 1 OR IsWord(MID$(u, p - 1, 1)) = 0) AND (p + lw > LEN(u) OR IsWord(MID$(u, p + lw, 1)) = 0) THEN
      RETURN p + lw
    END IF
    p = p + lw
  LOOP
  RETURN 0
END FUNCTION

FUNCTION WordSeq(u AS STRING, a AS STRING, b AS STRING) AS INTEGER
  DIM p AS LONG, q AS LONG
  p = 1
  DO
    p = FindWord(u, a, p)
    IF p = 0 THEN RETURN 0
    q = p
    DO WHILE q <= LEN(u) AND (MID$(u, q, 1) = " " OR MID$(u, q, 1) = CHR$(9)) : q = q + 1 : LOOP
    IF q + LEN(b) - 1 <= LEN(u) AND MID$(u, q, LEN(b)) = b THEN
      IF q + LEN(b) > LEN(u) OR IsWord(MID$(u, q + LEN(b), 1)) = 0 THEN RETURN -1
    END IF
  LOOP
END FUNCTION

FUNCTION WordSeq3(u AS STRING, a AS STRING, b AS STRING, c AS STRING) AS INTEGER
  DIM p AS LONG, q AS LONG, r AS LONG
  p = 1
  DO
    p = FindWord(u, a, p)
    IF p = 0 THEN RETURN 0
    q = p
    DO WHILE q <= LEN(u) AND (MID$(u, q, 1) = " " OR MID$(u, q, 1) = CHR$(9)) : q = q + 1 : LOOP
    IF q + LEN(b) - 1 <= LEN(u) AND MID$(u, q, LEN(b)) = b THEN
      r = q + LEN(b)
      DO WHILE r <= LEN(u) AND (MID$(u, r, 1) = " " OR MID$(u, r, 1) = CHR$(9)) : r = r + 1 : LOOP
      IF r + LEN(c) - 1 <= LEN(u) AND MID$(u, r, LEN(c)) = c THEN RETURN -1
    END IF
  LOOP
END FUNCTION

' indice della prima occorrenza di ch in u a partire da p (0 = assente)
FUNCTION At(u AS STRING, ch AS STRING, p AS LONG) AS LONG
  RETURN INSTR(p, u, ch)
END FUNCTION

FUNCTION Special(id AS LONG, u AS STRING) AS INTEGER
  DIM a AS LONG, b AS LONG, c AS LONG, d AS LONG, i AS LONG
  DIM x AS STRING, y AS STRING
  IF id = 0 OR id = 1 OR id = 3 OR id = 4 THEN
    a = At(u, "{", 1)
    DO WHILE a > 0
      b = At(u, "}", a + 1)
      IF b > 0 THEN
        x = MID$(u, a + 1, b - a - 1)
        IF INSTR(x, "{") = 0 AND INSTR(x, "}") = 0 THEN
          IF id = 0 THEN RETURN -1
          IF id = 1 AND INSTR(x, ":") > 0 THEN RETURN -1
          IF (id = 3 OR id = 4) THEN
            IF FindWord(x, "FOR", 1) <> 0 THEN
              IF id = 4 THEN
                IF INSTR(x, ":") > 0 THEN RETURN -1
              ELSE
                RETURN -1
              END IF
            END IF
          END IF
        END IF
      END IF
      a = At(u, "{", a + 1)
    LOOP
    RETURN 0
  END IF
  IF id = 2 OR id = 10 THEN
    a = At(u, "[", 1)
    DO WHILE a > 0
      b = At(u, "]", a + 1)
      IF b > 0 THEN
        x = MID$(u, a + 1, b - a - 1)
        IF LEN(x) > 0 AND INSTR(x, "[") = 0 AND INSTR(x, "]") = 0 THEN
          IF id = 2 THEN RETURN -1
          IF INSTR(x, ":") > 0 THEN RETURN -1
        END IF
      END IF
      a = At(u, "[", a + 1)
    LOOP
    RETURN 0
  END IF
  IF id = 5 THEN
    a = At(u, "[", 1)
    DO WHILE a > 0
      b = At(u, "]", a + 1)
      IF b > 0 THEN
        x = MID$(u, a + 1, b - a - 1)
        IF INSTR(x, "[") = 0 AND INSTR(x, "]") = 0 AND FindWord(x, "FOR", 1) <> 0 THEN RETURN -1
      END IF
      a = At(u, "[", a + 1)
    LOOP
    RETURN 0
  END IF
  IF id = 6 THEN
    a = At(u, "(", 1)
    DO WHILE a > 0
      b = At(u, ")", a + 1)
      IF b > 0 THEN
        x = MID$(u, a + 1, b - a - 1)
        IF INSTR(x, "(") = 0 AND INSTR(x, ")") = 0 AND INSTR(x, ",") > 0 THEN RETURN -1
      END IF
      a = At(u, "(", a + 1)
    LOOP
    RETURN 0
  END IF
  IF id = 7 THEN
    IF INSTR(u, "=>") > 0 THEN RETURN -1
    RETURN 0
  END IF
  IF id = 8 THEN
    IF INSTR(u, "IIF(") > 0 THEN RETURN -1
    a = At(u, "?", 1)
    DO WHILE a > 0
      b = At(u, ":", a + 1)
      IF b > a + 1 THEN RETURN -1
      a = At(u, "?", a + 1)
    LOOP
    RETURN 0
  END IF
  IF id = 9 THEN
    IF INSTR(u, "?.") > 0 OR INSTR(u, "?[") > 0 THEN RETURN -1
    RETURN 0
  END IF
  IF id = 11 THEN
    a = At(u, "$", 1)
    DO WHILE a > 0
      b = a + 1
      DO WHILE b <= LEN(u) AND (MID$(u, b, 1) = " " OR MID$(u, b, 1) = CHR$(9)) : b = b + 1 : LOOP
      IF b <= LEN(u) AND MID$(u, b, 1) = CHR$(34) THEN RETURN -1
      a = At(u, "$", a + 1)
    LOOP
    RETURN 0
  END IF
  IF id = 12 THEN
    a = FindWord(u, "IS", 1)
    DO WHILE a > 0
      b = a
      DO WHILE b <= LEN(u) AND (MID$(u, b, 1) = " " OR MID$(u, b, 1) = CHR$(9)) : b = b + 1 : LOOP
      IF b <= LEN(u) THEN
        x = MID$(u, b, 1)
        IF IsWord(x) <> 0 OR x = "." OR x = "$" THEN RETURN -1
      END IF
      a = FindWord(u, "IS", a)
    LOOP
    RETURN 0
  END IF
  IF id = 13 THEN
    a = At(u, ".", 1)
    DO WHILE a > 0
      IF a < LEN(u) THEN
        x = MID$(u, a + 1, 1)
        IF IsAlpha(x) <> 0 OR x = "_" THEN RETURN -1
      END IF
      a = At(u, ".", a + 1)
    LOOP
    RETURN 0
  END IF
  IF id = 14 OR id = 15 THEN
    a = 1
    DO WHILE a <= LEN(u) AND (MID$(u, a, 1) = " " OR MID$(u, a, 1) = CHR$(9)) : a = a + 1 : LOOP
    IF a > LEN(u) THEN RETURN 0
    IF IsAlpha(MID$(u, a, 1)) = 0 AND MID$(u, a, 1) <> "_" THEN RETURN 0
    b = a
    DO WHILE b <= LEN(u) AND (IsWord(MID$(u, b, 1)) <> 0 OR MID$(u, b, 1) = ".") : b = b + 1 : LOOP
    IF id = 15 THEN
      IF b <= LEN(u) AND MID$(u, b, 1) = ":" THEN RETURN -1
      RETURN 0
    END IF
    DO WHILE b <= LEN(u) AND (MID$(u, b, 1) = " " OR MID$(u, b, 1) = CHR$(9)) : b = b + 1 : LOOP
    IF b <= LEN(u) AND MID$(u, b, 1) = "=" THEN RETURN -1
    RETURN 0
  END IF
  IF id = 20 THEN
    IF WordSeq(u, "FOR", "EACH") <> 0 THEN RETURN -1
    RETURN 0
  END IF
  IF id = 21 THEN
    IF WordSeq(u, "GO", "SUB") <> 0 OR WordSeq(u, "GO", "PROCEDURE") <> 0 THEN RETURN -1
    RETURN 0
  END IF
  IF id = 22 THEN
    IF WordSeq3(u, "ON", "SPRITE", "COLLISION") <> 0 THEN RETURN -1
    RETURN 0
  END IF
  IF id = 23 THEN
    IF WordSeq(u, "LOOP", "WHILE") <> 0 OR WordSeq(u, "LOOP", "UNTIL") <> 0 THEN RETURN -1
    RETURN 0
  END IF
  IF id = 25 THEN
    IF WordSeq(u, "ON", "KEY") <> 0 THEN RETURN -1
    RETURN 0
  END IF
  IF id = 26 THEN
    IF WordSeq(u, "GO", "SELECT") <> 0 THEN RETURN -1
    RETURN 0
  END IF
  IF id = 27 THEN
    IF WordSeq(u, "ON", "ERROR") <> 0 THEN RETURN -1
    RETURN 0
  END IF
  RETURN 0
END FUNCTION

FUNCTION StripLine(line AS STRING) AS STRING
  DIM i AS LONG, c AS STRING, instr AS INTEGER, q AS STRING
  instr = 0 : q = ""
  FOR i = 1 TO LEN(line)
    c = MID$(line, i, 1)
    IF c = CHR$(34) THEN instr = 1 - instr
    IF c = "'" AND instr = 0 THEN EXIT FOR
    q = q + c
  NEXT i
  RETURN q
END FUNCTION

FUNCTION IsRem(u AS STRING) AS INTEGER
  DIM a AS LONG
  a = 1
  DO WHILE a <= LEN(u) AND (MID$(u, a, 1) = " " OR MID$(u, a, 1) = CHR$(9)) : a = a + 1 : LOOP
  IF MID$(u, a, 3) = "REM" THEN
    IF a + 3 > LEN(u) OR IsWord(MID$(u, a + 3, 1)) = 0 THEN RETURN -1
  END IF
  RETURN 0
END FUNCTION

SUB ScanFile(f AS STRING)
  DIM txt AS STRING, line AS STRING, u AS STRING, s AS STRING
  DIM parts$() AS STRING
  DIM wparts$() AS STRING
  DIM i AS LONG, b AS LONG, e AS LONG, c AS STRING, w AS STRING
  g_words = NEW DICT OF STRING, INTEGER
  txt = IO.READFILE(f)
  IF LEN(txt) = 0 THEN EXIT SUB
  parts$ = SPLIT(txt, CHR$(10))
  FOR b = 0 TO UBOUND(parts$)
    s = StripLine(parts$(b))
    u = UPPER(s)
    IF IsRem(u) = 0 AND LEN(u) > 0 THEN
      i = 1
      DO WHILE i <= LEN(u)
        c = MID$(u, i, 1)
        IF IsAlpha(c) <> 0 OR c = "_" THEN
          e = i
          DO WHILE e <= LEN(u)
            c = MID$(u, e, 1)
            IF IsWord(c) <> 0 THEN
              e = e + 1
            ELSE
              EXIT DO
            END IF
          LOOP
          w = MID$(u, i, e - i)
          g_words(w) = 1
          i = e
        ELSE
          i = i + 1
        END IF
      LOOP
    END IF
  NEXT b
  FOR i = 1 TO g_n
    g_hit(i) = 0
  NEXT i
  FOR i = 1 TO g_n
    IF LEN(g_cw(i)) > 0 THEN
      wparts$ = SPLIT(g_cw(i), "|")
      FOR b = 0 TO UBOUND(wparts$)
        IF wparts$(b) IN g_words THEN g_hit(i) = -1 : EXIT FOR
      NEXT b
    END IF
  NEXT i
  FOR i = 1 TO 40
    g_spec(i) = 0
  NEXT i
  FOR b = 0 TO UBOUND(parts$)
  NEXT b
  ' secondo passaggio: matcher speciali per riga
  FOR b = 0 TO UBOUND(parts$)
    s = StripLine(parts$(b))
    u = UPPER(s)
    IF IsRem(u) = 0 THEN
      FOR i = 1 TO g_n
        IF g_cx(i) >= 0 AND g_spec(g_cx(i) + 1) = 0 THEN
          IF Special(g_cx(i), u) <> 0 THEN g_spec(g_cx(i) + 1) = -1
        END IF
      NEXT i
    END IF
  NEXT b
  FOR i = 1 TO g_n
    IF g_hit(i) = 0 AND g_cx(i) >= 0 THEN
      IF g_spec(g_cx(i) + 1) <> 0 THEN g_hit(i) = -1
    END IF
    IF g_hit(i) <> 0 THEN g_cov(i) = g_cov(i) + 1
  NEXT i
END SUB

SUB SortRows()
  DIM i AS LONG, j AS LONG, n AS STRING, c AS STRING, v AS LONG, s AS INTEGER
  FOR i = 2 TO g_n
    n = g_cn(i) : c = g_cc(i) : v = g_cov(i) : s = g_sel(i)
    j = i - 1
    DO WHILE j >= 1 AND g_cov(j) > v
      g_cn(j + 1) = g_cn(j) : g_cc(j + 1) = g_cc(j) : g_cov(j + 1) = g_cov(j) : g_sel(j + 1) = g_sel(j)
      j = j - 1
    LOOP
    g_cn(j + 1) = n : g_cc(j + 1) = c : g_cov(j + 1) = v : g_sel(j + 1) = s
  NEXT i
END SUB

FUNCTION Main() AS INTEGER
  DIM root AS STRING, repo AS STRING, arg AS STRING, source AS STRING
  DIM only AS STRING, onlyCat AS STRING, onlyName AS STRING
  DIM missingOnly AS LONG, coveredOnly AS LONG
  DIM i AS LONG, up AS LONG, nf AS LONG, ci AS LONG, nsel AS LONG, b AS LONG
  DIM ot$() AS STRING
  DIM roots$() AS STRING
  DIM bn AS STRING, bc AS STRING, bv AS LONG
  DIM nmiss AS LONG, cmiss(30) AS STRING, cn(30) AS LONG
  DIM ccnt AS LONG, found AS LONG
  source = "all" : only = "" : missingOnly = 0 : coveredOnly = 0
  i = 0
  DO WHILE i < COMMANDCOUNT()
    i = i + 1
    arg = COMMAND$(i)
    IF arg = "--source" AND i < COMMANDCOUNT() THEN
      i = i + 1 : source = COMMAND$(i)
    ELSEIF arg = "--only" AND i < COMMANDCOUNT() THEN
      i = i + 1 : only = UPPER(COMMAND$(i))
    ELSEIF arg = "--missing" THEN
      missingOnly = -1
    ELSEIF arg = "--covered" THEN
      coveredOnly = -1
    END IF
  LOOP
  repo = IO.CWD()
  FOR up = 1 TO 6
    IF DIREXISTS(repo + "/GMBasicSharp/common") THEN EXIT FOR
    repo = IO.PARENTDIR(repo)
  NEXT up
  Catalog()
  nsel = 0
  FOR i = 1 TO g_n
    g_sel(i) = -1
    IF LEN(only) > 0 THEN
      g_sel(i) = 0
      ot$ = SPLIT(only, ",")
      FOR b = 0 TO UBOUND(ot$)
        IF UPPER(g_cn(i)) = ot$(b) OR UPPER(g_cc(i)) = ot$(b) THEN g_sel(i) = -1
      NEXT b
    END IF
    IF g_sel(i) <> 0 THEN nsel = nsel + 1
  NEXT i
  nf = 0
  IF source = "all" OR source = "minitests" THEN
    roots$ = GREP.FILES(" ", repo + "/shared/minitests")
    FOR i = 0 TO UBOUND(roots$)
      IF RIGHT$(UPPER(roots$(i)), 4) = ".BAS" THEN ScanFile(roots$(i)) : nf = nf + 1
    NEXT i
  END IF
  IF source = "all" OR source = "samples" THEN
    roots$ = GREP.FILES(" ", repo + "/shared/samples")
    FOR i = 0 TO UBOUND(roots$)
      IF RIGHT$(UPPER(roots$(i)), 4) = ".BAS" THEN ScanFile(roots$(i)) : nf = nf + 1
    NEXT i
  END IF
  SortRows()
  PRINT "ast_census — costrutti di linguaggio (sorgente: "; source; ")"
  PRINT "  file scan: "; nf; "   costrutti: "; nsel
  IF LEN(only) > 0 THEN PRINT "  filtro: "; only
  PRINT ""
  FOR ci = 1 TO g_n
    nmiss = 0
    bn = g_cn(ci) : bc = g_cc(ci) : bv = g_cov(ci)
    IF g_sel(ci) = 0 THEN nmiss = 1
    IF missingOnly <> 0 AND bv > 0 THEN
      nmiss = 1
    END IF
    IF coveredOnly <> 0 AND bv = 0 THEN
      nmiss = 1
    END IF
    IF LEN(only) > 0 THEN
      IF UPPER(bn) <> only AND UPPER(bc) <> only THEN nmiss = 1
    END IF
    IF nmiss = 0 THEN
      IF bv > 0 THEN
        PRINT "  [OK ] "; bn; " "; bc; " "; bv; " file"
      ELSE
        PRINT "  [MISS] "; bn; " "; bc; " "; bv; " file"
      END IF
    END IF
  NEXT ci
  PRINT ""
  nmiss = 0
  FOR ci = 1 TO g_n
    IF g_sel(ci) <> 0 AND g_cov(ci) = 0 THEN nmiss = nmiss + 1
  NEXT ci
  PRINT "TOTALE mancanti: "; nmiss; "/"; nsel
  FOR ci = 1 TO 30
    cmiss(ci) = "" : cn(ci) = 0
  NEXT ci
  ccnt = 0
  FOR ci = 1 TO g_n
    IF g_sel(ci) <> 0 AND g_cov(ci) = 0 THEN
      found = 0
      FOR i = 1 TO ccnt
        IF cmiss(i) = g_cc(ci) THEN cn(i) = cn(i) + 1 : found = 1
      NEXT i
      IF found = 0 AND ccnt < 30 THEN
        ccnt = ccnt + 1 : cmiss(ccnt) = g_cc(ci) : cn(ccnt) = 1
      END IF
    END IF
  NEXT ci
  FOR i = 1 TO ccnt
    IF cn(i) > 0 THEN PRINT "  - "; cmiss(i); ": "; cn(i); " mancanti"
  NEXT i
  IF nmiss = 0 THEN RETURN 0
  RETURN 1
END FUNCTION
