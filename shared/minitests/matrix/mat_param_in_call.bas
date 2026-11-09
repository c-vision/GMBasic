' EXPECT: ERROR: Il parametro matrice 'm' è dichiarato IN (sola lettura) ma è passato al parametro 'x' INOUT di 'Writer', che può modificarlo. Dichiaralo INOUT (o OUT) oppure passa una copia (MATRIX.CLONEMATRIX).
' ADR-MATRIX §3 (F3-bis), test NEGATIVO: un parametro IN non può essere passato a un
' parametro OUT/INOUT di un'altra procedura (propagazione del contratto).
SUB Writer(INOUT x AS MATRIX OF (INT, INT, DOUBLE))
  x(0, 0) = 1
END SUB

SUB Outer(IN m AS MATRIX OF (INT, INT, DOUBLE))
  CALL Writer(m)
END SUB

SUB Main()
  DIM m = MATRIX.CREATE(2, 2)
  CALL Outer(m)
END SUB
