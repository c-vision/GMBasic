' PARITY-EXCLUDE: nondeterministico: finestre OS live (screenshot/AX della macchina)
' COMPUTER USE L0 — screenshot/click/type/key/windows reali (ComputerProvider macOS)
' NOTA permessi: eventi → Accessibilità, screenshot → Screen Recording (Impostazioni > Privacy).
' Il sample usa solo azioni sicure (screenshot + finestre); click/type vanno abilitati a mano.
IMPORT COMPUTER WITH ALIAS

SUB Main()
  ' Elenco finestre visibili (JSON: id, pid, title, owner)
  DIM w$ AS STRING
  w$ = COMPUTER.WINDOWS()
  PRINT "windows="; LEFT$(w$, 160)

  ' Screenshot → PNG in work/ (Screen Recording permission)
  DIM shot$ AS STRING
  shot$ = COMPUTER.SCREENSHOT("work/computer_shot.png")
  PRINT "screenshot="; shot$
  IF LEFT$(shot$, 5) <> "error" THEN
    IF FILEEXISTS("work/computer_shot.png") THEN PRINT "SCREENSHOT OK" ELSE PRINT "SCREENSHOT FAIL (file mancante)"
  END IF

  ' Azioni mouse/tastiera: richiedono Accessibilità.
  ' PRINT "click="; COMPUTER.CLICK(100, 100)
  ' PRINT "dblclick="; COMPUTER.DOUBLECLICK(100, 100)
  ' PRINT "type="; COMPUTER.TYPE("ciao")
  ' PRINT "key="; COMPUTER.KEY("Return")
  ' PRINT "hotkey="; COMPUTER.HOTKEY("ctrl+alt+shift+z")   ' combo: -1 se malformata

  PRINT "ComputerUse completato."
END SUB
