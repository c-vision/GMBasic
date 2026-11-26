' check_enum_ids.bas — GATE unico (GMBasic): ID duplicati negli enum di dispatch,
' tabelle VM/C (core + ext), drift NOME=NUM nei commenti C, sync header syscall.
' Uso: test_ilrun --engine=il tools/basic/check_enum_ids.bas [--quiet] [--gen|--check]
'      exit code = numero problemi (0 = OK).
'      (monorepo: lanciare da GMBasic/; i C# stanno in ../GMBasicSharp/)
DIM gquiet AS INTEGER

FUNCTION Main() AS INTEGER
  DIM pref AS STRING
  DIM croot AS STRING
  DIM bad AS INTEGER
  DIM arg AS STRING
  DIM i AS INTEGER
  DIM genArg AS INTEGER
  DIM checkArg AS INTEGER
  ' RADICI: si SCOPRE dove sono i due alberi, senza assumere la CWD. Il vecchio `croot = "../"`
  ' valeva solo per il layout `GMBasic/tools/`; dopo lo spostamento in `<monorepo>/tools/basic/`
  ' risolveva a `tools/` e i controlli C finivano in SKIP silenzioso (il gate «passava» senza
  ' guardare né `gmb_tokenkind.h` né le tabelle VM/C: così un drift vero è passato inosservato).
  ' C  = albero con `native/gmb_vm_c/gmb_builtin.c`
  ' C# = albero con `common/Ast/TokenKind.cs` + `common/Compiler/SysCalls.cs`
  DIM cand(8) AS STRING
  DIM csz(6) AS STRING
  cand(0) = "../"            ' cwd = GMBasic/tools (layout storico)
  cand(1) = "./"             ' cwd = GMBasic
  cand(2) = "../../GMBasic/" ' cwd = tools/basic
  cand(3) = "GMBasic/"       ' cwd = radice monorepo
  cand(4) = "../../"         ' cwd = tools/<x> (una risalita in meno)
  cand(5) = "../GMBasic/"    ' cwd = GMBasicSharp
  croot = ""
  FOR i = 0 TO 5
    IF croot = "" THEN
      IF FILEEXISTS(cand(i) + "native/gmb_vm_c/gmb_builtin.c") THEN croot = cand(i)
    END IF
  NEXT i
  IF croot = "" THEN croot = "./"
  pref = ""
  csz(0) = croot                 ' layout originale: C# dentro l'albero C
  csz(1) = croot + "../GMBasicSharp/"   ' monorepo, C# accanto a GMBasic
  csz(2) = croot + "GMBasicSharp/"
  csz(3) = "../../GMBasicSharp/"        ' cwd = tools/basic
  csz(4) = "../GMBasicSharp/"           ' cwd = GMBasic
  FOR i = 0 TO 4
    IF pref = "" THEN
      IF FILEEXISTS(csz(i) + "common/Ast/TokenKind.cs") THEN
        IF FILEEXISTS(csz(i) + "common/Compiler/SysCalls.cs") THEN pref = csz(i)
      END IF
    END IF
  NEXT i
  IF pref = "" THEN pref = croot
  gquiet = 0
  genArg = 0
  checkArg = 0
  FOR i = 1 TO COMMANDCOUNT()
    arg = UPPER(COMMAND(i))
    IF arg = "--QUIET" THEN gquiet = 1
    IF arg = "--GEN" THEN genArg = 1
    IF arg = "--CHECK" THEN checkArg = 1
  NEXT i
  IF genArg OR checkArg THEN RETURN GenSyscalls(pref, croot, checkArg, gquiet)
  bad = 0
  bad = bad + CheckEnum(pref + "common/Ast/TokenKind.cs", "TokenKind", 1)
  bad = bad + CheckEnum(pref + "common/Compiler/BuiltInFunction.cs", "BuiltInFunction", 1)
  bad = bad + CheckEnum(croot + "native/gmb_compiler_c/frontend/include/gmb_tokenkind.h", "gmb_tokenkind (C)", 0)
  bad = bad + CheckTables(croot)
  bad = bad + CheckDrift(pref, croot)
  bad = bad + GenSyscalls(pref, croot, 1, gquiet)
  IF bad > 0 THEN
    PRINT "GATE FALLITO: "; bad; " problema/i"
  ELSEIF gquiet = 0 THEN
    PRINT "GATE OK: nessun ID duplicato, tabelle allineate, header syscall in sync"
  END IF
  RETURN bad
END FUNCTION

' prefix: 0 = enum C (GMB_TOK_...), 1 = enum C# (membri nudi)
FUNCTION CheckEnum(path AS STRING, label AS STRING, cs AS INTEGER) AS INTEGER
  DIM names(12000) AS STRING
  DIM vals(12000) AS LONG
  DIM hasv(12000) AS INTEGER
  DIM n AS INTEGER
  DIM prev AS LONG
  DIM inEnum AS INTEGER
  DIM inBlock AS INTEGER
  DIM pend AS INTEGER
  DIM ln AS STRING
  DIM work AS STRING
  DIM part AS STRING
  DIM u AS STRING
  DIM name AS STRING
  DIM ex AS STRING
  DIM ex2 AS STRING
  DIM v AS LONG
  DIM okv AS INTEGER
  DIM bad AS INTEGER
  DIM nlines AS INTEGER
  DIM i AS INTEGER
  DIM j AS INTEGER
  DIM k AS INTEGER
  DIM p AS INTEGER
  DIM q AS INTEGER
  DIM cp AS INTEGER
  bad = 0
  n = 0
  prev = -1
  inEnum = 0
  inBlock = 0
  pend = 0
  IF NOT FILEEXISTS(path) THEN
    ' NON si salta in silenzio: un path sbagliato/rinominato spegnerebbe il controllo
    ' (è quello che è successo dopo lo spostamento in tools/basic/).
    PRINT "ERRORE "; label; ": file assente ("; path; ")"
    CheckEnum = 1
    EXIT FUNCTION
  END IF
  OPEN path FOR INPUT AS #1
  nlines = 0
  DO UNTIL EOF(1)
    LINE INPUT #1, ln
    nlines = nlines + 1
    ' rimuove commenti (// e /* ... */ anche multi-riga)
    IF inBlock THEN
      p = INSTR(ln, "*/")
      IF p = 0 THEN
        ln = ""
      ELSE
        ln = MID$(ln, p + 2)
        inBlock = 0
      END IF
    END IF
    DO
      p = INSTR(ln, "/*")
      IF p = 0 THEN EXIT DO
      q = INSTR(p + 2, ln, "*/")
      IF q = 0 THEN
        ln = LEFT$(ln, p - 1)
        inBlock = 1
        EXIT DO
      ELSE
        ln = LEFT$(ln, p - 1) + MID$(ln, q + 2)
      END IF
    LOOP
    p = INSTR(ln, "//")
    IF p > 0 THEN ln = LEFT$(ln, p - 1)
    u = UPPER(ln)
    IF inEnum = 0 THEN
      IF INSTR(u, "ENUM") > 0 THEN pend = 1
      IF pend AND INSTR(ln, "{") > 0 THEN
        inEnum = 1
        pend = 0
        prev = -1
        work = MID$(ln, INSTR(ln, "{") + 1)
      ELSE
        work = ""
      END IF
    ELSE
      work = ln
    END IF
    IF inEnum THEN
      cp = INSTR(work, "}")
      IF cp > 0 THEN
        work = LEFT$(work, cp - 1)
      END IF
      ' split top-level sulle virgole (gli enum qui non hanno parentesi)
      DO WHILE LEN(work) > 0
        q = INSTR(work, ",")
        IF q > 0 THEN
          part = LEFT$(work, q - 1)
          work = MID$(work, q + 1)
        ELSE
          part = work
          work = ""
        END IF
        part = TRIM$(part)
        IF LEN(part) > 0 THEN
          k = INSTR(part, "=")
          IF k > 0 THEN
            name = TRIM$(LEFT$(part, k - 1))
            ex = TRIM$(MID$(part, k + 1))
          ELSE
            name = part
            ex = ""
          END IF
          IF cs = 0 THEN
            IF UPPER(LEFT$(name, 7)) = "GMB_TOK" THEN name = MID$(name, 8)
          END IF
          okv = 1
          IF LEN(name) = 0 THEN okv = 0
          FOR i = 1 TO LEN(name)
            j = INSTR("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_", MID$(name, i, 1))
            IF j = 0 THEN okv = 0
          NEXT i
          IF okv THEN
            IF LEN(ex) = 0 THEN
              IF prev >= 0 THEN v = prev + 1 ELSE v = 0
            ELSE
              ex2 = TRIM$(ex)
              IF ISNUMERIC(ex2) THEN
                v = VAL(ex2)
                okv = 1
              ELSEIF UPPER(LEFT$(ex2, 2)) = "0X" THEN
                v = HexVal(MID$(ex2, 3))
                okv = 1
              ELSE
                okv = 0
                FOR i = 1 TO n
                  IF hasv(i) THEN
                    IF names(i) = UPPER$(ex2) THEN
                      v = vals(i)
                      okv = 1
                      EXIT FOR
                    END IF
                  END IF
                NEXT i
              END IF
            END IF
          END IF
          IF okv THEN
            n = n + 1
            names(n) = UPPER(name)
            vals(n) = v
            hasv(n) = 1
            prev = v
            ' duplicato?
            FOR i = 1 TO n - 1
              IF hasv(i) THEN
                IF vals(i) = v THEN
                  bad = bad + 1
                  PRINT "DUPLICATO "; label; " "; v; ": "; name; ", "; names(i)
                  EXIT FOR
                END IF
              END IF
            NEXT i
          ELSE
            prev = -1
          END IF
        END IF
      LOOP
      IF cp > 0 THEN
        inEnum = 0
        prev = -1
      END IF
    END IF
  LOOP
  CLOSE #1
  IF cs = 0 THEN
    ' per l'enum C il prefisso GMB_TOK_ e' stato rimosso nei nomi stampati
  END IF
  IF gquiet = 0 THEN PRINT "OK "; label; ": "; n; " membri (nessun duplicato se sopra non compare nulla)"
  CheckEnum = bad
END FUNCTION

' Valore di un membro: intero, hex 0x, oppure nome gia' visto. 0 = non determinabile.
FUNCTION EvalEnum(ex AS STRING, names() AS STRING, vals() AS LONG, hasv() AS INTEGER, n AS INTEGER, v AS LONG) AS INTEGER
  DIM t AS STRING
  DIM i AS INTEGER
  DIM neg AS INTEGER
  EvalEnum = 0
  t = TRIM$(ex)
  IF LEN(t) = 0 THEN EXIT FUNCTION
  neg = 0
  IF LEFT$(t, 1) = "-" THEN neg = 1 : t = TRIM$(MID$(t, 2))
  IF LEFT$(UPPER(t), 2) = "0X" THEN
    v = HexVal(MID$(t, 3))
    IF neg THEN v = -v
    EvalEnum = 1
    EXIT FUNCTION
  END IF
  IF ISNUMERIC(t) THEN
    v = VAL(t)
    IF neg THEN v = -v
    EvalEnum = 1
    EXIT FUNCTION
  END IF
  t = UPPER(t)
  FOR i = 1 TO n
    IF hasv(i) THEN
      IF names(i) = t THEN
        v = vals(i)
        IF neg THEN v = -v
        EvalEnum = 1
        EXIT FUNCTION
      END IF
    END IF
  NEXT i
END FUNCTION

FUNCTION HexVal(h AS STRING) AS LONG
  DIM i AS INTEGER
  DIM c AS INTEGER
  DIM v AS LONG
  v = 0
  h = UPPER(h)
  FOR i = 1 TO LEN(h)
    c = ASC(MID$(h, i, 1))
    IF c >= 48 AND c <= 57 THEN
      v = v * 16 + (c - 48)
    ELSEIF c >= 65 AND c <= 70 THEN
      v = v * 16 + (c - 55)
    END IF
  NEXT i
  HexVal = v
END FUNCTION

' ---- SysCalls.cs: membri e valori (per drift e header) ----
FUNCTION LoadSysCalls(pref AS STRING, BYREF names() AS STRING, BYREF vals() AS LONG, BYREF hasv() AS INTEGER) AS INTEGER
  DIM path AS STRING
  DIM ln AS STRING
  DIM u AS STRING
  DIM work AS STRING
  DIM part AS STRING
  DIM name AS STRING
  DIM ex AS STRING
  DIM p AS INTEGER
  DIM q AS INTEGER
  DIM cp AS INTEGER
  DIM k AS INTEGER
  DIM n AS INTEGER
  DIM prev AS LONG
  DIM v AS LONG
  DIM inEnum AS INTEGER
  DIM pend AS INTEGER
  path = pref + "common/Compiler/SysCalls.cs"
  n = 0
  prev = -1
  inEnum = 0
  pend = 0
  IF NOT FILEEXISTS(path) THEN
    PRINT "ERRORE SysCalls.cs: file assente ("; path; ")"
    RETURN 1
  END IF
  OPEN path FOR INPUT AS #7
  DO UNTIL EOF(7)
    LINE INPUT #7, ln
    p = INSTR(ln, "//")
    IF p > 0 THEN ln = LEFT$(ln, p - 1)
    u = UPPER(ln)
    IF inEnum = 0 THEN
      IF INSTR(u, "ENUM") > 0 THEN pend = 1
      IF pend AND INSTR(ln, "{") > 0 THEN
        inEnum = 1
        pend = 0
        prev = -1
        work = MID$(ln, INSTR(ln, "{") + 1)
      ELSE
        work = ""
      END IF
    ELSE
      work = ln
    END IF
    IF inEnum THEN
      cp = INSTR(work, "}")
      IF cp > 0 THEN work = LEFT$(work, cp - 1)
      DO WHILE LEN(work) > 0
        q = INSTR(work, ",")
        IF q > 0 THEN
          part = LEFT$(work, q - 1)
          work = MID$(work, q + 1)
        ELSE
          part = work
          work = ""
        END IF
        part = TRIM$(part)
        IF LEN(part) > 0 THEN
          k = INSTR(part, "=")
          IF k > 0 THEN
            name = TRIM$(LEFT$(part, k - 1))
            ex = TRIM$(MID$(part, k + 1))
          ELSE
            name = part
            ex = ""
          END IF
          IF LEN(name) > 0 THEN
            IF LEN(ex) = 0 THEN
              v = prev + 1
            ELSEIF ISNUMERIC(ex) THEN
              v = VAL(ex)
            ELSEIF UPPER(LEFT$(ex, 2)) = "0X" THEN
              v = HexVal(MID$(ex, 3))
            ELSE
              v = prev + 1
            END IF
            n = n + 1
            names(n) = TRIM$(name)
            vals(n) = v
            hasv(n) = 1
            prev = v
          END IF
        END IF
      LOOP
      IF cp > 0 THEN inEnum = 0
    END IF
  LOOP
  CLOSE #7
  LoadSysCalls = n
END FUNCTION

' ---- gen_syscall_codes: render + check ----
FUNCTION GenSyscalls(pref AS STRING, croot AS STRING, checkOnly AS INTEGER, quiet AS INTEGER) AS INTEGER
  DIM names(2000) AS STRING
  DIM vals(2000) AS LONG
  DIM hasv(2000) AS INTEGER
  DIM n AS INTEGER
  DIM i AS INTEGER
  DIM bad AS INTEGER
  DIM hdr AS STRING
  DIM cur AS STRING
  DIM ln AS STRING
  DIM emdash AS STRING
  DIM out AS STRING
  DIM p AS INTEGER
  DIM q AS INTEGER
  DIM rest AS STRING
  DIM name AS STRING
  DIM num AS LONG
  DIM found AS INTEGER
  n = LoadSysCalls(pref, names(), vals(), hasv())
  IF n = 0 THEN RETURN 1
  emdash = "—"
  hdr = "/* gmb_syscall_codes.h " + emdash + " GENERATO da common/Compiler/SysCalls.cs (tools/basic/check_enum_ids.bas)." + CHR$(10)
  hdr = hdr + " * NON modificare a mano: ogni --gen riscrive questo file e i commenti scritti qui spariscono." + CHR$(10)
  hdr = hdr + " * La documentazione dei codici (P4, SCENE PICK, ...) si scrive in common/Compiler/SysCalls.cs," + CHR$(10)
  hdr = hdr + " * accanto al valore dell'enum: quello e' il sorgente e li' il commento sopravvive alla rigenerazione." + CHR$(10)
  hdr = hdr + " * Una riga di tabella VM/C si scrive per NOME: { GMB_SC_FILE_LOCK, handler }," + CHR$(10)
  hdr = hdr + " * cosi' il numero lo decide sempre l'enum e un inserimento a meta' non disallinea piu' la VM. */" + CHR$(10)
  hdr = hdr + "#ifndef GMB_SYSCALL_CODES_H" + CHR$(10)
  hdr = hdr + "#define GMB_SYSCALL_CODES_H" + CHR$(10) + CHR$(10)
  FOR i = 1 TO n
    hdr = hdr + "#define GMB_SC_" + names(i) + " " + LTRIM$(STR$(vals(i))) + CHR$(10)
  NEXT i
  hdr = hdr + CHR$(10) + "#endif"
  out = croot + "native/gmb_vm_c/gmb_syscall_codes.h"
  bad = 0
  IF checkOnly = 0 THEN
    OPEN out FOR OUTPUT AS #8
    PRINT #8, hdr
    CLOSE #8
    IF quiet = 0 THEN PRINT "scritto "; out; " ("; n; " syscall)"
    RETURN 0
  END IF
  cur = ""
  IF FILEEXISTS(out) THEN
    OPEN out FOR INPUT AS #8
    DO UNTIL EOF(8)
      LINE INPUT #8, ln
      cur = cur + ln + CHR$(10)
    LOOP
    CLOSE #8
  END IF
  IF cur <> hdr + CHR$(10) THEN
    PRINT "DRIFT: "; out; " fuori sync con SysCalls.cs - eseguire tools/check_enum_ids --gen"
    bad = bad + 1
  END IF
  ' macro GMB_SYS_* di gmb_db.h
  IF FILEEXISTS(croot + "native/gmb_db_c/include/gmb_db.h") THEN
    OPEN croot + "native/gmb_db_c/include/gmb_db.h" FOR INPUT AS #10
    DO UNTIL EOF(10)
      LINE INPUT #10, ln
      p = INSTR(UPPER(ln), "GMB_SYS_")
      IF p > 0 THEN
        rest = MID$(ln, p + 8)
        name = ""
        q = 1
        DO WHILE q <= LEN(rest)
          IF (MID$(rest, q, 1) >= "A" AND MID$(rest, q, 1) <= "Z") OR (MID$(rest, q, 1) >= "0" AND MID$(rest, q, 1) <= "9") OR MID$(rest, q, 1) = "_" THEN
            name = name + MID$(rest, q, 1)
            q = q + 1
          ELSE
            EXIT DO
          END IF
        LOOP
        DO WHILE q <= LEN(rest) AND MID$(rest, q, 1) = " " : q = q + 1 : LOOP
        IF LEN(name) > 0 AND q <= LEN(rest) AND MID$(rest, q, 1) >= "0" AND MID$(rest, q, 1) <= "9" THEN
          rest = MID$(rest, q)
          num = VAL(rest)
          found = 0
          FOR i = 1 TO n
            IF UPPER$(names(i)) = name THEN found = 1 : num = vals(i) - num : EXIT FOR
          NEXT i
          IF found AND num <> 0 THEN
            PRINT "DRIFT: gmb_db.h GMB_SYS_"; name; " (enum "; vals(i); ")"
            bad = bad + 1
          END IF
        END IF
      END IF
    LOOP
    CLOSE #10
  END IF
  IF bad = 0 AND quiet = 0 THEN PRINT "OK: gmb_syscall_codes.h in sync ("; n; " syscall), GMB_SYS_* di gmb_db.h coerenti"
  RETURN bad
END FUNCTION

' ---- drift NOME=NUM nei commenti C ----
SUB ScanClaims(txt AS STRING, file AS STRING, lineno AS INTEGER, BYREF names() AS STRING, BYREF vals() AS LONG, BYREF hasv() AS INTEGER, n AS INTEGER, BYREF bad AS INTEGER)
  DIM i AS INTEGER
  DIM st AS INTEGER
  DIM j AS INTEGER
  DIM q AS INTEGER
  DIM c AS INTEGER
  DIM cprev AS INTEGER
  DIM tail AS INTEGER
  DIM name AS STRING
  DIM num AS LONG
  DIM found AS INTEGER
  i = 1
  DO WHILE i <= LEN(txt)
    c = ASC(MID$(txt, i, 1))
    IF (c >= 65 AND c <= 90) OR c = 95 THEN
      st = i
      DO WHILE i <= LEN(txt)
        c = ASC(MID$(txt, i, 1))
        IF (c >= 65 AND c <= 90) OR (c >= 48 AND c <= 57) OR c = 95 THEN
          i = i + 1
        ELSE
          EXIT DO
        END IF
      LOOP
      IF i - st >= 3 THEN
        name = MID$(txt, st, i - st)
        q = i
        DO WHILE q <= LEN(txt) AND MID$(txt, q, 1) = " " : q = q + 1 : LOOP
        IF q <= LEN(txt) AND MID$(txt, q, 1) = "=" THEN
          q = q + 1
          DO WHILE q <= LEN(txt) AND MID$(txt, q, 1) = " " : q = q + 1 : LOOP
          IF q <= LEN(txt) AND MID$(txt, q, 1) >= "0" AND MID$(txt, q, 1) <= "9" THEN
            num = VAL(MID$(txt, q))
            tail = 0
            IF INSTR(name, "_") = 0 AND st > 1 THEN
              j = st - 1
              DO WHILE j >= 1 AND MID$(txt, j, 1) = " " : j = j - 1 : LOOP
              IF j >= 1 THEN
                cprev = ASC(MID$(txt, j, 1))
                IF (cprev >= 65 AND cprev <= 90) OR (cprev >= 48 AND cprev <= 57) OR cprev = 95 THEN tail = 1
              END IF
            END IF
            found = 0
            IF tail = 0 THEN
              FOR j = 1 TO n
                IF hasv(j) THEN
                  IF UPPER$(names(j)) = name THEN
                    found = 1
                    IF vals(j) <> num THEN
                      PRINT "DRIFT C#->C: "; file; ":"; lineno; ": "; name; "="; num; " (enum C#: "; vals(j); ")"
                      bad = bad + 1
                    END IF
                    EXIT FOR
                  END IF
                END IF
              NEXT j
            END IF
          END IF
        END IF
      END IF
    ELSE
      i = i + 1
    END IF
  LOOP
END SUB

FUNCTION CheckDrift(pref AS STRING, croot AS STRING) AS INTEGER
  DIM names(2000) AS STRING
  DIM vals(2000) AS LONG
  DIM hasv(2000) AS INTEGER
  DIM n AS INTEGER
  DIM fi AS INTEGER
  DIM ln AS STRING
  DIM txt AS STRING
  DIM p AS INTEGER
  DIM q AS INTEGER
  DIM ce AS INTEGER
  DIM lineno AS INTEGER
  DIM inBlock AS INTEGER
  DIM bad AS INTEGER
  DIM file AS STRING
  n = LoadSysCalls(pref, names(), vals(), hasv())
  bad = 0
  FOR fi = 0 TO 2
    IF fi = 0 THEN file = croot + "native/gmb_vm_c/gmb_vm_core.c"
    IF fi = 1 THEN file = croot + "native/gmb_vm_c/gmb_vm_tui.c"
    IF fi = 2 THEN file = croot + "native/gmb_vm_c/gmb_vm_tui.h"
    IF FILEEXISTS(file) THEN
      OPEN file FOR INPUT AS #11
      lineno = 0
      inBlock = 0
      DO UNTIL EOF(11)
        LINE INPUT #11, ln
        lineno = lineno + 1
        p = 1
        DO WHILE p <= LEN(ln)
          IF inBlock THEN
            q = INSTR(p, ln, "*/")
            IF q = 0 THEN
              ScanClaims(MID$(ln, p), file, lineno, names(), vals(), hasv(), n, bad)
              p = LEN(ln) + 1
            ELSE
              ScanClaims(MID$(ln, p, q - p), file, lineno, names(), vals(), hasv(), n, bad)
              p = q + 2
              inBlock = 0
            END IF
          ELSE
            q = INSTR(p, ln, "//")
            ce = INSTR(p, ln, "/*")
            IF q = 0 AND ce = 0 THEN
              p = LEN(ln) + 1
            ELSEIF q > 0 AND (ce = 0 OR q < ce) THEN
              ScanClaims(MID$(ln, q + 2), file, lineno, names(), vals(), hasv(), n, bad)
              p = LEN(ln) + 1
            ELSE
              q = INSTR(ce + 2, ln, "*/")
              IF q > 0 THEN
                ScanClaims(MID$(ln, ce + 2, q - ce - 2), file, lineno, names(), vals(), hasv(), n, bad)
                p = q + 2
              ELSE
                ScanClaims(MID$(ln, ce + 2), file, lineno, names(), vals(), hasv(), n, bad)
                inBlock = 1
                p = LEN(ln) + 1
              END IF
            END IF
          END IF
        LOOP
      LOOP
      CLOSE #11
    END IF
  NEXT fi
  IF bad = 0 AND gquiet = 0 THEN PRINT "OK drift C#->C: 0 affermazioni obsolete"
  CheckDrift = bad
END FUNCTION

' Tabelle dispatch VM/C: righe "{ <codice>, gmb_bf_<handler> },".
FUNCTION CheckTables(croot AS STRING) AS INTEGER
  DIM codes(40000) AS LONG
  DIM whr(40000) AS STRING
  DIM n AS INTEGER
  DIM bad AS INTEGER
  n = 0
  bad = 0
  bad = bad + ScanTable(croot + "native/gmb_vm_c/gmb_builtin.c", codes(), whr(), n)
  bad = bad + ScanTable(croot + "native/gmb_vm_c/gmb_builtin_ext.c", codes(), whr(), n)
  IF gquiet = 0 THEN PRINT "OK tabelle VM/C: "; n; " voci"
  CheckTables = bad
END FUNCTION

FUNCTION ScanTable(path AS STRING, BYREF codes() AS LONG, BYREF whr() AS STRING, BYREF n AS INTEGER) AS INTEGER
  DIM ln AS STRING
  DIM t AS STRING
  DIM code AS LONG
  DIM h AS STRING
  DIM p AS INTEGER
  DIM q AS INTEGER
  DIM cp AS INTEGER
  DIM i AS INTEGER
  DIM bad AS INTEGER
  bad = 0
  IF NOT FILEEXISTS(path) THEN
    PRINT "ERRORE tabella: file assente ("; path; ")"
    ScanTable = 1
    EXIT FUNCTION
  END IF
  OPEN path FOR INPUT AS #9
  nlines = 0
  DO UNTIL EOF(9)
    LINE INPUT #9, ln
    nlines = nlines + 1
    p = INSTR(ln, "{")
    q = INSTR(ln, ", gmb_bf_")
    IF p > 0 AND q > p THEN
      t = TRIM$(MID$(ln, p + 1, q - p - 1))
      IF ISNUMERIC(t) THEN
        code = VAL(t)
        h = MID$(ln, q + 2)
        h = TRIM$(h)
        ' toglie "},..." finale
        p = INSTR(h, "}")
        IF p > 0 THEN h = LEFT$(h, p - 1)
        h = TRIM$(h)
        n = n + 1
        codes(n) = code
        whr(n) = h
        ' duplicato nel set combinato
        FOR i = 1 TO n - 1
          IF codes(i) = code THEN
            bad = bad + 1
            PRINT "DUPLICATO tabella VM/C "; code; ": "; h; ", "; whr(i)
            EXIT FOR
          END IF
        NEXT i
      END IF
    END IF
  LOOP
  CLOSE #9
  ScanTable = bad
END FUNCTION
