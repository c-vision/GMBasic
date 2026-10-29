' gen_db_tokenkind.bas — genera gli ORDINALI TokenKind usati dal runtime DB in C.
' Traduzione fedele di tools/gen_db_tokenkind.py (reference): stesso parser dell'enum,
' stesso file d'uscita, stesse righe, stesso exit code.
'
' Perche' esiste: il compilatore C# emette `stmt.Location.Kind` come costante numerica
' (EmitConstant((double)kind)). Il runtime C (native/gmb_db_c, native/gmb_vm_c) riceve quel
' numero GREZZO e lo confronta con #define. Scritti a mano, ogni TokenKind inserito PRIMA di
' `Top` li sposta di uno e il confronto cade sul ramo sbagliato — in silenzio (ISAM/SPEED GO
' <area>, TOP finiva nel ramo BOTTOM). Qui il numero e' un DERIVATO: enum piatto di
' common/Ast/TokenKind.cs, ordinale = POSIZIONE del membro.
'
' Uso: test_ilrun --engine=il tools/basic/gen_db_tokenkind.bas [--check]
'      (l'harness imposta la CWD sulla cartella dello script: root = ..)
'      --check : non scrive; exit 1 se drift (gate CI).
'
' Scostamenti dichiarati dalla reference:
'   1. input malformato: la reference lancia SystemExit/traceback Python, qui "ERRORE: ..." + exit 2;
'   2. il testo del messaggio di drift nomina tools/basic/gen_db_tokenkind.bas (il .py non esiste piu').
'      La semantica (condizione, exit code) e' identica.
IMPORT IO

DIM g_croot AS STRING   ' lato C (native/, ...): la cartella che contiene tools/
DIM g_pref AS STRING    ' lato C#: in monorepo e' ../GMBasicSharp/, nel repo separato = croot
DIM g_m(6000) AS STRING
DIM g_mn AS INTEGER
DIM g_out AS STRING

' (costante C, membro dell'enum C#) — un solo posto da toccare, il resto e' generato.
DIM g_wantc(8) AS STRING
DIM g_wantm(8) AS STRING
DIM g_wantn AS INTEGER

FUNCTION ReadAll(path AS STRING) AS STRING
  DIM s AS STRING
  IF NOT FILEEXISTS(path) THEN RETURN ""
  OPEN path FOR INPUT AS #1
  s = IO.FINPUT(IO.LOF(1), 1)
  CLOSE #1
  ReadAll = s
END FUNCTION

' Scrittura BYTE-ESATTA (header senza modifiche di newline: PRINT # ne aggiungerebbe uno).
FUNCTION WriteAll(path AS STRING, s AS STRING) AS INTEGER
  DIM h AS LONG
  DIM w AS LONG
  h = IO.FCreate(path)
  IF h <= 0 THEN
    WriteAll = 0
  ELSE
    w = IO.FWrite(h, s)
    w = IO.FClose(h)
    WriteAll = 1
  END IF
END FUNCTION

FUNCTION IsIdentStart(c AS STRING) AS INTEGER
  IF (c >= "A" AND c <= "Z") OR (c >= "a" AND c <= "z") THEN
    IsIdentStart = 1
  ELSE
    IsIdentStart = 0
  END IF
END FUNCTION

FUNCTION IsIdentChar(c AS STRING) AS INTEGER
  IF IsIdentStart(c) THEN
    IsIdentChar = 1
  ELSEIF c >= "0" AND c <= "9" THEN
    IsIdentChar = 1
  ELSEIF c = "_" THEN
    IsIdentChar = 1
  ELSE
    IsIdentChar = 0
  END IF
END FUNCTION

FUNCTION IsSpaceChar(c AS STRING) AS INTEGER
  IF c = " " OR c = CHR$(9) OR c = CHR$(10) OR c = CHR$(13) OR c = CHR$(11) OR c = CHR$(12) THEN
    IsSpaceChar = 1
  ELSE
    IsSpaceChar = 0
  END IF
END FUNCTION

' Membri dell'enum TokenKind, in ordine — STESSO parser della reference:
'   body = da "enum TokenKind" al primo "\n}" (esclusi);
'   commenti di riga "// ..." e di blocco "/* ... */" rimossi PRIMA di tokenizzare;
'   ogni identificatore (confine di parola) seguito, anche dopo spazi, da "," e' un membro.
SUB CsMembers(text AS STRING)
  DIM i0 AS LONG
  DIM iend AS LONG
  DIM i AS LONG
  DIM q AS LONG
  DIM nm AS STRING
  g_mn = 0
  i0 = INSTR(text, "enum TokenKind")
  IF i0 = 0 THEN EXIT SUB
  iend = INSTR(i0, text, CHR$(10) + "}")
  IF iend = 0 THEN EXIT SUB
  i = i0
  DO WHILE i < iend
    IF MID$(text, i, 2) = "//" THEN
      DO WHILE i < iend AND MID$(text, i, 1) <> CHR$(10)
        i = i + 1
      LOOP
    ELSEIF MID$(text, i, 2) = "/*" THEN
      q = INSTR(i + 2, text, "*/")
      IF q = 0 THEN
        i = iend
      ELSE
        i = q + 2
      END IF
    ELSEIF IsIdentStart(MID$(text, i, 1)) AND (i = 1 OR IsIdentChar(MID$(text, i - 1, 1)) = 0) THEN
      nm = ""
      q = i
      DO WHILE q < iend AND IsIdentChar(MID$(text, q, 1))
        nm = nm + MID$(text, q, 1)
        q = q + 1
      LOOP
      DO WHILE q < iend AND IsSpaceChar(MID$(text, q, 1))
        q = q + 1
      LOOP
      IF q < iend AND MID$(text, q, 1) = "," THEN
        IF g_mn < 6000 THEN
          g_mn = g_mn + 1
          g_m(g_mn) = nm
        END IF
      END IF
      i = q
    ELSE
      i = i + 1
    END IF
  LOOP
END SUB

' Ordinale del membro; -1 se assente. Su duplicati vince l'ULTIMO (come il dict Python).
FUNCTION OrdinalOf(nm AS STRING) AS LONG
  DIM i AS INTEGER
  OrdinalOf = -1
  FOR i = 1 TO g_mn
    IF g_m(i) = nm THEN OrdinalOf = i - 1
  NEXT i
END FUNCTION

SUB Want(cst AS STRING, mem AS STRING)
  IF g_wantn < 8 THEN
    g_wantn = g_wantn + 1
    g_wantc(g_wantn) = cst
    g_wantm(g_wantn) = mem
  END IF
END SUB

FUNCTION PadRight(s AS STRING, n AS LONG) AS STRING
  DIM r AS STRING
  r = s
  DO WHILE LEN(r) < n
    r = r + " "
  LOOP
  PadRight = r
END FUNCTION

' render(): header completo (stringa), oppure "" se un membro atteso non esiste.
FUNCTION Render() AS STRING
  DIM w AS LONG
  DIM i AS INTEGER
  DIM v AS LONG
  DIM s AS STRING
  w = 0
  FOR i = 1 TO g_wantn
    IF LEN(g_wantc(i)) > w THEN w = LEN(g_wantc(i))
  NEXT i
  s = "/* native/gmb_db_c/include/gmb_db_tokenkind_gen.h — GENERATO, non modificare a mano." + CHR$(10)
  s = s + " *" + CHR$(10)
  s = s + " * Sorgente: common/Ast/TokenKind.cs (enum piatto: l'ordinale e' la POSIZIONE del" + CHR$(10)
  s = s + " * membro). E' lo stesso numero che il compilatore C# mette nel chunk per" + CHR$(10)
  s = s + " * `stmt.Location.Kind` (EmitConstant((double)kind)), quindi il C deve usarlo identico." + CHR$(10)
  s = s + " *" + CHR$(10)
  s = s + " * Rigenera:  tools/basic/gen_db_tokenkind.bas" + CHR$(10)
  s = s + " * Verifica:  tools/basic/gen_db_tokenkind.bas --check   (exit 1 = drift)" + CHR$(10)
  s = s + " */" + CHR$(10)
  s = s + "#ifndef GMB_DB_TOKENKIND_GEN_H" + CHR$(10)
  s = s + "#define GMB_DB_TOKENKIND_GEN_H" + CHR$(10)
  s = s + CHR$(10)
  FOR i = 1 TO g_wantn
    v = OrdinalOf(g_wantm(i))
    IF v < 0 THEN
      PRINT "ERRORE: TokenKind.cs non contiene: "; g_wantm(i)
      Render = ""
      EXIT FUNCTION
    END IF
    s = s + "#define " + PadRight(g_wantc(i), w) + " " + PadRight(LTRIM$(STR$(v)), 5) + " /* TokenKind." + g_wantm(i) + " */" + CHR$(10)
  NEXT i
  s = s + CHR$(10)
  s = s + "#endif /* GMB_DB_TOKENKIND_GEN_H */" + CHR$(10)
  Render = s
END FUNCTION

FUNCTION Main() AS INTEGER
  DIM cs AS STRING
  DIM cur AS STRING
  DIM out AS STRING
  DIM rel AS STRING
  DIM checkOnly AS INTEGER
  DIM i AS INTEGER
  DIM arg AS STRING

  ' l'harness imposta la CWD sulla cartella dello script (tools/): root = ..
  ' Due prefissi (stessa convenzione di tools/basic/check_enum_ids.bas): nel MONOREPO il C# vive in
  ' ../GMBasicSharp/ e il C/native in ../; nel repo separato coincidono.
  g_croot = "../"
  IF FILEEXISTS(g_croot + "common/Ast/TokenKind.cs") THEN
    g_pref = g_croot
  ELSE
    g_pref = g_croot + "../GMBasicSharp/"
  END IF
  IF NOT FILEEXISTS(g_pref + "common/Ast/TokenKind.cs") THEN
    PRINT "ERRORE: common/Ast/TokenKind.cs non trovato (CWD = "; IO.Cwd(); ")"
    RETURN 2
  END IF
  g_out = g_croot + "native/gmb_db_c/include/gmb_db_tokenkind_gen.h"
  rel = "native/gmb_db_c/include/gmb_db_tokenkind_gen.h"

  checkOnly = 0
  FOR i = 1 TO COMMANDCOUNT()
    arg = UPPER(COMMAND(i))
    IF arg = "--CHECK" THEN checkOnly = 1
  NEXT i

  g_wantn = 0
  CALL Want("GMB_DB_TOK_TOP", "Top")
  CALL Want("GMB_DB_TOK_BOTTOM", "Bottom")

  cs = ReadAll(g_pref + "common/Ast/TokenKind.cs")
  IF LEN(cs) = 0 THEN
    PRINT "ERRORE: TokenKind.cs illeggibile"
    RETURN 2
  END IF
  CsMembers(cs)

  out = Render()
  IF LEN(out) = 0 THEN RETURN 2
  cur = ReadAll(g_out)
  IF cur = out THEN
    PRINT rel; " in sync."
    RETURN 0
  END IF
  IF checkOnly THEN
    PRINT "DRIFT: "; rel; " non corrisponde a TokenKind.cs (rigenera: test_ilrun --engine=il tools/basic/gen_db_tokenkind.bas)"
    RETURN 1
  END IF
  IF WriteAll(g_out, out) = 0 THEN
    PRINT "ERRORE: scrittura fallita: "; g_out
    RETURN 2
  END IF
  ' stampa le righe #define GMB_DB_TOK_* appena scritte (come la reference)
  DIM p AS LONG
  DIM e AS LONG
  p = 1
  DO
    p = INSTR(p, out, "#define GMB_DB_TOK_")
    IF p = 0 THEN EXIT DO
    e = INSTR(p, out, CHR$(10))
    IF e = 0 THEN e = LEN(out) + 1
    PRINT "  " + MID$(out, p, e - p)
    p = e
  LOOP
  PRINT "scritto "; rel
  RETURN 0
END FUNCTION
