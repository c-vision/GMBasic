' CUA — CuaDiagnostics: verifica ogni sottosistema del runtime agente e stampa un report.
' NON è un test "stub": interroga davvero browser (CDP), tool registry, computer (finestre)
' e stato del brain, con esito per componente. Il browser va attivo (BROWSER.START).
IMPORT BROWSER WITH ALIAS
IMPORT TOOL WITH ALIAS
IMPORT COMPUTER WITH ALIAS
IMPORT AGENT WITH ALIAS

FUNCTION Has(h AS STRING, needle AS STRING) AS INTEGER
  Has = 0
  IF INSTR(h, needle) > 0 THEN Has = -1
END FUNCTION

SUB Main()
  DIM ok AS INTEGER
  DIM warn AS INTEGER
  ok = 0
  warn = 0

  ' 1) Browser
  DIM st AS STRING
  st = BROWSER.START(%AUTO, 9222)
  PRINT "browser="; st
  IF Has(st, "connected") <> 0 OR Has(st, "navigated") <> 0 OR Has(st, "already") <> 0 THEN
    ok = ok + 1
  ELSE
    warn = warn + 1
  END IF

  ' 2) Tool registry (per dominio)
  DIM tb AS STRING
  DIM tc AS STRING
  DIM tf AS STRING
  tb = TOOL.SEARCH("browser")
  tc = TOOL.SEARCH("computer")
  tf = TOOL.SEARCH("files")
  PRINT "tools_browser="; Has(tb, "browser.navigate") <> 0
  PRINT "tools_computer="; Has(tc, "computer.click") <> 0
  PRINT "tools_files="; Has(tf, "files.read") <> 0
  IF Has(tb, "browser.navigate") <> 0 AND Has(tc, "computer.click") <> 0 THEN ok = ok + 1 ELSE warn = warn + 1

  ' 3) Computer (finestre reali)
  DIM win AS STRING
  win = COMPUTER.WINDOWS()
  PRINT "windows_len="; LEN(win)
  IF LEN(win) > 2 THEN ok = ok + 1 ELSE warn = warn + 1

  ' 4) Brain agente (loop C): senza brain/embedding -> errore esplicito (atteso)
  DIM ag AS STRING
  ag = AGENT.RUN("test", 1)
  PRINT "agent_brain="; LEFT$(ag, 80)

  PRINT "report ok="; ok; " warn="; warn
END SUB
