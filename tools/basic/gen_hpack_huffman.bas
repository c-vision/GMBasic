' ==============================================================================
' gen_hpack_huffman.bas — genera le tabelle C di HPACK (RFC 7541) dai dati ufficiali.
'
' Perche' un generatore invece di scrivere 257 codici a mano: la tabella Huffman e'
' grande e un singolo bit sbagliato produce header corrotti solo su alcuni valori.
' Cosi' la tabella si rigenera e si confronta con l'RFC.
'
' Uso (dalla root del repo, gamba nativa):
'   bash tools/sh/gmb_c99native_run.sh ../tools/basic/gen_hpack_huffman.bas hpck
'   bash tools/sh/gmb_c99native_run.sh ../tools/basic/gen_hpack_huffman.bas hpck -- --check
'   ... --rfc /tmp/rfc7541.txt     (usa una copia locale invece di scaricare)
'
' Produce:
'   GMBasic/native/gmb_runtime_c/src/gmb_hpack_huffman.h  (257 simboli + EOS)
'   GMBasic/native/gmb_runtime_c/src/gmb_hpack_static.h   (tabella statica, 61 voci)
' ==============================================================================
IMPORT IO
IMPORT SYSTEM

DIM g_url AS STRING
DIM g_cache AS STRING

DIM g_hcode(300) AS LONG
DIM g_hbits(300) AS LONG
DIM g_hhave(300) AS LONG

DIM g_sidx(128) AS LONG
DIM g_sname(128) AS STRING
DIM g_sval(128) AS STRING
DIM g_shave(128) AS LONG

FUNCTION TrimS(s AS STRING) AS STRING
  RETURN RTRIM$(LTRIM$(s))
END FUNCTION

FUNCTION IsDigitCh(ch AS STRING) AS LONG
  DIM c AS LONG
  IF LEN(ch) = 0 THEN RETURN 0
  c = ASC(ch)
  IF c >= 48 AND c <= 57 THEN RETURN -1
  RETURN 0
END FUNCTION

FUNCTION IsHexCh(ch AS STRING) AS LONG
  DIM c AS LONG
  IF LEN(ch) = 0 THEN RETURN 0
  c = ASC(ch)
  IF (c >= 48 AND c <= 57) OR (c >= 97 AND c <= 102) OR (c >= 65 AND c <= 70) THEN RETURN -1
  RETURN 0
END FUNCTION

FUNCTION HexVal(h AS STRING) AS LONG
  DIM i AS LONG, v AS LONG, c AS LONG, ch AS STRING
  v = 0
  FOR i = 1 TO LEN(h)
    ch = UCASE$(MID$(h, i, 1))
    c = 0
    IF ch >= "0" AND ch <= "9" THEN c = ASC(ch) - 48
    IF ch >= "A" AND ch <= "F" THEN c = ASC(ch) - 55
    v = v * 16 + c
  NEXT i
  RETURN v
END FUNCTION

FUNCTION HexDigit(v AS LONG) AS STRING
  DIM d AS STRING
  d = "0123456789abcdef"
  RETURN MID$(d, (v MOD 16) + 1, 1)
END FUNCTION

FUNCTION Hex8(v AS LONG) AS STRING
  DIM s AS STRING, i AS LONG, x AS LONG
  s = ""
  x = v
  FOR i = 1 TO 8
    s = HexDigit(x MOD 16) + s
    x = x \ 16
  NEXT i
  RETURN s
END FUNCTION

FUNCTION Pad2(n AS LONG) AS STRING
  DIM s AS STRING
  s = LTRIM$(STR$(n))
  IF LEN(s) < 2 THEN s = " " + s
  RETURN s
END FUNCTION

' ---- RFC ---------------------------------------------------------------------
FUNCTION RfcText(rfcLocal AS STRING) AS STRING
  DIM t AS STRING, wr AS LONG
  IF LEN(rfcLocal) > 0 THEN
    IF FILEEXISTS(rfcLocal) THEN RETURN IO.READFILE(rfcLocal)
  END IF
  IF FILEEXISTS(g_cache) THEN
    t = IO.READFILE(g_cache)
    IF INSTR(t, "(  0)  |11111111|11000") > 0 THEN RETURN t
  END IF
  wr = SYSTEM.SHELLOUT("curl -fsSL --max-time 60 " + g_url + " -o " + CHR$(34) + g_cache + CHR$(34))
  IF FILEEXISTS(g_cache) THEN RETURN IO.READFILE(g_cache)
  RETURN ""
END FUNCTION

' ---- parsing Appendice B -----------------------------------------------------
SUB ParseHuffman(t AS STRING)
  DIM ln$() AS STRING, i AS LONG, line AS STRING, p AS LONG, q AS LONG
  DIM cp AS LONG, bits AS STRING, codeHex AS STRING, lenStr AS STRING, c AS STRING
  DIM blen AS LONG, v AS LONG, k AS LONG, cpStr AS STRING
  ln$ = SPLIT(t, CHR$(10))
  IF UBOUND(ln$) < 0 THEN EXIT SUB
  FOR i = 0 TO UBOUND(ln$)
    line = ln$(i)
    p = INSTR(line, "(")
    IF p > 0 THEN
      q = INSTR(p + 1, line, ")")
      IF q > p THEN
        cpStr = TrimS(MID$(line, p + 1, q - p - 1))
        IF LEN(cpStr) > 0 AND IsDigitCh(LEFT$(cpStr, 1)) <> 0 THEN
          cp = VAL(cpStr)
          ' pattern di bit dopo la ')'
          p = INSTR(q, line, "|")
          IF p > 0 THEN
            k = p
            bits = ""
            DO WHILE k <= LEN(line)
              c = MID$(line, k, 1)
              IF c = "0" OR c = "1" THEN
                bits = bits + c
                k = k + 1
              ELSEIF c = "|" THEN
                k = k + 1
              ELSE
                EXIT DO
              END IF
            LOOP
            ' salta spazi, poi codice esadecimale
            DO WHILE k <= LEN(line) AND MID$(line, k, 1) = " "
              k = k + 1
            LOOP
            codeHex = ""
            DO WHILE k <= LEN(line) AND IsHexCh(MID$(line, k, 1)) <> 0
              codeHex = codeHex + MID$(line, k, 1)
              k = k + 1
            LOOP
            ' lunghezza tra parentesi quadre
            lenStr = ""
            p = INSTR(k, line, "[")
            IF p > 0 THEN
              q = INSTR(p, line, "]")
              IF q > p THEN lenStr = TrimS(MID$(line, p + 1, q - p - 1))
            END IF
            IF LEN(bits) > 0 AND LEN(codeHex) > 0 AND LEN(lenStr) > 0 THEN
              blen = VAL(lenStr)
              v = 0
              FOR k = 1 TO LEN(bits)
                v = v * 2
                IF MID$(bits, k, 1) = "1" THEN v = v + 1
              NEXT k
              IF cp >= 0 AND cp <= 256 THEN
                g_hcode(cp) = v
                g_hbits(cp) = blen
                g_hhave(cp) = -1
              END IF
            END IF
          END IF
        END IF
      END IF
    END IF
  NEXT i
END SUB

' ---- parsing Appendice A -----------------------------------------------------
SUB ParseStatic(t AS STRING)
  DIM st AS LONG, en AS LONG, seg AS STRING, ln$() AS STRING, i AS LONG
  DIM line AS STRING, f$() AS STRING, idx AS LONG, nm AS STRING, vl AS STRING
  st = INSTR(t, "Appendix A.  Static Table Definition")
  IF st = 0 THEN st = INSTR(t, "Static Table Definition")
  en = INSTR(t, "Appendix B.  Huffman Code")
  IF st = 0 THEN EXIT SUB
  IF en > st THEN
    seg = MID$(t, st, en - st)
  ELSE
    seg = MID$(t, st)
  END IF
  ln$ = SPLIT(seg, CHR$(10))
  FOR i = 0 TO UBOUND(ln$)
    line = TrimS(ln$(i))
    IF LEFT$(line, 1) = "|" THEN
      f$ = SPLIT(line, "|")
      IF UBOUND(f$) >= 4 THEN
        IF LEN(TrimS(f$(1))) > 0 AND IsDigitCh(LEFT$(TrimS(f$(1)), 1)) <> 0 THEN
          idx = VAL(TrimS(f$(1)))
          nm = TrimS(f$(2))
          vl = TrimS(f$(3))
          IF idx >= 0 AND idx <= 127 THEN
            g_sidx(idx) = idx
            g_sname(idx) = nm
            g_sval(idx) = vl
            g_shave(idx) = -1
          END IF
        END IF
      END IF
    END IF
  NEXT i
END SUB

' ---- generazione -------------------------------------------------------------
FUNCTION GenHuffman() AS STRING
  DIM out AS STRING, sym AS LONG
  out = "/* GENERATO da tools/basic/gen_hpack_huffman.bas — NON modificare a mano." + CHR$(10)
  out = out + " * Tabella dei codici Huffman di HPACK (RFC 7541, Appendice B): 256 simboli + EOS (256)." + CHR$(10)
  out = out + " * `code` è allineato ai bit ALTI (come nell'RFC); `bits` è la lunghezza in bit. */" + CHR$(10)
  out = out + "#ifndef GMB_HPACK_HUFFMAN_H" + CHR$(10)
  out = out + "#define GMB_HPACK_HUFFMAN_H" + CHR$(10)
  out = out + CHR$(10)
  out = out + "typedef struct { unsigned int code; unsigned char bits; } gmb_hpack_huff_t;" + CHR$(10)
  out = out + CHR$(10)
  out = out + "static const gmb_hpack_huff_t gmb_hpack_huffman[257] = {" + CHR$(10)
  FOR sym = 0 TO 256
    out = out + "    { 0x" + Hex8(g_hcode(sym)) + "u, " + Pad2(g_hbits(sym)) + " },   /* " + LTRIM$(STR$(sym)) + " */" + CHR$(10)
  NEXT sym
  out = out + "};" + CHR$(10)
  out = out + CHR$(10)
  out = out + "#endif /* GMB_HPACK_HUFFMAN_H */" + CHR$(10)
  RETURN out
END FUNCTION

FUNCTION GenStatic() AS STRING
  DIM out AS STRING, idx AS LONG
  out = "/* GENERATO da tools/basic/gen_hpack_huffman.bas — NON modificare a mano." + CHR$(10)
  out = out + " * Tabella STATICA di HPACK (RFC 7541, Appendice A): indice 1..61 (0 = vuoto). */" + CHR$(10)
  out = out + "#ifndef GMB_HPACK_STATIC_H" + CHR$(10)
  out = out + "#define GMB_HPACK_STATIC_H" + CHR$(10)
  out = out + CHR$(10)
  out = out + "typedef struct { const char* name; const char* value; } gmb_hpack_static_t;" + CHR$(10)
  out = out + CHR$(10)
  out = out + "static const gmb_hpack_static_t gmb_hpack_static[62] = {" + CHR$(10)
  out = out + "    { NULL, NULL },   /* 0: non usato */" + CHR$(10)
  FOR idx = 1 TO 61
    out = out + "    { " + CHR$(34) + g_sname(idx) + CHR$(34) + ", " + CHR$(34) + g_sval(idx) + CHR$(34) + " },   /* " + LTRIM$(STR$(idx)) + " */" + CHR$(10)
  NEXT idx
  out = out + "};" + CHR$(10)
  out = out + CHR$(10)
  out = out + "#endif /* GMB_HPACK_STATIC_H */" + CHR$(10)
  RETURN out
END FUNCTION

SUB WriteIf(dst AS STRING, body AS STRING)
  DIM h AS LONG, wr AS LONG
  wr = SYSTEM.SHELLOUT("rm -f " + CHR$(34) + dst + CHR$(34))
  h = IO.FCREATE(dst)
  wr = IO.FWRITE(h, body)
  IO.FCLOSE(h)
END SUB

FUNCTION Main() AS INTEGER
  DIM a AS STRING, i AS LONG, src AS STRING, rfcLocal AS STRING
  DIM t AS STRING, hAll AS STRING, stAll AS STRING, cur AS STRING
  DIM checkOnly AS LONG, rc AS LONG, nH AS LONG, nS AS LONG

  SET SHELL ON
  g_url = "https://www.rfc-editor.org/rfc/rfc7541.txt"
  g_cache = "/tmp/rfc7541.txt"
  checkOnly = 0
  rfcLocal = ""
  IF DIREXISTS(IO.PARENTDIR(IO.PARENTDIR(IO.CWD())) + "/GMBasic/native/gmb_runtime_c/src") THEN
    src = IO.PARENTDIR(IO.PARENTDIR(IO.CWD())) + "/GMBasic/native/gmb_runtime_c/src"
  ELSE
    src = "GMBasic/native/gmb_runtime_c/src"
  END IF
  i = 1
  DO WHILE i <= COMMANDCOUNT()
    a = COMMAND$(i)
    IF a = "--check" THEN checkOnly = -1
    IF a = "--rfc" AND i < COMMANDCOUNT() THEN
      i = i + 1
      rfcLocal = COMMAND$(i)
    END IF
    IF a = "--src" AND i < COMMANDCOUNT() THEN
      i = i + 1
      src = COMMAND$(i)
    END IF
    i = i + 1
  LOOP

  t = RfcText(rfcLocal)
  IF LEN(t) = 0 THEN
    PRINT "ERRORE: RFC 7541 non disponibile (rete assente e nessuna copia locale)"
    RETURN 1
  END IF
  ParseHuffman(t)
  ParseStatic(t)

  nH = 0
  FOR i = 0 TO 256
    IF g_hhave(i) <> 0 THEN nH = nH + 1
  NEXT i
  nS = 0
  FOR i = 1 TO 61
    IF g_shave(i) <> 0 THEN nS = nS + 1
  NEXT i
  IF nH <> 257 THEN
    PRINT "ERRORE: tabella Huffman incompleta ("; nH; "/257)"
    RETURN 1
  END IF
  IF nS <> 61 THEN
    PRINT "ERRORE: tabella statica incompleta ("; nS; "/61)"
    RETURN 1
  END IF

  hAll = GenHuffman()
  stAll = GenStatic()

  rc = 0
  IF checkOnly <> 0 THEN
    cur = ""
    IF FILEEXISTS(src + "/gmb_hpack_huffman.h") THEN cur = IO.READFILE(src + "/gmb_hpack_huffman.h")
    IF TrimS(cur) <> TrimS(hAll) THEN
      PRINT "DIVERSO: "; src; "/gmb_hpack_huffman.h"
      rc = 1
    ELSE
      PRINT "OK      "; src; "/gmb_hpack_huffman.h"
    END IF
    cur = ""
    IF FILEEXISTS(src + "/gmb_hpack_static.h") THEN cur = IO.READFILE(src + "/gmb_hpack_static.h")
    IF TrimS(cur) <> TrimS(stAll) THEN
      PRINT "DIVERSO: "; src; "/gmb_hpack_static.h"
      rc = 1
    ELSE
      PRINT "OK      "; src; "/gmb_hpack_static.h"
    END IF
    RETURN rc
  END IF

  WriteIf(src + "/gmb_hpack_huffman.h", hAll)
  PRINT "scritto "; src; "/gmb_hpack_huffman.h"
  WriteIf(src + "/gmb_hpack_static.h", stAll)
  PRINT "scritto "; src; "/gmb_hpack_static.h"
  RETURN 0
END FUNCTION
