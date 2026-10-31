' BrowserProfileStart.bas — isolamento sessione per-utente via profilo nominativo:
' BROWSER.START(engine, port, profile) lancia Chromium con un profilo separato in
' libs-runtime/chromium/profiles/<profile> (cookie/storage isolati, feature #3).
' NOTA: profilo gestito nel launch managed (IL/PCode). Usa una porta libera (non 9222) per
' forzare un avvio nuovo con quel profilo (warm-start su stessa porta lo riuserebbe).
IMPORT BROWSER
SUB Main()
  PRINT "start="; BROWSER.START(%CHROMIUM, 9255, "demo-userX")
  PRINT "nav="; BROWSER.NAVIGATE("data:text/html,<h1>profiled</h1>")
END SUB
