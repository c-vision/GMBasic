' VIEW PRINT test
' Sets scroll region and prints within it
'
' VIEW PRINT richiede il buffer a griglia: StandardConsole si apre di default in modalita'
' NATIVE (scrollback reale del terminale, nessun buffer), quindi va attivata esplicitamente
' la modalita' BUFFERED prima di usare VIEW PRINT (altrimenti l'istruzione fallisce con un
' errore runtime).
SCROLLMODE "BUFFERED"

COLOR 15, 1
CLS

' Print header lines that should stay fixed
LOCATE 1, 1: PRINT "=== HEADER (FIXED) ==="
LOCATE 2, 1: PRINT "Line 2 - fixed"
LOCATE 3, 1: PRINT "Line 3 - fixed"

' Set scroll region to lines 4-20
VIEW PRINT 4 TO 20

' Print many lines that scroll within region
FOR i = 1 TO 30
    PRINT "Scrolling line "; i
NEXT i

' Reset to full-screen scroll
VIEW PRINT

PRINT "After reset - this scrolls normally"
PRINT "Done"
