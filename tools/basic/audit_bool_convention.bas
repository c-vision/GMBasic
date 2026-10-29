' audit_bool_convention.bas — bug hunter (GMBasic): funzioni BOOLEANE (BitType)
' il cui IlRuntime ritorna 1.0 invece del canonico GMBASIC -1.0.
' Incrocia StdLib (TokenKind + BitType), codegen IL (case TokenKind -> IlRuntime.M)
' e IlRuntime.cs (M => ... ? 1.0/-1.0 : 0.0). Nessun Python.
IMPORT IO
IMPORT GREP

DIM g_tk(2000) AS STRING
DIM g_name(2000) AS STRING
DIM g_bn AS INTEGER
DIM g_mtk(2000) AS STRING
DIM g_mname(2000) AS STRING
DIM g_mn AS INTEGER
DIM g_vname(2000) AS STRING
DIM g_val(2000) AS STRING
DIM g_vn AS INTEGER

FUNCTION ReadAll(path AS STRING) AS STRING
  DIM s AS STRING
  IF NOT FILEEXISTS(path) THEN RETURN ""
  OPEN path FOR INPUT AS #1
  s = IO.FINPUT(IO.LOF(1), 1)
  CLOSE #1
  ReadAll = s
END FUNCTION

SUB AddBool(tk AS STRING, nm AS STRING)
  DIM i AS INTEGER
  FOR i = 1 TO g_bn
    IF g_tk(i) = tk THEN EXIT SUB
  NEXT i
  IF g_bn < 2000 THEN
    g_bn = g_bn + 1
    g_tk(g_bn) = tk
    g_name(g_bn) = nm
  END IF
END SUB

SUB AddMap(tk AS STRING, mt AS STRING)
  DIM i AS INTEGER
  FOR i = 1 TO g_mn
    IF g_mtk(i) = tk THEN EXIT SUB
  NEXT i
  IF g_mn < 2000 THEN
    g_mn = g_mn + 1
    g_mtk(g_mn) = tk
    g_mname(g_mn) = mt
  END IF
END SUB

SUB AddVal(mt AS STRING, v AS STRING)
  DIM i AS INTEGER
  FOR i = 1 TO g_vn
    IF g_vname(i) = mt THEN EXIT SUB
  NEXT i
  IF g_vn < 2000 THEN
    g_vn = g_vn + 1
    g_vname(g_vn) = mt
    g_val(g_vn) = v
  END IF
END SUB

FUNCTION MapMethod(tk AS STRING) AS STRING
  DIM i AS INTEGER
  FOR i = 1 TO g_mn
    IF g_mtk(i) = tk THEN RETURN g_mname(i)
  NEXT i
  MapMethod = ""
END FUNCTION

FUNCTION MethodVal(mt AS STRING) AS STRING
  DIM i AS INTEGER
  FOR i = 1 TO g_vn
    IF g_vname(i) = mt THEN RETURN g_val(i)
  NEXT i
  MethodVal = ""
END FUNCTION

FUNCTION Main() AS INTEGER
  DIM croot AS STRING
  DIM pref AS STRING
  DIM files$()
  DIM f AS INTEGER
  DIM s AS STRING
  DIM p AS LONG
  DIM q AS LONG
  DIM r AS LONG
  DIM tk AS STRING
  DIM nm AS STRING
  DIM mt AS STRING
  DIM seg AS STRING
  DIM c AS STRING
  DIM nbool AS INTEGER
  DIM nok AS INTEGER
  DIM nsus AS INTEGER
  DIM nunk AS INTEGER

  croot = "../"
  IF FILEEXISTS(croot + "common/Ast/TokenKind.cs") THEN
    pref = croot
  ELSE
    pref = croot + "../GMBasicSharp/"
  END IF
  g_bn = 0 : g_mn = 0 : g_vn = 0

  ' 1) StdLib: T(TokenKind.X, "NAME") ... new BitType()
  files$ = GREP.FILES("TokenKind.", pref + "common/StdLib")
  FOR f = 0 TO UBOUND(files$)
    IF INSTR(files$(f), "/obj/") > 0 OR INSTR(files$(f), "/bin/") > 0 THEN
      s = ""
    ELSE
      s = ReadAll(files$(f))
    END IF
    p = 1
    DO
      p = INSTR(p, s, "TokenKind.")
      IF p = 0 THEN EXIT DO
      q = p + 10
      tk = ""
      DO WHILE q <= LEN(s)
        c = MID$(s, q, 1)
        IF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR (c >= "0" AND c <= "9") OR c = "_" THEN
          tk = tk + c : q = q + 1
        ELSE
          EXIT DO
        END IF
      LOOP
      IF LEN(tk) > 0 AND p > 3 AND INSTR(MID$(s, p - 3, 3), "T(") > 0 THEN
        r = INSTR(q, s, CHR$(34))
        IF r > 0 THEN
          q = r + 1
          nm = ""
          DO WHILE q <= LEN(s)
            c = MID$(s, q, 1)
            IF c = CHR$(34) THEN EXIT DO
            nm = nm + c : q = q + 1
          LOOP
          r = INSTR(q, s, "new NativeProcedureDefinition")
          IF r = 0 THEN r = LEN(s) + 1
          seg = MID$(s, q, r - q)
          IF INSTR(seg, "new BitType()") > 0 THEN AddBool(tk, nm)
        END IF
      END IF
      p = p + 10
    LOOP
  NEXT f

  ' 2) Codegen IL: case TokenKind.X: ... nameof(IlRuntime.M)
  files$ = GREP.FILES("case TokenKind.", pref + "common/IlBackendCodegen")
  FOR f = 0 TO UBOUND(files$)
    s = ReadAll(files$(f))
    p = 1
    DO
      p = INSTR(p, s, "case TokenKind.")
      IF p = 0 THEN EXIT DO
      q = p + 15
      tk = ""
      DO WHILE q <= LEN(s)
        c = MID$(s, q, 1)
        IF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR (c >= "0" AND c <= "9") OR c = "_" THEN
          tk = tk + c : q = q + 1
        ELSE
          EXIT DO
        END IF
      LOOP
      IF LEN(tk) > 0 THEN
        seg = MID$(s, q, 500)
        r = INSTR(seg, "nameof(IlRuntime.")
        IF r > 0 THEN
          r = r + 17
          mt = ""
          DO WHILE r <= LEN(seg)
            c = MID$(seg, r, 1)
            IF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR (c >= "0" AND c <= "9") OR c = "_" THEN
              mt = mt + c : r = r + 1
            ELSE
              EXIT DO
            END IF
          LOOP
          IF LEN(mt) > 0 THEN AddMap(tk, mt)
        END IF
      END IF
      p = p + 15
    LOOP
  NEXT f

  ' 3) IlRuntime: M(...) => ... ? 1.0|-1.0 : 0.0
  s = ReadAll(pref + "common/IlBackendRuntime/IlRuntime.cs")
  p = 1
  DO
    p = INSTR(p, s, "public static ")
    IF p = 0 THEN EXIT DO
    q = p + 15
    r = INSTR(q, s, "(")
    IF r = 0 THEN EXIT DO
    mt = ""
    q = r - 1
    DO WHILE q > p
      c = MID$(s, q, 1)
      IF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR (c >= "0" AND c <= "9") OR c = "_" THEN
        mt = c + mt : q = q - 1
      ELSE
        EXIT DO
      END IF
    LOOP
    IF LEN(mt) > 0 THEN
      seg = MID$(s, r, 1600)
      IF INSTR(seg, "? 1.0 : 0.0") > 0 THEN AddVal(mt, "1.0")
      IF INSTR(seg, "? -1.0 : 0.0") > 0 THEN AddVal(mt, "-1.0")
    END IF
    p = r + 1
  LOOP

  nbool = g_bn
  nok = 0 : nsus = 0 : nunk = 0
  PRINT "bool (StdLib BitType): "; nbool
  FOR f = 1 TO g_bn
    mt = MapMethod(g_tk(f))
    IF LEN(mt) = 0 THEN
      nunk = nunk + 1
    ELSE
      seg = MethodVal(mt)
      IF seg = "1.0" THEN
        nsus = nsus + 1
        PRINT "  SOSPETTO "; g_tk(f); " "; g_name(f); " -> "; mt
      ELSEIF seg = "-1.0" THEN
        nok = nok + 1
      END IF
    END IF
  NEXT f
  PRINT "  canonici -1.0: "; nok
  PRINT "  SOSPETTI  1.0: "; nsus
  PRINT "  non mappati: "; nunk
  RETURN nsus
END FUNCTION
