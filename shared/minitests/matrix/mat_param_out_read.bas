' EXPECT: COMPILE ERROR: Il parametro matrice 'm' è dichiarato OUT (solo scrittura): non può essere letto. Dichiaralo INOUT se devi leggere e scrivere.
' ADR-MATRIX §3 (F3-ter), test NEGATIVO: un parametro OUT è solo scrittura; la lettura
' di un elemento è rifiutata in compilazione sui quattro backend.
SUB Bad(OUT m AS MATRIX OF (INT, INT, DOUBLE))
  DIM v AS DOUBLE
  v = m(0, 0)
END SUB

SUB Main()
  DIM m = MATRIX.CREATE(2, 2)
  CALL Bad(m)
END SUB
