' ELSEIF senza IF aperto: errore di parsing (prima HANG del front-end).
' EXPECT: ERROR: ELSEIF without a matching IF.
SUB Main()
  r = 1
  ELSEIF 1 THEN
  PRINT r
END SUB
