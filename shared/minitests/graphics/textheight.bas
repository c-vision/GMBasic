' EXPECT: h-ok|w-ok|width-lineare|stabile
' TEXTHEIGHT/TEXTWIDTH headless: altezza e larghezza non nulle, larghezza lineare nel
' numero di caratteri, altezza stabile fra due chiamate. Il valore assoluto (19) e' fissato
' da graphics/metrics.bas; qui si verificano le proprieta' che un "ritorno 0" violava.
IMPORT GRAPHICS
h1 = TextHeight()
w1 = TextWidth("M")
w3 = TextWidth("MMM")
IF h1 > 0 THEN PRINT "h-ok" ELSE PRINT "h-FAIL"
IF w1 > 0 THEN PRINT "w-ok" ELSE PRINT "w-FAIL"
IF w3 = 3 * w1 THEN PRINT "width-lineare" ELSE PRINT "width-NON-lineare"
h2 = TextHeight()
IF h2 = h1 THEN PRINT "stabile" ELSE PRINT "INSTABILE"
