' EXPECT: ERROR: Il parametro matrice 'm' è dichiarato OUT (solo scrittura): non può essere letto. Dichiaralo INOUT se devi leggere e scrivere.
' ADR-MATRIX §3 (F3-quater), test NEGATIVO: l'INTERA matrice OUT non può essere passata
' a un parametro IN/INOUT di un'altra procedura (che la leggerebbe).
SUB Reader(IN x AS MATRIX OF (INT, INT, DOUBLE))
  DIM v AS DOUBLE
  v = x(0, 0)
END SUB

SUB Bad(OUT m AS MATRIX OF (INT, INT, DOUBLE))
  CALL Reader(m)
END SUB

SUB Main()
  DIM m = MATRIX.CREATE(2, 2)
  CALL Bad(m)
END SUB
