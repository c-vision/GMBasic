' END IF orfano DENTRO un ciclo (WHILE): prima il terminatore veniva inghiottito in silenzio (nessun errore, semantica errata);
' ora e' un errore di parsing come negli altri contesti.
' EXPECT: COMPILE ERROR: END IF without a matching IF. (a single-line 'IF c THEN stmt [ELSE stmt]' has no END IF; if you mixed it with a multi-line block, use the block form 'IF c THEN' / 'ELSE' / 'END IF')
SUB Main()
  i = 0
  WHILE i < 3
    IF i = 1 THEN r = 1 ELSE
      r = 0
    END IF
    i = i + 1
  WEND
  PRINT i
END SUB
