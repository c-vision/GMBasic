' FILE: Browser/BrowserGetCookies.bas
' =================================================================================================
' BROWSER.GETCOOKIES() — recupera i cookies del browser CUA
' Richiede un browser avviato (BROWSER.START).
' NOTA: senza browser attivo restituisce errore.
' =================================================================================================
IMPORT BROWSER

SUB Main()
    CLS
    PRINT "=== BROWSER.GETCOOKIES ==="
    DIM cookies$ = BROWSER.GETCOOKIES()
    PRINT "Cookies: "; LEFT$(cookies$, 200)
    PRINT "=== OK ==="
END SUB