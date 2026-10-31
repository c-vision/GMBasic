' FILE: Browser/BrowserSetUserAgent.bas
' =================================================================================================
' BROWSER.SETUSERAGENT(userAgent$) — imposta lo User-Agent per il browser CUA
' NOTA: senza browser attivo restituisce errore.
' =================================================================================================
IMPORT BROWSER

SUB Main()
    CLS
    PRINT "=== BROWSER.SETUSERAGENT ==="
    DIM result$ = BROWSER.SETUSERAGENT("Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7)")
    PRINT "Result: "; LEFT$(result$, 100)
    PRINT "=== OK ==="
END SUB