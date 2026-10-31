' BROWSER.NAVIGATE/SNAPSHOT/CLICK/FILL/EVALUATE — CDP via Obscura/Chromium (Universal Action Runtime)
' BROWSER.START([engine],[port]) lancia il browser se non già su CDP (Obscura se in cache,
' altrimenti Chromium via launch_chromium.sh); porta default 9222 (env GMBASIC_CDP_PORT).
IMPORT BROWSER WITH ALIAS

SUB Main()
  PRINT "start="; BROWSER.START(%AUTO, 9222)
  PRINT "navigate="; BROWSER.NAVIGATE("https://example.com")

  ' attesa caricamento con retry su document.title
  DIM i AS INTEGER
  DIM title$ AS STRING
  FOR i = 1 TO 30
    title$ = BROWSER.EVALUATE("document.title")
    IF LEN(title$) > 0 THEN EXIT FOR
    SLEEP 200
  NEXT i

  PRINT "title="; title$
  IF title$ = "Example Domain" THEN
    PRINT "BROWSER OK: titolo atteso"
  ELSE
    PRINT "BROWSER FAIL: titolo inatteso (len="; LEN(title$); ")"
  END IF

  ' snapshot DOM → controlla che contenga l'h1
  DIM snap$ AS STRING
  snap$ = BROWSER.SNAPSHOT()
  IF INSTR(snap$, "Example Domain") > 0 THEN PRINT "SNAPSHOT OK: h1 presente" ELSE PRINT "SNAPSHOT FAIL (len="; LEN(snap$); ")"

  ' azioni di servizio (selettori inesistenti → ok=false senza crash)
  PRINT "click="; BROWSER.CLICK("#non-esiste")
  PRINT "fill="; BROWSER.FILL("input#non-esiste", "ciao")
  PRINT "clearcookies="; BROWSER.CLEARCOOKIES()
  PRINT "humanfill="; BROWSER.HUMANFILL("input#non-esiste", "ciao")
END SUB
