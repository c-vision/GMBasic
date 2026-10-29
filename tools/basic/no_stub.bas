' no_stub.bas — gate anti-stub (equivalente del vecchio audit/no_stub.py, senza Python).
'
' Rileva handler DICHIARATI ma senza lavoro reale:
'   EMPTY / GUARD_ONLY / STUB_ZERO (corpo vuoto, sola guardia, return costante)
'   NOT_IMPLEMENTED (IL), NOOP_UNSUPPORTED (C99Native), CODICE_NON_DISPATCHATO (VM/C),
'   PUSH_COSTANTE (syscall VM/C), TABELLA_SENZA_DISPATCH (C99Native, reported).
'
' Uso: gmb no_stub.bas [--quiet] [--list]
' Exit: 1 se esiste almeno una COLPA (binding), 0 altrimenti.
IMPORT IO
IMPORT GREP

DIM g_n AS LONG
DIM g_ib(500) AS STRING
DIM g_if(500) AS STRING
DIM g_ih(500) AS STRING
DIM g_it(500) AS STRING
DIM g_root AS STRING

FUNCTION StripC(s AS STRING) AS STRING
  DIM i AS LONG, n AS LONG, c AS STRING, q AS STRING, instr AS INTEGER
  n = LEN(s) : i = 1 : q = "" : instr = 0
  DO WHILE i <= n
    c = MID$(s, i, 1)
    IF c = CHR$(34) THEN instr = 1 - instr
    IF instr = 0 AND c = "/" AND i < n THEN
      IF MID$(s, i + 1, 1) = "*" THEN
        i = i + 2
        DO WHILE i < n
          IF MID$(s, i, 1) = "*" AND MID$(s, i + 1, 1) = "/" THEN i = i + 2 : EXIT DO
          i = i + 1
        LOOP
      ELSEIF MID$(s, i + 1, 1) = "/" THEN
        EXIT DO
      ELSE
        q = q + c : i = i + 1
      END IF
    ELSE
      q = q + c : i = i + 1
    END IF
  LOOP
  RETURN q
END FUNCTION

FUNCTION Norm(s AS STRING) AS STRING
  DIM i AS LONG, c AS STRING, q AS STRING
  q = ""
  FOR i = 1 TO LEN(s)
    c = MID$(s, i, 1)
    IF c <> " " AND c <> CHR$(9) AND c <> CHR$(10) AND c <> CHR$(13) THEN q = q + c
  NEXT i
  RETURN q
END FUNCTION

FUNCTION Lower(s AS STRING) AS STRING
  DIM i AS LONG, c AS STRING, q AS STRING
  q = ""
  FOR i = 1 TO LEN(s)
    c = MID$(s, i, 1)
    IF c >= "A" AND c <= "Z" THEN c = CHR$(ASC(c) + 32)
    q = q + c
  NEXT i
  RETURN q
END FUNCTION

FUNCTION Rel(p AS STRING) AS STRING
  IF LEFT$(p, LEN(g_root) + 1) = g_root + "/" THEN RETURN MID$(p, LEN(g_root) + 2)
  RETURN p
END FUNCTION

SUB AddIssue(backend AS STRING, file AS STRING, handler AS STRING, tipo AS STRING)
  IF g_n >= 500 THEN EXIT SUB
  g_n = g_n + 1
  g_ib(g_n) = backend : g_if(g_n) = Rel(file) : g_ih(g_n) = handler : g_it(g_n) = tipo
END SUB

FUNCTION Binding(t AS STRING) AS INTEGER
  IF t = "EMPTY" OR t = "GUARD_ONLY" OR t = "STUB_ZERO" THEN RETURN -1
  IF t = "CODICE_NON_DISPATCHATO" OR t = "PUSH_COSTANTE" OR t = "NOT_IMPLEMENTED" THEN RETURN -1
  RETURN 0
END FUNCTION

FUNCTION IdAfter(s AS STRING, p AS LONG) AS STRING
  DIM q AS LONG, c AS STRING, r AS STRING
  DO WHILE p <= LEN(s) AND (MID$(s, p, 1) = " " OR MID$(s, p, 1) = CHR$(9)) : p = p + 1 : LOOP
  q = p
  DO WHILE q <= LEN(s)
    c = MID$(s, q, 1)
    IF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") OR (c >= "0" AND c <= "9") OR c = "_" THEN
      q = q + 1
    ELSE
      EXIT DO
    END IF
  LOOP
  RETURN MID$(s, p, q - p)
END FUNCTION

' Nome della funzione che termina subito prima della '(' alla colonna col.
FUNCTION FuncNameAt(line AS STRING, col AS LONG) AS STRING
  DIM i AS LONG, e AS LONG
  i = INSTR(col, line, "(")
  IF i = 0 THEN RETURN ""
  e = i
  DO WHILE e > 1
    IF (MID$(line, e - 1, 1) >= "A" AND MID$(line, e - 1, 1) <= "Z") OR (MID$(line, e - 1, 1) >= "a" AND MID$(line, e - 1, 1) <= "z") OR (MID$(line, e - 1, 1) >= "0" AND MID$(line, e - 1, 1) <= "9") OR MID$(line, e - 1, 1) = "_" THEN
      e = e - 1
    ELSE
      EXIT DO
    END IF
  LOOP
  RETURN MID$(line, e, i - e)
END FUNCTION

FUNCTION ColOf(hit AS STRING) AS LONG
  DIM p1 AS LONG, p2 AS LONG
  p1 = INSTR(hit, ":")
  p2 = INSTR(p1 + 1, hit, ":")
  RETURN VAL(MID$(hit, p2 + 1))
END FUNCTION

FUNCTION LineOf(hit AS STRING) AS LONG
  DIM p1 AS LONG
  p1 = INSTR(hit, ":")
  RETURN VAL(MID$(hit, p1 + 1))
END FUNCTION

FUNCTION HitLine(hit AS STRING) AS STRING
  DIM p AS LONG
  p = INSTR(hit, ":")
  p = INSTR(p + 1, hit, ":")
  p = INSTR(p + 1, hit, ":")
  IF p = 0 THEN RETURN ""
  RETURN MID$(hit, p + 1)
END FUNCTION

' corpo { } della funzione `name` nel sorgente
FUNCTION BodyOf(src AS STRING, name AS STRING) AS STRING
  DIM p AS LONG, q AS LONG, r AS LONG, depth AS LONG, c AS STRING
  p = 1
  DO WHILE p <= LEN(src)
    p = INSTR(p, src, name)
    IF p = 0 THEN RETURN ""
    q = p + LEN(name)
    DO WHILE q <= LEN(src) AND (MID$(src, q, 1) = " " OR MID$(src, q, 1) = CHR$(9) OR MID$(src, q, 1) = CHR$(10) OR MID$(src, q, 1) = CHR$(13)) : q = q + 1 : LOOP
    IF q <= LEN(src) AND MID$(src, q, 1) = "(" THEN
      depth = 0 : r = q
      DO WHILE r <= LEN(src)
        c = MID$(src, r, 1)
        IF c = "(" THEN depth = depth + 1
        IF c = ")" THEN
          depth = depth - 1
          IF depth = 0 THEN r = r + 1 : EXIT DO
        END IF
        r = r + 1
      LOOP
      DO WHILE r <= LEN(src) AND (MID$(src, r, 1) = " " OR MID$(src, r, 1) = CHR$(9) OR MID$(src, r, 1) = CHR$(10) OR MID$(src, r, 1) = CHR$(13)) : r = r + 1 : LOOP
      IF r <= LEN(src) AND MID$(src, r, 1) = "{" THEN
        depth = 1 : q = r + 1
        DO WHILE q <= LEN(src) AND depth > 0
          c = MID$(src, q, 1)
          IF c = "{" THEN depth = depth + 1
          IF c = "}" THEN depth = depth - 1
          q = q + 1
        LOOP
        RETURN MID$(src, r + 1, q - r - 2)
      END IF
    END IF
    p = p + LEN(name)
  LOOP
  RETURN ""
END FUNCTION

SUB ClassifyBody(body AS STRING, handler AS STRING, backend AS STRING, file AS STRING)
  DIM n AS STRING
  n = Norm(StripC(body))
  IF LEN(n) = 0 THEN AddIssue(backend, file, handler, "EMPTY") : EXIT SUB
  IF n = "GML_VM_REQUIRE();" THEN AddIssue(backend, file, handler, "GUARD_ONLY") : EXIT SUB
  IF Lower(n) = "return0;" OR Lower(n) = "return1;" OR Lower(n) = "returnnull;" THEN
    AddIssue(backend, file, handler, "STUB_ZERO") : EXIT SUB
  END IF
  IF n = "bout->result=0;" OR n = "bout->result=0.0;" THEN AddIssue(backend, file, handler, "STUB_ZERO")
END SUB

SUB CheckCFile(path AS STRING, pattern AS STRING, backend AS STRING)
  DIM src AS STRING, h AS LONG, line AS STRING, name AS STRING, body AS STRING
  DIM hits$() AS STRING
  src = IO.READFILE(path)
  IF LEN(src) = 0 THEN EXIT SUB
  hits$ = GREP.MATCHES(pattern, path)
  IF UBOUND(hits$) < 0 THEN EXIT SUB
  FOR h = 0 TO UBOUND(hits$)
    line = HitLine(hits$(h))
    name = FuncNameAt(line, ColOf(hits$(h)))
    IF LEN(name) > 0 THEN
      body = BodyOf(src, name)
      IF LEN(body) > 0 THEN ClassifyBody(body, name, backend, path)
    END IF
  NEXT h
END SUB

SUB CheckC99Statements(path AS STRING)
  DIM src AS STRING
  DIM lines$() AS STRING
  DIM i AS LONG, k AS LONG, j AS LONG, t AS STRING, name AS STRING, body AS STRING, n AS STRING
  src = IO.READFILE(path)
  IF LEN(src) = 0 THEN EXIT SUB
  lines$ = SPLIT(src, CHR$(10))
  FOR i = 0 TO UBOUND(lines$)
    t = LTRIM$(lines$(i))
    IF LEFT$(t, 14) = "case GMB_STMT_" THEN
      k = INSTR(t, ":")
      IF k > 0 THEN
        name = "GMB_STMT_" + MID$(t, 15, k - 15)
        body = MID$(t, k + 1)
        j = i + 1
        DO WHILE j <= UBOUND(lines$)
          t = LTRIM$(lines$(j))
          IF LEFT$(t, 14) = "case GMB_STMT_" OR LEFT$(t, 8) = "default:" OR LEFT$(t, 1) = "}" THEN EXIT DO
          body = body + CHR$(10) + lines$(j)
          j = j + 1
        LOOP
        n = Norm(StripC(body))
        IF LEFT$(n, 9) = "c99_fail(" THEN
          AddIssue("C99Native", path, name, "NOOP_UNSUPPORTED")
        END IF
      END IF
    END IF
  NEXT i
END SUB

SUB CheckC99Table()
  DIM tbl AS STRING, emit AS STRING, src AS STRING
  DIM files$() AS STRING
  DIM lines$() AS STRING
  DIM i AS LONG, j AS LONG, k AS LONG, p AS LONG, q AS LONG, n AS LONG
  DIM name AS STRING, tok AS STRING, bare AS STRING, f AS STRING
  DIM bdir AS STRING
  bdir = g_root + "/GMBasic/native/gmb_c99native"
  tbl = IO.READFILE(bdir + "/include/gmb_c99_emit_builtin.h")
  IF LEN(tbl) = 0 THEN EXIT SUB
  files$ = GREP.FILES(" ", bdir + "/src")
  IF UBOUND(files$) >= 0 THEN
  FOR i = 0 TO UBOUND(files$)
    IF INSTR(files$(i), "gmb_c99_emit") > 0 AND RIGHT$(files$(i), 2) = ".c" THEN emit = emit + CHR$(10) + IO.READFILE(files$(i))
  NEXT i
  END IF
  lines$ = SPLIT(tbl, CHR$(10))
  FOR i = 0 TO UBOUND(lines$)
    j = INSTR(lines$(i), "{")
    IF j > 0 THEN
      p = INSTR(j, lines$(i), CHR$(34))
      IF p > 0 THEN
        q = INSTR(p + 1, lines$(i), CHR$(34))
        IF q > p THEN
          name = MID$(lines$(i), p + 1, q - p - 1)
          IF LEN(name) > 0 THEN
            bare = name
            IF RIGHT$(bare, 1) = "$" THEN bare = LEFT$(bare, LEN(bare) - 1)
            tok = ""
            k = INSTR(q, lines$(i), "GMB_TOK_")
            IF k > 0 THEN
              n = k + 8
              DO WHILE n <= LEN(lines$(i))
                IF (MID$(lines$(i), n, 1) >= "A" AND MID$(lines$(i), n, 1) <= "Z") OR (MID$(lines$(i), n, 1) >= "0" AND MID$(lines$(i), n, 1) <= "9") OR MID$(lines$(i), n, 1) = "_" THEN
                  n = n + 1
                ELSE
                  EXIT DO
                END IF
              LOOP
              tok = MID$(lines$(i), k + 8, n - k - 8)
            END IF
            IF INSTR(emit, CHR$(34) + bare + CHR$(34)) = 0 THEN
              IF tok = "" OR INSTR(emit, "GMB_TOK_" + tok) = 0 THEN
                AddIssue("C99Native", bdir + "/include/gmb_c99_emit_builtin.h", name, "TABELLA_SENZA_DISPATCH")
              END IF
            END IF
          END IF
        END IF
      END IF
    END IF
  NEXT i
END SUB

SUB CheckCases(path AS STRING, marker AS STRING, backend AS STRING)
  DIM src AS STRING
  DIM lines$() AS STRING
  DIM i AS LONG, j AS LONG, k AS LONG, t AS STRING, name AS STRING, body AS STRING, n AS STRING
  DIM hasemit AS INTEGER
  src = IO.READFILE(path)
  IF LEN(src) = 0 THEN EXIT SUB
  lines$ = SPLIT(src, CHR$(10))
  FOR i = 0 TO UBOUND(lines$)
    t = LTRIM$(lines$(i))
    IF LEFT$(t, LEN(marker)) = marker THEN
      k = INSTR(t, ":")
      IF k > 0 THEN
        name = MID$(t, LEN(marker) + 1, k - LEN(marker) - 1)
        body = MID$(t, k + 1)
        j = i + 1
        DO WHILE j <= UBOUND(lines$)
          t = LTRIM$(lines$(j))
          IF LEFT$(t, LEN(marker)) = marker OR LEFT$(t, 8) = "default:" OR LEFT$(t, 1) = "}" THEN EXIT DO
          body = body + CHR$(10) + lines$(j)
          j = j + 1
        LOOP
        n = Norm(body)
        IF backend = "PCodeVM" THEN
          IF n = "return0;" OR n = "returnnull;" OR INSTR(body, "NotImplementedException") > 0 OR n = "break;" THEN
            AddIssue(backend, path, name, "STUB_ZERO")
          END IF
        ELSE
          hasemit = 0
          IF INSTR(n, "Emit") > 0 OR INSTR(n, "IlRuntime") > 0 OR INSTR(n, "Method") > 0 OR INSTR(n, "_il.") > 0 OR INSTR(n, "args") > 0 OR INSTR(n, "switch") > 0 THEN hasemit = -1
          IF INSTR(body, "NotImplementedException") > 0 AND hasemit = 0 THEN
            AddIssue(backend, path, name, "NOT_IMPLEMENTED")
          END IF
        END IF
      END IF
    END IF
  NEXT i
END SUB

SUB CheckCSharpDir(dir AS STRING, prefix AS STRING, marker AS STRING, backend AS STRING)
  DIM i AS LONG
  DIM files$() AS STRING
  files$ = GREP.FILES(" ", dir)
  IF UBOUND(files$) < 0 THEN EXIT SUB
  FOR i = 0 TO UBOUND(files$)
    IF RIGHT$(UPPER(files$(i)), 3) = ".CS" THEN
      IF INSTR(files$(i), "/obj/") = 0 AND INSTR(files$(i), "/bin/") = 0 THEN
        IF INSTR(files$(i), prefix) > 0 THEN CheckCases(files$(i), marker, backend)
      END IF
    END IF
  NEXT i
END SUB

FUNCTION ElseZero(body AS STRING) AS INTEGER
  DIM n AS STRING
  n = Norm(StripC(body))
  IF INSTR(n, "else{bout->result=0") > 0 THEN RETURN -1
  IF INSTR(n, "else{return0") > 0 THEN RETURN -1
  IF INSTR(n, "elsebout->result=0") > 0 THEN RETURN -1
  IF INSTR(n, "elsereturn0") > 0 THEN RETURN -1
  RETURN 0
END FUNCTION

SUB CheckVmcTable()
  DIM path AS STRING, src AS STRING
  DIM lines$() AS STRING
  DIM handlers(5000) AS STRING
  DIM codes(5000) AS STRING
  DIM nh AS LONG, i AS LONG, j AS LONG, p AS LONG, q AS LONG, k AS LONG
  DIM name AS STRING, cnum AS STRING, body AS STRING, n AS STRING, full AS STRING
  DIM h AS LONG, ci AS LONG, tested AS INTEGER
  DIM cs$() AS STRING
  path = g_root + "/GMBasic/native/gmb_vm_c/gmb_builtin.c"
  src = IO.READFILE(path)
  IF LEN(src) = 0 THEN EXIT SUB
  lines$ = SPLIT(src, CHR$(10))
  nh = 0
  FOR i = 0 TO UBOUND(lines$)
    p = INSTR(lines$(i), "{")
    IF p > 0 THEN
      q = INSTR(p + 1, lines$(i), ",")
      IF q > p THEN
        cnum = MID$(lines$(i), p + 1, q - p - 1)
        cnum = Norm(cnum)
        IF LEN(cnum) > 0 AND cnum >= "0" AND cnum <= "9" THEN
          k = INSTR(q + 1, lines$(i), ",")
          IF k > q THEN
            name = Norm(MID$(lines$(i), q + 1, k - q - 1))
            IF LEFT$(name, 7) = "gmb_bf_" THEN
              j = 0
              FOR h = 1 TO nh
                IF handlers(h) = name THEN j = h
              NEXT h
              IF j = 0 THEN
                nh = nh + 1 : handlers(nh) = name : codes(nh) = cnum
              ELSE
                codes(j) = codes(j) + "," + cnum
              END IF
            END IF
          END IF
        END IF
      END IF
    END IF
  NEXT i
  FOR h = 1 TO nh
    IF INSTR(codes(h), ",") > 0 THEN
      body = BodyOf(src, handlers(h))
      IF LEN(body) > 0 THEN
        n = Norm(StripC(body))
        IF INSTR(n, "func_code==") > 0 OR INSTR(n, "func_code!=") > 0 OR INSTR(n, "case") > 0 THEN
          IF ElseZero(body) <> 0 THEN
            cs$ = SPLIT(codes(h), ",")
            FOR ci = 0 TO UBOUND(cs$)
              tested = 0
              IF INSTR(n, "func_code==" + cs$(ci)) > 0 OR INSTR(n, "func_code!=" + cs$(ci)) > 0 THEN tested = -1
              IF INSTR(n, "case" + cs$(ci) + ":") > 0 THEN tested = -1
              IF tested = 0 THEN
                full = handlers(h) + "[" + cs$(ci) + "]"
                AddIssue("VMC", path, full, "CODICE_NON_DISPATCHATO")
              END IF
            NEXT ci
          END IF
        END IF
      END IF
    END IF
  NEXT h
END SUB

FUNCTION AllowedCall(w AS STRING) AS INTEGER
  IF w = "vm_pop_str" OR w = "vm_pop_num" OR w = "vm_pop_int" OR w = "vm_pop" THEN RETURN -1
  IF w = "gmb_task_pop" OR w = "gmb_obj_release" THEN RETURN -1
  IF w = "push_num_entry" OR w = "push_str_entry" OR w = "push_ref_entry" OR w = "push_nil_entry" THEN RETURN -1
  IF w = "strlen" OR w = "sizeof" OR w = "snprintf" OR w = "if" OR w = "while" OR w = "for" OR w = "switch" OR w = "return" OR w = "defined" THEN RETURN -1
  RETURN 0
END FUNCTION

SUB CheckVmcPush()
  DIM dir AS STRING, f AS STRING, src AS STRING
  DIM files$() AS STRING
  DIM i AS LONG, h AS LONG, p AS LONG, q AS LONG, c AS STRING, w AS STRING
  DIM body AS STRING, n AS STRING, name AS STRING, line AS STRING
  DIM hits$() AS STRING
  DIM extra AS INTEGER
  DIM calls$() AS STRING
  dir = g_root + "/GMBasic/native/gmb_vm_c"
  files$ = GREP.FILES(" ", dir)
  FOR i = 0 TO UBOUND(files$)
    f = files$(i)
    IF INSTR(f, "gmb_syscall_") > 0 AND RIGHT$(f, 2) = ".c" THEN
      src = IO.READFILE(f)
      hits$ = GREP.MATCHES("(?:static\s+)?int\s+(gmb_\w+)\s*\([^)]*\)", f)
      FOR h = 0 TO UBOUND(hits$)
        line = HitLine(hits$(h))
        name = FuncNameAt(line, ColOf(hits$(h)))
        IF LEN(name) > 0 THEN
          body = BodyOf(src, name)
          IF LEN(body) > 0 THEN
            n = Norm(StripC(body))
            IF (INSTR(n, "push_num_entry(task,0,") > 0 OR INSTR(n, "push_num_entry(task,1,") > 0 OR INSTR(n, "push_str_entry(task," + CHR$(34) + CHR$(34)) > 0 OR INSTR(n, "push_nil_entry(task") > 0) THEN
              extra = 0
              p = 1
              DO WHILE p <= LEN(n)
                c = MID$(n, p, 1)
                IF (c >= "a" AND c <= "z") OR c = "_" THEN
                  q = p
                  DO WHILE q <= LEN(n)
                    c = MID$(n, q, 1)
                    IF (c >= "a" AND c <= "z") OR (c >= "0" AND c <= "9") OR c = "_" THEN
                      q = q + 1
                    ELSE
                      EXIT DO
                    END IF
                  LOOP
                  w = MID$(n, p, q - p)
                  IF q <= LEN(n) AND MID$(n, q, 1) = "(" THEN
                    IF AllowedCall(w) = 0 THEN extra = -1
                  END IF
                  p = q
                ELSE
                  p = p + 1
                END IF
              LOOP
              IF extra = 0 THEN AddIssue("VMC", f, name, "PUSH_COSTANTE")
            END IF
          END IF
        END IF
      NEXT h
    END IF
  NEXT i
END SUB

FUNCTION Main() AS INTEGER
  DIM repo AS STRING, arg AS STRING, quiet AS LONG, list AS LONG, selftest AS LONG
  DIM i AS LONG, up AS LONG, nb AS LONG, nr AS LONG, h AS LONG
  DIM bdir AS STRING
  DIM files$() AS STRING
  quiet = 0 : list = 0 : selftest = 0 : i = 0
  DO WHILE i < COMMANDCOUNT()
    i = i + 1
    arg = COMMAND$(i)
    IF arg = "--quiet" THEN quiet = -1
    IF arg = "--list" THEN list = -1
    IF arg = "--selftest" THEN selftest = -1
    IF arg = "--json" THEN
      PRINT "--json non supportato (usare --list)"
      RETURN 2
    END IF
  LOOP
  repo = IO.CWD()
  FOR up = 1 TO 7
    IF DIREXISTS(repo + "/GMBasicSharp/common") AND DIREXISTS(repo + "/GMBasic/native") THEN EXIT FOR
    repo = IO.PARENTDIR(repo)
  NEXT up
  g_root = repo
  IF selftest <> 0 THEN
    h = IO.FCREATE("/tmp/gmb_no_stub_selftest.c")
    IO.FWRITE(h, "static void gmb_bf_empty(void) { }" + CHR$(10))
    IO.FWRITE(h, "static void gmb_bf_guard(void) { GML_VM_REQUIRE(); }" + CHR$(10))
    IO.FWRITE(h, "static void gmb_bf_zero(void) { return 0; }" + CHR$(10))
    IO.FWRITE(h, "static void gmb_bf_ok(void) { bout->result = x + 1; }" + CHR$(10))
    IO.FCLOSE(h)
    CheckCFile("/tmp/gmb_no_stub_selftest.c", "static\s+void\s+(gmb_bf_\w+)\s*\([^)]*\)", "TEST")
    PRINT "selftest issues="; g_n
    IF g_n = 3 THEN PRINT "selftest OK" ELSE PRINT "selftest FAIL"
    KILL "/tmp/gmb_no_stub_selftest.c"
    IF g_n = 3 THEN RETURN 0
    RETURN 1
  END IF
  ' VM/C
  bdir = repo + "/GMBasic/native/gmb_vm_c"
  CheckCFile(bdir + "/gmb_builtin.c", "static\s+void\s+(gmb_bf_\w+)\s*\([^)]*\)", "VMC")
  CheckVmcTable()
  CheckVmcPush()
  files$ = GREP.FILES(" ", bdir)
  FOR i = 0 TO UBOUND(files$)
    IF INSTR(files$(i), "gmb_syscall_") > 0 AND RIGHT$(files$(i), 2) = ".c" THEN
      CheckCFile(files$(i), "(?:static\s+)?(?:int|void|gmb_val_t)\s+(gmb_\w+)\s*\([^)]*\)", "VMC")
    END IF
  NEXT i
  ' C99Native
  CheckC99Statements(repo + "/GMBasic/native/gmb_c99native/src/gmb_c99_emit.c")
  CheckC99Table()
  ' C# PCodeVM / IL
  CheckCSharpDir(repo + "/GMBasicSharp/common/IlBackendRuntime/PCode", "PCodeProcessor", "case BuiltInFunction.", "PCodeVM")
  CheckCSharpDir(repo + "/GMBasicSharp/common/IlBackendCodegen", ".cs", "case TokenKind.", "IL")
  nb = 0 : nr = 0
  FOR i = 1 TO g_n
    IF Binding(g_it(i)) <> 0 THEN nb = nb + 1 ELSE nr = nr + 1
  NEXT i
  IF quiet <> 0 THEN
    IF nb > 0 THEN
      PRINT "no_stub: "; nb; " COLPA (blocca) / "; nr; " reported (credito)"
    ELSE
      PRINT "no_stub: 0 colpa / "; nr; " reported (credito sottratto, non blocca)"
    END IF
    IF nb > 0 THEN RETURN 1
    RETURN 0
  END IF
  IF list <> 0 THEN
    FOR i = 1 TO g_n
      IF Binding(g_it(i)) <> 0 THEN
        PRINT "binding   "; g_ib(i); " "; g_it(i); " "; g_if(i); " : "; g_ih(i)
      ELSE
        PRINT "reported  "; g_ib(i); " "; g_it(i); " "; g_if(i); " : "; g_ih(i)
      END IF
    NEXT i
    PRINT "totale: "; nb; " colpa (binding) + "; nr; " reported (credito)"
    RETURN 0
  END IF
  IF nb > 0 THEN
    PRINT "GATE ANTI-STUB — COLPA (fatti deterministici, BLOCCA):"
    FOR i = 1 TO g_n
      IF Binding(g_it(i)) <> 0 THEN PRINT "  ["; g_ib(i); "] "; g_it(i); " "; g_if(i); " : "; g_ih(i)
    NEXT i
    PRINT ""
  END IF
  IF nr > 0 THEN
    PRINT "CREDITO SOTTRATTO (inferenze/gap dichiarati — "; nr; ", NON blocca):"
    FOR i = 1 TO g_n
      IF Binding(g_it(i)) = 0 THEN PRINT "  ["; g_ib(i); "] "; g_it(i); " "; g_if(i); " : "; g_ih(i)
    NEXT i
  END IF
  IF nb = 0 AND nr = 0 THEN PRINT "GATE ANTI-STUB — ok: nessuna colpa, nessun credito sottratto."
  IF nb > 0 THEN RETURN 1
  RETURN 0
END FUNCTION
