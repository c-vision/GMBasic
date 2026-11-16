' Forma MISTA invalida `IF c THEN <stmt> ELSE` + blocco + `END IF`: lascia un END IF senza IF aperto. Prima il front-end andava in
' HANG (nessun errore, nessun output) su ogni backend; ora deve dare un errore di parsing chiaro.
' EXPECT: COMPILE ERROR: END IF without a matching IF. (a single-line 'IF c THEN stmt [ELSE stmt]' has no END IF; if you mixed it with a multi-line block, use the block form 'IF c THEN' / 'ELSE' / 'END IF')
SUB Main()
  lf = 0
  r = 1
  IF lf = 0 THEN r = 1 ELSE
    r = 0
  END IF
  PRINT "r=" + STR(r)
END SUB
