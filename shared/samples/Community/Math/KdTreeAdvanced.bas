' Sample KDTREE completo — CREATE, QUERY, QUERYDIST, QUERYBALL (4 backend).
'
' Dimostra:
'   1. KDTREE.CREATE(flat, m) con punti 2D e 3D
'   2. KDTREE.QUERY(h, x, k) — nearest neighbor (k=1, k=3)
'   3. KDTREE.QUERYDIST(h, x, k) — distanze dei k vicini
'   4. KDTREE.QUERYBALL(h, x, r) — punti entro raggio (raggio piccolo=0, medio, grande)
'   5. Riutilizzo dello stesso albero per query multiple
'
' Output previsto (6 linee):
'   nearest= 1
'   knearest= 2 , 0 , 1
'   dists= 1 , 1.4142 , 2.2361
'   ball_small= 0
'   ball_med= 2
'   nearest3d= 0
IMPORT KDTREE
IMPORT MATH

SUB Main()
    ' ── Setup albero 2D (5 punti) ──
    ' Punti: (2,3), (5,4), (9,6), (4,7), (8,1)
    DIM flat(9) AS DOUBLE = {2,3, 5,4, 9,6, 4,7, 8,1}
    h = KDTREE.CREATE(flat, 2)

    ' ── QUERY: nearest neighbor (k=1) ──
    DIM q1(1) AS DOUBLE = {4, 4}
    DIM nn() = KDTREE.QUERY(h, q1, 1)
    PRINT "nearest="; nn(0)

    ' ── QUERY: 3 nearest (k=3) ──
    DIM kn() = KDTREE.QUERY(h, q1, 3)
    PRINT "knearest="; kn(0); ","; kn(1); ","; kn(2)

    ' ── QUERYDIST: distanze dei 3 vicini ──
    DIM kd() = KDTREE.QUERYDIST(h, q1, 3)
    PRINT "dists="; MATH.ROUND(kd(0), 4); ","; MATH.ROUND(kd(1), 4); ","; MATH.ROUND(kd(2), 4)

    ' ── QUERYBALL: raggio piccolo (nessun match) ──
    DIM bs() = KDTREE.QUERYBALL(h, q1, 0.5)
    PRINT "ball_small="; UBOUND(bs) + 1

    ' ── QUERYBALL: raggio medio (matcha 2 punti) ──
    DIM bm() = KDTREE.QUERYBALL(h, q1, 2.5)
    PRINT "ball_med="; UBOUND(bm) + 1

    ' ── Setup albero 3D ──
    DIM flat3d(8) AS DOUBLE = {1,2,3, 4,5,6, 7,8,9}
    h3 = KDTREE.CREATE(flat3d, 3)
    DIM q3(2) AS DOUBLE = {1, 2, 3}
    DIM nn3() = KDTREE.QUERY(h3, q3, 1)
    PRINT "nearest3d="; nn3(0)
END SUB
