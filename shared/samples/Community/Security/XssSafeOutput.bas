' FILE: Security/XssSafeOutput.bas
' =================================================================================================
'       SECURITY — output HTML XSS-safe (STRINGS.HTMLENCODE)
' =================================================================================================
' Dimostra il pattern anti-XSS (lo stesso che BspCompiler ora applica in automatico a
' <%= %>/@Model): ogni dato utente in uscita HTML va HTML-encoded. Verifica in BASIC che il
' payload malevolo NON sopravviva grezzo e che compaiano le entita'.
' Eseguibile su IL, PCodeVM e VM/C (C99Native: gap dichiarato 2026-09-02).

IMPORT STRINGS

PRINT "=== Anti-XSS ==="

DIM attacker$ = "<script>alert('x')</script>"
DIM safe$ = STRINGS.HTMLENCODE(attacker$)
PRINT "raw:  "; attacker$
PRINT "safe: "; safe$

IF INSTR(safe$, "<script>") = 0 THEN
    IF INSTR(safe$, "&lt;script&gt;") > 0 THEN
        PRINT "XSS SAFE OK"
    ELSE
        PRINT "XSS SAFE FAIL"
    END IF
ELSE
    PRINT "XSS SAFE FAIL"
END IF

' URL dentro un attributo href/link: percent-encode
DIM link$ = "https://host/path?q=" + STRINGS.URLENCODE("a b&c=d")
PRINT "link: "; link$
IF INSTR(link$, " ") = 0 AND INSTR(link$, "%20") > 0 THEN
    PRINT "URL SAFE OK"
ELSE
    PRINT "URL SAFE FAIL"
END IF

PRINT "XssSafeOutput demo done"
