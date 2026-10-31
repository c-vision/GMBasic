' FILE: Grep/GrepSearch.bas
' GREP — ricerca file (MATCHES/COUNT/FILES), ritorno ARRAY di stringhe.
IMPORT GREP

SUB Main()
    DIM self AS STRING
    self = PROJECTFOLDER() + "/GrepSearch.bas"
    PRINT "================================================"
    PRINT "GREP - ricerca in: "; self
    PRINT "================================================"
    PRINT "COUNT SUB Main: "; GREP.COUNT("SUB Main", self)
    PRINT
    DIM hits() AS STRING
    hits = GREP.MATCHES("SUB Main", self)
    PRINT "MATCHES SUB Main ("; UBOUND(hits) + 1; " hit):"
    FOR i = 0 TO UBOUND(hits)
        PRINT "  "; hits[i]
    NEXT
    PRINT
    DIM files() AS STRING
    files = GREP.FILES("GREP", self)
    PRINT "FILES GREP ("; UBOUND(files) + 1; " file):"
    FOR i = 0 TO UBOUND(files)
        PRINT "  "; files[i]
    NEXT
END SUB
