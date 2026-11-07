' QUAT (FFI) — le 19 funzioni dei quaternioni su valori verificabili a mano.
' Convenzione (x,y,z,w); rotazioni con quaternioni normalizzati; matrici 3x3 row-major.
' Il quaternione è un TYPE passato per valore: q = QUAT.MAKE(...).
' EXPECT: id=0.0,0.0,0.0,1.0||make=0.5,-0.5,0.5,0.5||dirZ=0.000,0.000,0.000,1.000||dirX=0.000,0.707,0.000,0.707||axis=0.000,0.000,0.707,0.707||two=0.000,0.000,0.707,0.707||add=11.0,22.0,33.0,44.0||scale=2.0,4.0,6.0,8.0||neg=-1.0,-2.0,-3.0,-4.0||conj=-1.0,-2.0,-3.0,4.0||dot=300.0||len=5.477226||mul=0.0,0.0,1.0,0.0||norm=1.000000||inv= 0 :0.500000||inv0=-1 :0.0||norm0=1.0||lerp=0.707107,0.707107||slerp=0.382683,0.923880||mat= 0 :0.0,-1.0,1.0,0.0||fmat=0.707107,0.707107||rot= 0 :0.000000,1.000000,0.000000||f=0
IMPORT QUAT

SUB Main()
  DIM q AS Quat, a AS Quat, b AS Quat, r AS Quat

  ' ── costruzione ───────────────────────────────────────────────────────────
  q = QUAT.IDENTITY()
  PRINT "id="; FORMAT(q.X, "#.0"); ","; FORMAT(q.Y, "#.0"); ","; FORMAT(q.Z, "#.0"); ","; FORMAT(q.W, "#.0")
  q = QUAT.MAKE(0.5, -0.5, 0.5, 0.5)
  PRINT "|make="; FORMAT(q.X, "#.0"); ","; FORMAT(q.Y, "#.0"); ","; FORMAT(q.Z, "#.0"); ","; FORMAT(q.W, "#.0")
  q = QUAT.FROMDIR(0, 0, 1)
  PRINT "|dirZ="; FORMAT(q.X, "#.000"); ","; FORMAT(q.Y, "#.000"); ","; FORMAT(q.Z, "#.000"); ","; FORMAT(q.W, "#.000")
  q = QUAT.FROMDIR(1, 0, 0)
  PRINT "|dirX="; FORMAT(q.X, "#.000"); ","; FORMAT(q.Y, "#.000"); ","; FORMAT(q.Z, "#.000"); ","; FORMAT(q.W, "#.000")

  ' qZ90 = 90° attorno a Z
  a = QUAT.FROMAXISANGLE(0, 0, 1, 1.5707963267948966)
  PRINT "|axis="; FORMAT(a.X, "#.000"); ","; FORMAT(a.Y, "#.000"); ","; FORMAT(a.Z, "#.000"); ","; FORMAT(a.W, "#.000")
  b = QUAT.FROMTWOVECTORS(1, 0, 0, 0, 1, 0)
  PRINT "|two="; FORMAT(b.X, "#.000"); ","; FORMAT(b.Y, "#.000"); ","; FORMAT(b.Z, "#.000"); ","; FORMAT(b.W, "#.000")

  ' ── algebra ───────────────────────────────────────────────────────────────
  q = QUAT.MAKE(1, 2, 3, 4)
  b = QUAT.MAKE(10, 20, 30, 40)
  r = QUAT.ADD(q, b)
  PRINT "|add="; FORMAT(r.X, "#.0"); ","; FORMAT(r.Y, "#.0"); ","; FORMAT(r.Z, "#.0"); ","; FORMAT(r.W, "#.0")
  r = QUAT.SCALE(q, 2)
  PRINT "|scale="; FORMAT(r.X, "#.0"); ","; FORMAT(r.Y, "#.0"); ","; FORMAT(r.Z, "#.0"); ","; FORMAT(r.W, "#.0")
  r = QUAT.NEG(q)
  PRINT "|neg="; FORMAT(r.X, "#.0"); ","; FORMAT(r.Y, "#.0"); ","; FORMAT(r.Z, "#.0"); ","; FORMAT(r.W, "#.0")
  r = QUAT.CONJUGATE(q)
  PRINT "|conj="; FORMAT(r.X, "#.0"); ","; FORMAT(r.Y, "#.0"); ","; FORMAT(r.Z, "#.0"); ","; FORMAT(r.W, "#.0")
  PRINT "|dot="; FORMAT(QUAT.DOT(q, b), "#.0")
  PRINT "|len="; FORMAT(QUAT.LENGTH(q), "#.000000")

  ' i * j = k
  a = QUAT.MAKE(1, 0, 0, 0) : b = QUAT.MAKE(0, 1, 0, 0)
  r = QUAT.MUL(a, b)
  PRINT "|mul="; FORMAT(r.X, "#.0"); ","; FORMAT(r.Y, "#.0"); ","; FORMAT(r.Z, "#.0"); ","; FORMAT(r.W, "#.0")

  ' normalizza (0,0,0,2) -> (0,0,0,1); inversa di (0,0,0,2) -> (0,0,0,0.5)
  a = QUAT.MAKE(0, 0, 0, 2)
  r = QUAT.NORMALIZE(a)
  PRINT "|norm="; FORMAT(r.W, "#.000000")
  DIM st(0) AS LONG
  r = QUAT.INVERSE(a, st)
  PRINT "|inv="; st(0); ":"; FORMAT(r.W, "#.000000")
  ' quaternione nullo -> st(0) = -1, risultato zero; NORMALIZE torna l'identità
  a = QUAT.MAKE(0, 0, 0, 0)
  r = QUAT.INVERSE(a, st)
  PRINT "|inv0="; st(0); ":"; FORMAT(r.W, "#.0")
  r = QUAT.NORMALIZE(a)
  PRINT "|norm0="; FORMAT(r.W, "#.0")

  ' ── interpolazione ────────────────────────────────────────────────────────
  a = QUAT.IDENTITY()
  b = QUAT.MAKE(0, 1, 0, 0)
  r = QUAT.LERP(a, b, 0.5)
  PRINT "|lerp="; FORMAT(r.Y, "#.000000"); ","; FORMAT(r.W, "#.000000")
  b = QUAT.FROMAXISANGLE(0, 0, 1, 1.5707963267948966)
  r = QUAT.SLERP(a, b, 0.5)
  PRINT "|slerp="; FORMAT(r.Z, "#.000000"); ","; FORMAT(r.W, "#.000000")

  ' ── matrice e rotazione di un vettore ─────────────────────────────────────
  DIM m(8) AS DOUBLE
  q = QUAT.FROMAXISANGLE(0, 0, 1, 1.5707963267948966)
  PRINT "|mat="; QUAT.TOMAT3(q, m); ":"; FORMAT(m(0), "#.0"); ","; FORMAT(m(1), "#.0"); ","; FORMAT(m(3), "#.0"); ","; FORMAT(m(4), "#.0")
  r = QUAT.FROMMAT3(m)
  PRINT "|fmat="; FORMAT(r.Z, "#.000000"); ","; FORMAT(r.W, "#.000000")
  DIM v(2) AS DOUBLE
  PRINT "|rot="; QUAT.ROTATEVEC3(q, 1, 0, 0, v); ":"; FORMAT(v(0), "#.000000"); ","; FORMAT(v(1), "#.000000"); ","; FORMAT(v(2), "#.000000")
  PRINT "|f=0"
END SUB
