' FILE: Security/EncodingDemo.bas
' =================================================================================================
'       SECURITY — Encoding HTML / URL / JS (anti-XSS, BSP safe output)
' =================================================================================================
' Verifica end-to-end di STRINGS.HTMLENCODE / URLENCODE / JSENCODE (2026-09-02).
' Set canonico (non WebUtility): HTML = 5 entita' + passthrough non-ASCII; URL = percent RFC
' 3986 (unreserved conservati, %HH maiuscolo); JS = escape di \ " ' e controlli C0.
' Eseguibile su IL, PCodeVM e VM/C (C99Native: gap dichiarato 2026-09-02).

IMPORT STRINGS

PRINT "=== HTMLENCODE ==="
DIM rawHtml$ = "<a href=" + CHR$(34) + "x" + CHR$(34) + ">Tom & Jerry 'ok'</a>"
PRINT "input:  "; rawHtml$
PRINT "output: "; STRINGS.HTMLENCODE(rawHtml$)
IF STRINGS.HTMLENCODE("<b>x</b>") = "&lt;b&gt;x&lt;/b&gt;" THEN
    PRINT "HTML ENCODE OK"
ELSE
    PRINT "HTML ENCODE FAIL"
END IF

PRINT "=== URLENCODE ==="
PRINT "output: "; STRINGS.URLENCODE("a b&c=d/x?y")
IF STRINGS.URLENCODE("a b&c=d/x?y") = "a%20b%26c%3Dd%2Fx%3Fy" THEN
    PRINT "URL ENCODE OK"
ELSE
    PRINT "URL ENCODE FAIL"
END IF

PRINT "=== JSENCODE ==="
DIM jsQuote$ = "a" + CHR$(34) + "b"
PRINT "quote: "; STRINGS.JSENCODE(jsQuote$)
PRINT "lf:    "; STRINGS.JSENCODE("x" + CHR$(10) + "y")
PRINT "apos:  "; STRINGS.JSENCODE("it's ok")
