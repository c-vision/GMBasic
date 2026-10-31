' CUA — AgentWorkflow: sessione browser multi-step + computer use, deterministico.
IMPORT BROWSER WITH ALIAS
IMPORT COMPUTER WITH ALIAS

SUB Main()
  PRINT "start="; BROWSER.START(%AUTO, 9222)

  PRINT "nav1="; BROWSER.NAVIGATE("https://example.com")
  DIM t1 AS STRING
  DIM k AS INTEGER
  t1 = ""
  FOR k = 1 TO 50
    t1 = BROWSER.EVALUATE("document.title")
    IF LEN(t1) > 0 THEN EXIT FOR
    SLEEP 200
  NEXT k
  PRINT "title1="; t1
  PRINT "h11="; LEFT$(BROWSER.EVALUATE("(document.querySelector('h1')||{}).innerText||''"), 60)

  PRINT "nav2="; BROWSER.NAVIGATE("https://example.org")
  DIM t2 AS STRING
  t2 = ""
  FOR k = 1 TO 50
    t2 = BROWSER.EVALUATE("document.title")
    IF LEN(t2) > 0 THEN EXIT FOR
    SLEEP 200
  NEXT k
  PRINT "title2="; t2

  DIM win AS STRING
  win = COMPUTER.WINDOWS()
  PRINT "windows_len="; LEN(win)

  DIM okc AS INTEGER
  okc = 0
  IF LEN(t1) > 0 THEN okc = okc + 1
  IF LEN(t2) > 0 THEN okc = okc + 1

  IF okc = 2 AND LEN(win) > 2 THEN
    PRINT "AGENT WORKFLOW OK: pages=2 windows="; LEN(win)
  ELSE
    PRINT "AGENT WORKFLOW PARZIALE okc="; okc
  END IF
END SUB
