' gen_c_tokenkind.bas — sincronizza i tokenkind C (C99Native) con common/Ast/TokenKind.cs.
' Traduzione fedele di tools/gen_c_tokenkind.py (reference: stessi nomi di file, stesso
' ordine di operazioni, stesse righe di output, stesso exit code).
'
' Il generatore e' ADDITIVO e IDEMPOTENTE:
'   - legge i membri di TokenKind.cs (in ordine);
'   - aggiunge in CODA all'enum i token C# non ancora presenti nell'header C;
'   - aggiorna GMB_TOK_COUNT;
'   - preserva ordine/commenti/token native-only. Nessun token viene mai rimosso.
'
' Uso: test_ilrun --engine=il tools/basic/gen_c_tokenkind.bas [--check]
'      (l'harness imposta la CWD sulla cartella dello script: root = ..)
'      --check : non scrive; exit 1 se fuori sync (gate CI).
'
' Scostamenti dichiarati dalla reference (solo percorsi d'errore, input malformato):
'   la reference lancia un'eccezione Python (traceback); qui si stampa "ERRORE: ..." e
'   si esce con 2. Nessun'altra differenza.
IMPORT IO

DIM g_croot AS STRING   ' lato C (native/, ...): la cartella che contiene tools/
DIM g_pref AS STRING    ' lato C#: in monorepo e' ../GMBasicSharp/, nel repo separato = croot
DIM g_csname(6000) AS STRING
DIM g_csn AS INTEGER
DIM g_have(6000) AS STRING
DIM g_haven AS INTEGER
DIM g_miss(6000) AS STRING
DIM g_missn AS INTEGER
DIM g_block AS STRING

FUNCTION ReadAll(path AS STRING) AS STRING
  DIM s AS STRING
  IF NOT FILEEXISTS(path) THEN RETURN ""
  OPEN path FOR INPUT AS #1
  s = IO.FINPUT(IO.LOF(1), 1)
  CLOSE #1
  ReadAll = s
END FUNCTION

' Scrittura BYTE-ESATTA (l'header NON finisce con un newline: PRINT # ne aggiungerebbe uno).
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
  IF c = " " OR c = CHR$(9) OR c = CHR$(10) OR c = CHR$(13) THEN
    IsSpaceChar = 1
  ELSE
    IsSpaceChar = 0
  END IF
END FUNCTION

SUB AddHave(nm AS STRING)
  DIM i AS INTEGER
  DIM found AS INTEGER
  IF LEN(nm) = 0 THEN EXIT SUB
  found = 0
  FOR i = 1 TO g_haven
    IF g_have(i) = nm THEN
      found = 1
      EXIT FOR
    END IF
  NEXT i
  IF found = 0 AND g_haven < 6000 THEN
    g_haven = g_haven + 1
    g_have(g_haven) = nm
  END IF
END SUB

FUNCTION InHave(nm AS STRING) AS INTEGER
  DIM i AS INTEGER
  InHave = 0
  FOR i = 1 TO g_haven
    IF g_have(i) = nm THEN
      InHave = 1
      EXIT FOR
    END IF
  NEXT i
END FUNCTION

' Membri dell'enum TokenKind, in ordine di dichiarazione — STESSO parser della reference:
'   body = testo da "enum TokenKind" al primo "\n}" (esclusi);
'   commenti di riga "// ..." rimossi PRIMA di tokenizzare;
'   ogni identificatore seguito (anche dopo spazi/newline) da "," e' un membro.
' NB: un membro con valore esplicito ("Foo = 5,") NON viene catturato (dopo il nome c'e'
' " = ", non la virgola): e' il comportamento della reference, non una svista. La scansione
' qui lo riproduce identico (si guarda il primo carattere non-spazio dopo il nome).
SUB CsMembers(text AS STRING)
  DIM i0 AS LONG
  DIM iend AS LONG
  DIM i AS LONG
  DIM q AS LONG
  DIM c AS STRING
  DIM nm AS STRING
  g_csn = 0
  i0 = INSTR(text, "enum TokenKind")
  IF i0 = 0 THEN EXIT SUB
  iend = INSTR(i0, text, CHR$(10) + "}")
  IF iend = 0 THEN EXIT SUB
  i = i0
  DO WHILE i < iend
    c = MID$(text, i, 2)
    IF c = "//" THEN
      ' salta il commento di riga (il newline resta: e' spazio)
      DO WHILE i < iend AND MID$(text, i, 1) <> CHR$(10)
        i = i + 1
      LOOP
    ELSE
      c = MID$(text, i, 1)
      IF IsIdentStart(c) AND i > 0 AND (i = 1 OR IsIdentChar(MID$(text, i - 1, 1)) = 0) THEN
        ' il confine di parola (\b della reference) e' rispettato: un identificatore
        ' preceduto da una cifra/underscore NON e' un membro (es. "1Foo,").
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
          IF g_csn < 6000 THEN
            g_csn = g_csn + 1
            g_csname(g_csn) = nm
          END IF
        END IF
        i = q
      ELSE
        i = i + 1
      END IF
    END IF
  LOOP
END SUB

' Tutti i nomi gia' presenti nell'header: GMB_TOK_<nome> (anche GMB_TOK_COUNT ->
' "COUNT": la reference usa lo stesso insieme, quindi si riproduce identico).
SUB HeaderMembers(text AS STRING)
  DIM p AS LONG
  DIM q AS LONG
  DIM nm AS STRING
  g_haven = 0
  p = 1
  DO
    p = INSTR(p, text, "GMB_TOK_")
    IF p = 0 THEN EXIT DO
    q = p + 8
    nm = ""
    DO WHILE q <= LEN(text) AND IsIdentChar(MID$(text, q, 1))
      nm = nm + MID$(text, q, 1)
      q = q + 1
    LOOP
    AddHave(nm)
    IF q > p + 8 THEN
      p = q
    ELSE
      p = p + 8
    END IF
  LOOP
END SUB

' missing = [n for n in cs_members if n not in have]  (senza aggiungere a `have`: una
' ripetizione nel C# produrrebbe due aggiunte — come la reference).
SUB ComputeMissing()
  DIM i AS INTEGER
  g_missn = 0
  g_block = ""
  FOR i = 1 TO g_csn
    IF InHave(g_csname(i)) = 0 THEN
      IF g_missn < 6000 THEN
        g_missn = g_missn + 1
        g_miss(g_missn) = g_csname(i)
        IF g_missn > 1 THEN g_block = g_block + CHR$(10)
        g_block = g_block + "    GMB_TOK_" + g_csname(i) + ","
      END IF
    END IF
  NEXT i
END SUB

FUNCTION JoinMissing() AS STRING
  DIM i AS INTEGER
  DIM s AS STRING
  s = ""
  FOR i = 1 TO g_missn
    IF i > 1 THEN s = s + ", "
    s = s + g_miss(i)
  NEXT i
  JoinMissing = s
END FUNCTION

' Aggiunge in coda all'enum (prima di "} gmb_tokenkind_t;") e aggiorna GMB_TOK_COUNT.
' Ritorna il nuovo testo dell'header, oppure "" in caso di errore.
FUNCTION ApplyTo(hdr AS STRING) AS STRING
  DIM i AS LONG
  DIM p AS LONG
  DIM q AS LONG
  DIM cnt AS LONG
  DIM out AS STRING
  IF g_missn = 0 THEN
    ApplyTo = hdr
    EXIT FUNCTION
  END IF
  i = INSTR(hdr, "} gmb_tokenkind_t;")
  IF i = 0 THEN
    PRINT "ERRORE: '} gmb_tokenkind_t;' non trovata nell'header"
    ApplyTo = ""
    EXIT FUNCTION
  END IF
  out = MID$(hdr, 1, i - 1) + g_block + CHR$(10) + MID$(hdr, i)

  p = INSTR(out, "#define GMB_TOK_COUNT ")
  IF p = 0 THEN
    PRINT "ERRORE: '#define GMB_TOK_COUNT <n>' non trovato nell'header"
    ApplyTo = ""
    EXIT FUNCTION
  END IF
  q = p + 22
  DIM dg AS STRING
  dg = ""
  DO WHILE q <= LEN(out) AND MID$(out, q, 1) >= "0" AND MID$(out, q, 1) <= "9"
    dg = dg + MID$(out, q, 1)
    q = q + 1
  LOOP
  IF LEN(dg) = 0 THEN
    PRINT "ERRORE: conteggio GMB_TOK_COUNT non numerico"
    ApplyTo = ""
    EXIT FUNCTION
  END IF
  cnt = VAL(dg) + g_missn
  ApplyTo = MID$(out, 1, p - 1) + "#define GMB_TOK_COUNT " + LTRIM$(STR$(cnt)) + MID$(out, q)
END FUNCTION

FUNCTION Main() AS INTEGER
  DIM cs AS STRING
  DIM hdrPath AS STRING
  DIM hdr AS STRING
  DIM out AS STRING
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
  hdrPath = g_croot + "native/gmb_compiler_c/frontend/include/gmb_tokenkind.h"

  checkOnly = 0
  FOR i = 1 TO COMMANDCOUNT()
    arg = UPPER(COMMAND(i))
    IF arg = "--CHECK" THEN checkOnly = 1
  NEXT i

  cs = ReadAll(g_pref + "common/Ast/TokenKind.cs")
  hdr = ReadAll(hdrPath)
  IF LEN(cs) = 0 OR LEN(hdr) = 0 THEN
    PRINT "ERRORE: TokenKind.cs o gmb_tokenkind.h illeggibili"
    RETURN 2
  END IF

  CsMembers(cs)
  HeaderMembers(hdr)
  ComputeMissing()

  IF checkOnly THEN
    IF g_missn > 0 THEN
      PRINT "OUT OF SYNC: mancano " + LTRIM$(STR$(g_missn)) + " token: " + JoinMissing()
      RETURN 1
    END IF
    PRINT "tokenkind C# <-> C in sync."
    RETURN 0
  END IF

  IF g_missn = 0 THEN
    PRINT "tokenkind C# <-> C già in sync (nessuna aggiunta)."
    RETURN 0
  END IF

  out = ApplyTo(hdr)
  IF LEN(out) = 0 THEN RETURN 2
  IF WriteAll(hdrPath, out) = 0 THEN
    PRINT "ERRORE: scrittura fallita: "; hdrPath
    RETURN 2
  END IF
  PRINT "aggiunti " + LTRIM$(STR$(g_missn)) + " token (in coda, count aggiornato): " + JoinMissing()
  PRINT "ATTENZIONE: aggiungere i token è necessario ma NON sufficiente per il supporto reale in C99Native: servono lexer keyword + parser + emissione (emit_bf_*/tabella)."
  RETURN 0
END FUNCTION
