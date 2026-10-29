' mutation_sample.bas — mutation check dell'audit (da mutation_sample.py).
' Uso: mutation_sample.bas [IL|PCodeVM|C99|VMC|C99Native] [TOKEN]
IMPORT IO
IMPORT GREP
IMPORT SYSTEM

DIM g_old(200) AS STRING
DIM g_path(200) AS STRING
DIM g_n AS LONG

FUNCTION AuditCount(backend AS STRING) AS LONG
  DIM o AS STRING, l AS STRING, ls$() AS STRING, p AS LONG, v AS STRING, i AS LONG
  o = SYSTEM.SHELLOUT("dotnet run --project tools/audit --no-build -- --all")
  ls$ = SPLIT(o, CHR$(10))
  i = 0
  DO WHILE i <= UBOUND(ls$)
    l = TRIM$(ls$(i))
    IF UPPER$(LEFT$(l, LEN(backend))) = UPPER$(backend) THEN
      p = INSTR(l, " ")
      IF p > 0 THEN
        v = TRIM$(MID$(l, p + 1, LEN(l) - p))
        p = INSTR(v, " ")
        IF p > 0 THEN v = LEFT$(v, p - 1)
        IF LEN(v) > 0 THEN RETURN VAL(v)
      END IF
    END IF
    i = i + 1
  LOOP
  RETURN -1
END FUNCTION

FUNCTION FirstToken() AS STRING
  DIM o AS STRING, ls$() AS STRING, l AS STRING, i AS LONG, p AS LONG
  o = SYSTEM.SHELLOUT("dotnet run --project tools/audit --no-build -- --surface-tk")
  ls$ = SPLIT(o, CHR$(10))
  i = 0
  DO WHILE i <= UBOUND(ls$)
    l = TRIM$(ls$(i))
    IF LEN(l) > 0 THEN
      p = INSTR(l, CHR$(9))
      IF p > 0 THEN RETURN LEFT$(l, p - 1)
    END IF
    i = i + 1
  LOOP
  RETURN ""
END FUNCTION

SUB MutateFiles(dir AS STRING, old AS STRING, repl AS STRING)
  DIM files$() AS STRING, f AS STRING, s AS STRING, i AS LONG, h AS LONG
  files$ = GREP.FILES(old, dir)
  g_n = 0
  i = 0
  DO WHILE i <= UBOUND(files$)
    f = files$(i)
    s = IO.READFILE(f)
    IF INSTR(s, old) > 0 THEN
      g_old(g_n) = s
      g_path(g_n) = f
      g_n = g_n + 1
      h = IO.FCREATE(f)
      IO.FWRITE(h, REPLACE(s, old, repl))
      IO.FCLOSE(h)
    END IF
    i = i + 1
  LOOP
END SUB

SUB RestoreAll()
  DIM i AS LONG, h AS LONG
  i = 0
  DO WHILE i < g_n
    h = IO.FCREATE(g_path(i))
    IO.FWRITE(h, g_old(i))
    IO.FCLOSE(h)
    i = i + 1
  LOOP
END SUB

FUNCTION CBase() AS STRING
  IF DIREXISTS("GMBasicSharp/common/Compiler") <> 0 THEN RETURN "GMBasicSharp/common/"
  RETURN "common/"
END FUNCTION

FUNCTION NBase() AS STRING
  IF DIREXISTS("GMBasic/native/gmb_vm_c") <> 0 THEN RETURN "GMBasic/native/"
  RETURN "native/"
END FUNCTION

FUNCTION TokenCode(token AS STRING) AS STRING
  DIM txt AS STRING, ls$() AS STRING, l AS STRING, i AS LONG, p AS LONG, s AS STRING
  txt = IO.READFILE(CBase() + "Compiler/BuiltInFunction.cs")
  ls$ = SPLIT(txt, CHR$(10))
  i = 0
  DO WHILE i <= UBOUND(ls$)
    l = TRIM$(ls$(i))
    p = INSTR(l, token + " =")
    IF p = 0 THEN p = INSTR(l, token + "=")
    IF p > 0 THEN
      p = INSTR(l, "=")
      s = TRIM$(MID$(l, p + 1, LEN(l) - p))
      p = INSTR(s, ",")
      IF p > 0 THEN s = LEFT$(s, p - 1)
      RETURN TRIM$(s)
    END IF
    i = i + 1
  LOOP
  RETURN ""
END FUNCTION

SUB MutateRemoveLine(path AS STRING, needle AS STRING)
  DIM txt AS STRING, ls$() AS STRING, out AS STRING, l AS STRING, i AS LONG, h AS LONG
  txt = IO.READFILE(path)
  ls$ = SPLIT(txt, CHR$(10))
  g_old(g_n) = txt
  g_path(g_n) = path
  g_n = g_n + 1
  out = ""
  i = 0
  DO WHILE i <= UBOUND(ls$)
    l = ls$(i)
    IF INSTR(l, needle) = 0 THEN
      out = out + l
      IF i < UBOUND(ls$) THEN out = out + CHR$(10)
    END IF
    i = i + 1
  LOOP
  h = IO.FCREATE(path)
  IO.FWRITE(h, out)
  IO.FCLOSE(h)
END SUB
FUNCTION Main() AS INTEGER
  SET SHELL ON
  DIM backend AS STRING, token AS STRING, old AS STRING, repl AS STRING
  DIM before AS LONG, after AS LONG, code AS STRING
  DIM hit$() AS STRING
  backend = "IL"
  token = ""
  IF COMMANDCOUNT() >= 1 THEN backend = UPPER$(COMMAND$(1))
  IF COMMANDCOUNT() >= 2 THEN token = COMMAND$(2)
  IF LEN(token) = 0 THEN token = FirstToken()
  IF LEN(token) = 0 THEN
    PRINT "mutation: nessun token dalla superficie"
    RETURN 2
  END IF
  before = AuditCount(backend)
  IF before < 0 THEN
    PRINT "mutation: audit non leggibile per "; backend
    RETURN 2
  END IF
  g_n = 0
  IF backend = "IL" THEN
    old = "TokenKind." + token
    repl = "TokenKind.__MUT_" + token
    MutateFiles(CBase() + "IlBackendCodegen", old, repl)
  ELSEIF backend = "C99" THEN
    old = "TokenKind." + token
    repl = "TokenKind.__MUT_" + token
    MutateFiles(NBase() + "gmb_c99native/src", old, repl)
  ELSEIF backend = "PCODEVM" THEN
    old = "BuiltInFunction." + token
    repl = "BuiltInFunction.__MUT_" + token
    MutateFiles(CBase() + "IlBackendRuntime/PCode", old, repl)
  ELSEIF backend = "VMC" THEN
    code = TokenCode(token)
    IF LEN(code) > 0 THEN
      IF INSTR(IO.READFILE(NBase() + "gmb_vm_c/gmb_builtin.c"), "{ " + code + ", gmb_bf_") > 0 THEN
        MutateRemoveLine(NBase() + "gmb_vm_c/gmb_builtin.c", "{ " + code + ", gmb_bf_")
      END IF
    END IF
  ELSEIF backend = "C99NATIVE" THEN
    hit$ = GREP.FILES("GMB_TOK_" + token, NBase() + "gmb_compiler_c/src")
    IF UBOUND(hit$) >= 0 THEN
      MutateRemoveLine(NBase() + "gmb_compiler_c/include/gmb_c99_emit_builtin.h", "GMB_TOK_" + token)
    END IF
  ELSE
    PRINT "mutation: backend sconosciuto "; backend
    RETURN 3
  END IF
  IF g_n = 0 THEN
    PRINT "mutation: token non trovato "; token
    RETURN 3
  END IF
  after = AuditCount(backend)
  RestoreAll()
  PRINT backend; " "; token; " before="; before; " after="; after; " delta="; before - after
  IF after < 0 THEN RETURN 2
  IF before - after >= 1 THEN
    PRINT "mutation OK"
    RETURN 0
  END IF
  PRINT "mutation NON QUADRA"
  RETURN 1
END FUNCTION
