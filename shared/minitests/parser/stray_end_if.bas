' END IF senza IF aperto in un corpo SUB: errore di parsing (prima HANG del front-end).
' EXPECT: ERROR: END IF without a matching IF. (a single-line 'IF c THEN stmt [ELSE stmt]' has no END IF; if you mixed it with a multi-line block, use the block form 'IF c THEN' / 'ELSE' / 'END IF')
SUB Main()
  r = 1
  END IF
  PRINT r
END SUB
