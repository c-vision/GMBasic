' END SELECT senza SELECT CASE aperto: errore di parsing (prima HANG del front-end).
' EXPECT: ERROR: END SELECT without a matching SELECT CASE.
SUB Main()
  r = 1
  END SELECT
  PRINT r
END SUB
