' MiniTest NAMESPACE CONVERSIONS — tutti gli 11 simboli della superficie.
'
' Superficie (shared/spec/registry/surface.csv): BIN, CBYTE, CCUR, CDBL, CINT, CLNG,
' CSNG, CSTR, HEX, OCT, VAL.
'
' Scopo: verificare che i 4 backend supportino la STESSA versione del namespace,
' non un'applicazione. Oracolo deterministico (nessuna stringa dipendente da locale).
' NOTA: CINT arrotonda half-to-even (2.5 → 2, 3.5 → 4) su tutte le gambe.
' EXPECT: bin=101|hex=FF|oct=10|cint25= 2 |cint35= 4|clng= 42 |cdbl= 7 |csng= 2.5|cbyte= 65 |ccur= 12.5|cstrNum=[ 42]|cstrStr=[ab]|val= 42
IMPORT CONVERSIONS

SUB Main()
  ' ── basi numeriche ───────────────────────────────────────────────────────
  PRINT "bin="; BIN(5); "|hex="; HEX(255); "|oct="; OCT(8)

  ' ── conversioni numeriche ────────────────────────────────────────────────
  PRINT "cint25="; CINT(2.5); "|cint35="; CINT(3.5)
  PRINT "clng="; CLNG(42); "|cdbl="; CDBL(7); "|csng="; CSNG(2.5)
  PRINT "cbyte="; CBYTE(65); "|ccur="; ROUND(CCUR("12.5"), 2)

  ' ── conversioni da/per stringa ───────────────────────────────────────────
  PRINT "cstrNum=["; CSTR(42); "]|cstrStr=["; CSTR("ab"); "]"
  PRINT "val="; VAL("42")
END SUB
