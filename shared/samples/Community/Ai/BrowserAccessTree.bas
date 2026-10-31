' PARITY-EXCLUDE: browser/CDP: AX tree reale Chromium vs mock headless C
IMPORT BROWSER
SUB Main()
  PRINT "nav="; BROWSER.NAVIGATE("data:text/html,<button>OK</button>")
  PRINT "ax-begin"
  PRINT BROWSER.ACCESSTREE()
  PRINT "ax-end"
END SUB
