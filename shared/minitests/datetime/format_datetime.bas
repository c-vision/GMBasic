' MiniTest DATETIME — formato canonico di data/ora ("dd/MM/yyyy HH:mm:ss").
'
' È il formato che PRINT usa per i valori DATA/ORA (prima la gamba C99 stampava il NUMERO
' SERIALE OADate: "46327.7783564815" invece di "01/11/2026 18:40:53", divergenza trovata sul
' sample Http/HttpRoutes.bas che logga NOW()). Il test usa date FISSE costruite con
' DATESERIAL + TIMESERIAL (DATEVALUE restituisce solo la data, senza ora) per avere un
' oracolo deterministico su tutte e 4 le gambe.
'
' EXPECT: d=02/01/2026 03:04:05|date=02/01/2026|iso=2026-01-02T03:04:05|len= 19|month-end=31/01/2026|dtoc=01-02-2026
IMPORT DATETIME

SUB Main()
  DIM d AS DATETIME
  d = DATETIME.DATESERIAL(2026, 1, 2) + DATETIME.TIMESERIAL(3, 4, 5)

  ' (1) formato completo usato dal PRINT dei valori DATA/ORA
  PRINT "d="; FORMAT(d, "dd/MM/yyyy HH:mm:ss")

  ' (2) sola data
  PRINT "date="; FORMAT(d, "dd/MM/yyyy")

  ' (3) forma ISO (canone JSON)
  PRINT "iso="; FORMAT(d, "yyyy-MM-ddTHH:mm:ss")

  ' (4) lunghezza del formato canonico (19 caratteri, niente suffissi/spazi)
  DIM s AS STRING
  s = FORMAT(d, "dd/MM/yyyy HH:mm:ss")
  PRINT "len="; LEN(s)

  ' (5) fine mese (EOMONTH senza secondo argomento: il mese opzionale deve valere 0)
  PRINT "month-end="; FORMAT(DATETIME.EOMONTH(DATETIME.DATEVALUE("2026-01-15")), "dd/MM/yyyy")

  ' (6) forma compatta yyyyMMdd
  PRINT "dtoc="; DATETIME.DTOC(DATETIME.DATEVALUE("2026-01-02"))
END SUB
