' scipy.spatial.KDTree: costruzione (punti FLAT + m), query k-NN (indici e distanze),
' query_ball_point. Oracoli: SciPy 1.18 (query([4,4],k=3) → d=[1,√2,√5], i=[1,7,0]).
IMPORT KDTREE

SUB Main()
  DIM flat(19) AS DOUBLE = {2,3, 5,4, 9,6, 4,7, 8,1, 7,2, 1,8, 3,3, 6,5, 0,6}
  h = KDTREE.CREATE(flat, 2)

  DIM x(1) AS DOUBLE = {4, 4}
  DIM q() = KDTREE.QUERY(h, x, 3)
  PRINT "query   ="; q(0); q(1); q(2)
  DIM qd() = KDTREE.QUERYDIST(h, x, 3)
  PRINT "qdist   ="; qd(0); qd(1); qd(2)
  DIM b() = KDTREE.QUERYBALL(h, x, 2)
  PRINT "ball    ="; b(0); b(1)
END SUB
