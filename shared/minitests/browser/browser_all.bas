' MiniTest NAMESPACE BROWSER — superficie COMPLETA (21 funzioni), parità di superficie.
' Le funzioni che avviano/guidano Chromium (NAVIGATE/CLICK/EVALUATE/HUMAN*/SCREENSHOT...)
' sono compilate e linkate nella stessa forma sui 4 backend (ramo IF 0) ma NON eseguite:
' aprire un browser reale in un test automatico è invasivo e non deterministico. AUTH è
' eseguita davvero (lettura della directory dei profili, nessun browser).
' EXPECT: auth_len>=0=-1
IMPORT BROWSER

SUB Main()
  DIM a AS STRING
  a = BROWSER.AUTH("list", "")
  PRINT "auth_len>=0="; (LEN(a) >= 0)

  IF 0 THEN
    _b1 = BROWSER.AccessTree()
    _b2 = BROWSER.Auth("", "")
    _b3 = BROWSER.ClearCookies()
    _b4 = BROWSER.Click("")
    _b5 = BROWSER.Console("")
    _b6 = BROWSER.Evaluate("")
    _b7 = BROWSER.Fill("", "")
    _b8 = BROWSER.Find("", "")
    _b9 = BROWSER.GetCookies()
    _b10 = BROWSER.HumanClick("")
    _b11 = BROWSER.HumanFill("", "")
    _b12 = BROWSER.HumanScroll("")
    _b13 = BROWSER.Navigate("")
    _b14 = BROWSER.NetLog("")
    _b15 = BROWSER.Observe("")
    _b16 = BROWSER.Screenshot("")
    _b17 = BROWSER.SetCookie("", "", "")
    _b18 = BROWSER.SetHeader("", "")
    _b19 = BROWSER.SetUserAgent("")
    _b20 = BROWSER.Snapshot()
    _b21 = BROWSER.Start("", "", "")
  END IF
END SUB
