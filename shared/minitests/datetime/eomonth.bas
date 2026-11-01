' EXPECT: 1999|12|31|2026|2|28|2026|1|31
SUB Main()
  ' EOMONTH(data) — un solo argomento: fine del mese corrente.
  d = DATETIME.EOMONTH(DATETIME.DATEVALUE("1999-12-15"))
  PRINT DATETIME.YEAR(d)
  PRINT DATETIME.MONTH(d)
  PRINT DATETIME.DAY(d)

  ' EOMONTH(data, mesi) — secondo argomento OPZIONALE: mese spostato.
  ' Su VM/C il builtin leggeva args[1] senza guardare argc: con UNA sola chiamata il
  ' "mese" era spazzatura e il risultato cadeva in un mese arbitrario.
  e = DATETIME.EOMONTH(DATETIME.DATEVALUE("2026-01-15"), 1)
  PRINT DATETIME.YEAR(e)
  PRINT DATETIME.MONTH(e)
  PRINT DATETIME.DAY(e)

  ' EOMONTH(data, 0) — esplicito, stesso risultato della forma a 1 argomento.
  f = DATETIME.EOMONTH(DATETIME.DATEVALUE("2026-01-15"), 0)
  PRINT DATETIME.YEAR(f)
  PRINT DATETIME.MONTH(f)
  PRINT DATETIME.DAY(f)
END SUB
