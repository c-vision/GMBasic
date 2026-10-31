' EXPECT: browser-ok
' BROWSER.START(%AUTO, porta) restituisce una STRINGA di stato che dipende dallo stato del
' processo: la prima gamba riceve "started", le successive "already running" — confrontare il
' testo fra le gambe rendeva il MiniTest FLAKY (falliva a freddo, passava a caldo). Si normalizza
' l'esito mantenendo la verifica: START deve restituire uno stato non vuoto.
' Chromium è presente in libs-runtime/chromium/<rid>/ (Chrome for Testing):
' nessuna dipendenza esterna da installare.
IMPORT BROWSER

SUB Main()
  DIM s AS STRING
  s = BROWSER.START(%AUTO, 9222)
  IF LEN(TRIM$(s)) > 0 THEN
    PRINT "browser-ok"
  ELSE
    PRINT "browser-ko"
  END IF
END SUB
