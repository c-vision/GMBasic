' BrowserStealth.bas — anti-detection (stealth) applicata automaticamente alla connessione
' della sessione (funzione avanzata Camoufox via CDP puro). Verifica che dopo una nuova
' navigazione navigator.webdriver sia assente/falso e i fingerprint spoofati.
' NOTA: stealth è implementata nel client CDP managed (IL/PCode). Richiede un browser CDP
' attivo (Chromium/Obscura su GMBASIC_CDP_PORT, default 9222).
IMPORT BROWSER
SUB Main()
  PRINT "nav="; BROWSER.NAVIGATE("data:text/html,<h1>st</h1>")
  ' navigator.webdriver deve essere false/undefined (anti-detection)
  PRINT "webdriver-defined="; BROWSER.EVALUATE("String(navigator.webdriver !== undefined && navigator.webdriver !== false)")
  PRINT "maxTouchPoints="; BROWSER.EVALUATE("String(navigator.maxTouchPoints)")
  PRINT "languages="; BROWSER.EVALUATE("String(navigator.languages)")
  PRINT "OK-stealth"
END SUB
