' EXPECT: ERROR: Il parametro matrice 'm' è dichiarato IN (sola lettura) e non può essere modificato. Dichiaralo INOUT (o OUT) per consentire la scrittura.
' ADR-MATRIX §3 (F3), test NEGATIVO: la scrittura su un parametro matrice IN (default)
' deve essere rifiutata in compilazione sui quattro backend.
SUB Bad(IN m AS MATRIX OF (INT, INT, DOUBLE))
  m(0, 0) = 1
END SUB

SUB Main()
  DIM m = MATRIX.CREATE(2, 2)
  CALL Bad(m)
END SUB
