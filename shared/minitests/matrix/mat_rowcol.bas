' EXPECT: row= 5|col= 7|r= 1|rc= 3|contig=-1
' MATRIX F2b: ROW/COL restituiscono una matrice rank-2 (1×cols / rows×1),
' ISCONTIGUOUS è il predicato di contiguità (Bit, -1/0).
SUB Main()
  DIM m(1, 2) AS DOUBLE
  m(0, 0) = 1 : m(0, 1) = 5 : m(0, 2) = 9
  m(1, 0) = 7 : m(1, 1) = 6 : m(1, 2) = 3

  DIM r = MATRIX.ROW(m, 0)
  PRINT "row="; MATRIX.GETVALUE(r, 0, 1)
  DIM c = MATRIX.COL(m, 0)
  PRINT "col="; MATRIX.GETVALUE(c, 1, 0)
  PRINT "r="; MATRIX.ROWCOUNT(r)
  PRINT "rc="; MATRIX.COLUMNCOUNT(r)
  PRINT "contig="; MATRIX.ISCONTIGUOUS(m)
END SUB
