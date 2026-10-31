' COMPUTER INPUT — DOUBLECLICK + HOTKEY (CUA #7 ASTRA: primitive 2 e 12 di 12)
' Il sample esercita le due funzioni del namespace COMPUTER su percorsi reali e
' fail-closed: gli esiti -1 di HOTKEY su combo malformata sono DETERMINISTICI anche
' senza permesso di Accessibilità; i percorsi interattivi accettano 0 o -1 (permesso).
' Permessi macOS: eventi → Impostazioni > Privacy e sicurezza > Accessibilità.
IMPORT COMPUTER WITH ALIAS

FUNCTION Describe(r AS INTEGER) AS STRING
  IF r = 0 THEN
    Describe = "ok"
  ELSE
    IF r = -1 THEN
      Describe = "negato o malformato (-1)"
    ELSE
      Describe = "shape inatteso " + LTRIM(STR$(r))
    END IF
  END IF
END FUNCTION

' 1 = il valore è un esito ammesso (0 ok / -1 errore), 0 = fallimento di shape.
FUNCTION ShapeOk(r AS INTEGER) AS INTEGER
  IF r = 0 OR r = -1 THEN
    ShapeOk = 1
  ELSE
    ShapeOk = 0
  END IF
END FUNCTION

SUB Main()
  DIM fails AS INTEGER
  fails = 0

  PRINT "== contesto: COMPUTER.WINDOWS() =="
  DIM w$ AS STRING
  w$ = COMPUTER.WINDOWS()
  IF LEN(w$) > 2 THEN
    PRINT "elenco finestre: presente"
  ELSE
    PRINT "elenco finestre: vuoto"
  END IF

  PRINT "== DOUBLECLICK interattivo (doppio click reale, coordinate innocue) =="
  DIM d AS INTEGER
  d = COMPUTER.DOUBLECLICK(0, 0)
  PRINT "DOUBLECLICK(0,0) -> "; d; " "; Describe(d)
  IF ShapeOk(d) = 0 THEN
    fails = fails + 1
  END IF

  PRINT "== HOTKEY fail-closed (nessun evento postato, deterministico) =="
  DIM r1 AS INTEGER, r2 AS INTEGER, r3 AS INTEGER, r4 AS INTEGER
  r1 = COMPUTER.HOTKEY("hyper+z")
  r2 = COMPUTER.HOTKEY("z")
  r3 = COMPUTER.HOTKEY("")
  r4 = COMPUTER.HOTKEY("ctrl+z+extra+bad-mod")
  PRINT "hyper+z -> "; r1
  PRINT "z (nessun modificatore) -> "; r2
  PRINT "stringa vuota -> "; r3
  PRINT "combo con modificatore ignoto -> "; r4
  IF r1 = -1 AND r2 = -1 AND r3 = -1 AND r4 = -1 THEN
    PRINT "fail-closed: OK (4/4 = -1)"
  ELSE
    PRINT "fail-closed: FAIL"
    fails = fails + 1
  END IF

  PRINT "== HOTKEY combo reale (tasto senza binding globale) =="
  DIM h AS INTEGER
  h = COMPUTER.HOTKEY("ctrl+alt+shift+j")
  PRINT "ctrl+alt+shift+j -> "; h; " "; Describe(h)
  IF ShapeOk(h) = 0 THEN
    fails = fails + 1
  END IF

  ' la grammatica è case-insensitive e tollera spazi attorno ai token
  DIM h2 AS INTEGER
  h2 = COMPUTER.HOTKEY("CTRL + ALT + SHIFT + J")
  PRINT "CTRL + ALT + SHIFT + J -> "; h2; " "; Describe(h2)
  IF ShapeOk(h2) = 0 THEN
    fails = fails + 1
  END IF

  IF fails = 0 THEN
    PRINT "ComputerInput completato: nessun fallimento."
  ELSE
    PRINT "ComputerInput: "; fails; " fallimento/i"
  END IF
END SUB
