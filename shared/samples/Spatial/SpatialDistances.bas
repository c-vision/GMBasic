' SPATIAL — distanze a coppie, matrici di distanza e inviluppo convesso (scipy.spatial).
' I punti sono MATRICI rank-2: `DIM P(n, 1) AS DOUBLE`, P(i,j).
IMPORT SPATIAL

SUB Main()
  ' --- insieme di punti 2D: quadrato + punto interno ---
  DIM P(4, 1) AS DOUBLE
  P(0,0) = 0 : P(0,1) = 0
  P(1,0) = 4 : P(1,1) = 0
  P(2,0) = 4 : P(2,1) = 4
  P(3,0) = 0 : P(3,1) = 4
  P(4,0) = 2 : P(4,1) = 2

  ' PDIST: distanze a coppie condensate (triangolo superiore, i<j).
  DIM d() = SPATIAL.PDIST(P, "euclidean")
  PRINT "pdist(0,1)="; ROUND(d(0), 4); " pdist(0,2)="; ROUND(d(1), 4)

  ' CONVEXHULL: indici dei vertici dell'inviluppo convesso (il punto interno è escluso).
  DIM h() = SPATIAL.CONVEXHULL(P)
  PRINT "hull="; h(0); h(1); h(2); h(3)

  ' CDIST: matrice delle distanze P×Q (ogni riga di P contro ogni riga di Q).
  DIM Q(1, 1) AS DOUBLE
  Q(0,0) = 0 : Q(0,1) = 0
  Q(1,0) = 4 : Q(1,1) = 4
  m = SPATIAL.CDIST(P, Q, "euclidean")
  PRINT "cdist(0,0)="; ROUND(MATRIX.GETVALUE(m, 0, 0), 4); " cdist(2,1)="; ROUND(MATRIX.GETVALUE(m, 2, 1), 4)

  ' DISTANCEMATRIX: come CDIST ma con metrica euclidea.
  dm = SPATIAL.DISTANCEMATRIX(P, Q)
  PRINT "distmatrix(1,1)="; ROUND(MATRIX.GETVALUE(dm, 1, 1), 4)
END SUB
