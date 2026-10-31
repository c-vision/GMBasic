' GETTEMPDIR / GETTEMPFILE sono i NOMI CANONICI (decisione utente 2026-10-27); GETTEMPFILE e
' GETTEMPDIR risolvono sullo stesso token di TEMPDIR / TEMPFILE (grafie sinonime approvate).
' Le grafie lunghe interne GetTempDirFunc / GetTempFileFunc NON sono accettate: il loro rifiuto
' e' verificato nei controlli negativi del protocollo (§4.1), non qui (un MiniTest deve girare).
'
' Nessun percorso assoluto viene stampato: la CWD/il temp dir differiscono tra le gambe.
' EXPECT: gettempdir=si|gettempfile=si|tempdir=si|tempfile=si|eq-dir=si|uniq=si|0-arg=si
IMPORT IO
SUB Main()
  DIM a = IO.GETTEMPDIR()
  DIM b = IO.GETTEMPFILE()
  DIM c = IO.TEMPDIR()
  DIM d = IO.TEMPFILE()

  IF LEN(a) > 0 THEN PRINT "gettempdir=si" ELSE PRINT "gettempdir=no"
  IF LEN(b) > 0 THEN PRINT "gettempfile=si" ELSE PRINT "gettempfile=no"
  IF LEN(c) > 0 THEN PRINT "tempdir=si" ELSE PRINT "tempdir=no"
  IF LEN(d) > 0 THEN PRINT "tempfile=si" ELSE PRINT "tempfile=no"

  ' TemPDIR e' stabile: le due grafie (canonica e sinonima) danno lo STESSO valore → stesso token.
  IF a = c THEN PRINT "eq-dir=si" ELSE PRINT "eq-dir=no"

  ' TEMPFILE invece genera un nome NUOVO a ogni chiamata: due chiamate della stessa grafia
  ' differiscono come due chiamate di grafie diverse (stessa funzione, stessa semantica).
  DIM e = IO.GETTEMPFILE()
  IF (b <> d) AND (e <> b) THEN PRINT "uniq=si" ELSE PRINT "uniq=no"

  ' parametri: ZERO argomenti in tutte le forme (l'arità e' 0)
  IF LEN(IO.GETTEMPDIR()) = LEN(IO.TEMPDIR()) THEN PRINT "0-arg=si" ELSE PRINT "0-arg=no"
END SUB
