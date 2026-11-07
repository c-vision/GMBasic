REM FILE: ShapeLab.bas — GFXSHAPE: AABB 2D, closest point, intersezioni, poligoni, point cloud.
REM Scenario: un quadrato e un rettangolo; si calcolano AABB, distanze, intersezioni di un
REM raggio 2D, baricentro, area con segno, esagono e distanze fra point cloud 3D. Deterministico.
IMPORT GFXSHAPE

SUB Main()
  DIM p(7) AS DOUBLE, o(3) AS DOUBLE
  p(0) = 0  : p(1) = 0
  p(2) = 10 : p(3) = 0
  p(4) = 10 : p(5) = 10
  p(6) = 0  : p(7) = 10

  PRINT "aabb="; GFXSHAPE.AABBFROMPOINTS2D(p(), o()); ":";
  PRINT o(0); ","; o(1); ","; o(2); ","; o(3)
  PRINT "area="; GFXSHAPE.POLYGONAREA(p()) ; " sarea="; GFXSHAPE.POLYGONSIGNEDAREA(p()) 

  DIM c(1) AS DOUBLE, d(1) AS DOUBLE
  c(0) = 3 : c(1) = 4
  d(0) = 0 : d(1) = 0
  PRINT "dist="; GFXSHAPE.DIST(d(), c())
  PRINT "circ="; GFXSHAPE.POINTINCIRCLE(3, 4, 0, 0, 5); " tri=";
  PRINT GFXSHAPE.POINTINTRIANGLE(2, 2, 0, 0, 10, 0, 0, 10)

  DIM ray(3) AS DOUBLE, t(1) AS DOUBLE
  ray(0) = 0 : ray(1) = 5 : ray(2) = 1 : ray(3) = 0
  PRINT "ray2="; GFXSHAPE.RAY2SEGMENT(ray(), p(), c(), t()); ":"; t(0)
  PRINT "bary="; GFXSHAPE.BARYCENTRIC(d(), p(), c(), c(), o()); ":";
  PRINT o(0); ","; o(1); ","; o(2)

  DIM amn(1) AS DOUBLE, amx(1) AS DOUBLE
  amn(0) = 1 : amn(1) = 2
  amx(0) = 5 : amx(1) = 7
  PRINT "a2mk="; GFXSHAPE.AABB2MAKE(amn(), amx(), o()); ":"; o(0); ","; o(1); ","; o(2); ","; o(3)
  PRINT "r2at="; GFXSHAPE.RAY2AT(ray(), 5, o()); ":"; o(0); ","; o(1)

  PRINT "hex="; GFXSHAPE.HEXAREA(2); ","; GFXSHAPE.HEXINNER(2)

  DIM A(5) AS DOUBLE, B(5) AS DOUBLE
  A(0)=0:A(1)=0:A(2)=0 : A(3)=1:A(4)=0:A(5)=0
  B(0)=0:B(1)=0:B(2)=2 : B(3)=1:B(4)=0:B(5)=2
  PRINT "cham="; GFXSHAPE.CHAMFER(A(), B()); " haus="; GFXSHAPE.HAUSDORFF(A(), B())
END SUB
