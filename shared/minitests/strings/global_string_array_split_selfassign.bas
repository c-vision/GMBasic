' Array STRINGA GLOBALE (DIM SYM$(n)) popolato in una funzione da SPLIT e letto in un'altra, con l'idioma
' di binding dei PARAM (`nleg = nleg`, `k_atr = k_atr`) nella funzione che popola. Scenario della nota
' (open) C99Native-Array-Stringa-Globale-Persa: il C99 lasciava l'array vuoto; qui deve restare popolato.
' EXPECT: c= 7  S0=[BTCUSDT] S2=[BNBUSDT] S6=[DOGEUSDT]|idx= 0  4  6 -1|stable= 7  S0=[BTCUSDT]
DIM nleg = 7
DIM k_atr = 14
DIM SYM$(16) AS STRING
DIM NSYM

FUNCTION SRCOF(k AS DOUBLE) AS STRING
  IF k = 0 THEN RETURN "BTCUSDT,ETHUSDT,BNBUSDT"
  IF k = 1 THEN RETURN "SOLUSDT,ADAUSDT"
  RETURN "XRPUSDT,DOGEUSDT"
END FUNCTION

FUNCTION PARSE_SYMS() AS DOUBLE
  DIM tok() AS STRING
  DIM c, k, i, nn
  nleg = nleg
  k_atr = k_atr
  c = 0
  FOR k = 0 TO 2
    tok() = SPLIT(SRCOF(k), ",")
    nn = UBOUND(tok)
    FOR i = 0 TO nn
      IF c < 16 THEN
        SYM$(c) = tok(i)
      END IF
      c = c + 1
    NEXT i
  NEXT k
  NSYM = c
  RETURN c
END FUNCTION

FUNCTION LEGIDX(s$) AS DOUBLE
  DIM i
  FOR i = 0 TO NSYM - 1
    IF SYM$(i) = s$ THEN RETURN i
  NEXT i
  RETURN -1
END FUNCTION

FUNCTION TOUCH() AS DOUBLE
  nleg = nleg
  RETURN nleg
END FUNCTION

SUB Main()
  DIM c, t
  c = PARSE_SYMS()
  PRINT "c="; c; " S0=["; SYM$(0); "] S2=["; SYM$(2); "] S6=["; SYM$(6); "]"
  PRINT "idx="; LEGIDX("BTCUSDT"); LEGIDX("ADAUSDT"); LEGIDX("DOGEUSDT"); LEGIDX("NOPE")
  t = TOUCH()
  PRINT "stable="; NSYM; " S0=["; SYM$(0); "]"
END SUB
