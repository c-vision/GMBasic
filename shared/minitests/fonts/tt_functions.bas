' FONTS.TTRENDER / FONTS.TTMETRICS / FONTS.TTGLYPHS sono FUNZIONI (parentesi obbligatorie,
' non scrivono nulla). Su un font assente ritornano liste VUOTE, senza errore fatale: il
' programma prosegue identico sui 4 backend (IL, PCodeVM, VM/C, C99Native).
' La verifica numerica su un font reale sta in tools/c/gmb_fonts_selftest.c (non dipende da
' un font installato sulla macchina, quindi non entra nell'oracolo del MiniTest).
' EXPECT: metrics_ok|render_ok|glyphs_ok|fine
IMPORT FONTS

DIM m()
m = FONTS.TTMETRICS("/tmp/gmb_fonts_non_esiste.ttf", 16, 65)
PRINT "metrics_ok"

DIM b()
b = FONTS.TTRENDER("/tmp/gmb_fonts_non_esiste.ttf", 16, 65)
PRINT "render_ok"

DIM g()
g = FONTS.TTGLYPHS("/tmp/gmb_fonts_non_esiste.ttf")
PRINT "glyphs_ok"

PRINT "fine"
