' MiniTest NAMESPACE FONTS — tutte e 3 le funzioni della superficie.
' TTMETRICS (8 valori: ascender/descender/advance/bearing...), TTRENDER (bitmap),
' TTGLYPHS (elenco glifi) su un VERO font della fixture (Sixtyfour, OFL) + contratto
' d'errore con font assente (liste vuote, nessun errore fatale).
' Gli indici delle liste non sono stampati: su C99 l'indicizzazione di una lista
' restituita da funzione non è emessa (gap noto); si verificano le LUNGHEZZE, identiche
' sui 4 backend, più il caso "font assente".
' EXPECT: metrics_n= 8|render_n= 196|glyphs_n= 470|missing_n= 0
IMPORT FONTS

DIM g_font AS STRING
g_font = "../../samples/Fixtures/fonts/Sixtyfour.ttf"

SUB Main()
  DIM m()
  m = FONTS.TTMETRICS(g_font, 16, 65)
  PRINT "metrics_n="; UBOUND(m) + 1

  DIM b()
  b = FONTS.TTRENDER(g_font, 16, 65)
  PRINT "render_n="; UBOUND(b) + 1

  DIM g()
  g = FONTS.TTGLYPHS(g_font)
  PRINT "glyphs_n="; UBOUND(g) + 1

  DIM m2()
  m2 = FONTS.TTMETRICS("/tmp/gmb_fonts_non_esiste.ttf", 16, 65)
  PRINT "missing_n="; UBOUND(m2) + 1
END SUB
