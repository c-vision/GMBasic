' MiniTest FONTS: una funzione che ritorna LISTA usata in tre forme — `DIM m() = call`,
' `DIM m()` + `m = call` (assegnazione separata) e UBOUND(call) in linea (nome nudo e con IMPORT SYSTEM).
' EXPECT: a= 8   14|b= 8   14|c= 8|d= 8
IMPORT SYSTEM
IMPORT FONTS

DIM g_font AS STRING
g_font = "../../samples/Fixtures/fonts/Sixtyfour.ttf"

SUB Main()
  DIM a() = FONTS.TTMETRICS(g_font, 16, 65)
  DIM b()
  b = FONTS.TTMETRICS(g_font, 16, 65)
  PRINT "a="; UBOUND(a) + 1; " "; a(0)
  PRINT "b="; UBOUND(b) + 1; " "; b(0)
  PRINT "c="; UBOUND(FONTS.TTMETRICS(g_font, 16, 65)) + 1
  PRINT "d="; SYSTEM.UBOUND(FONTS.TTMETRICS(g_font, 16, 65)) + 1
END SUB
