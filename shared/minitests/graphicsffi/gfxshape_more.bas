' EXPECT: dist= 5|aabb= 0 : 0 , 0 , 4 , 2|center= 0 : 2 , 1|ext= 0 : 2 , 1|valid= 1|merge= 0 : 0 , 6 , 3|aa= 1  ac= 0|clray= 0 : 2 , 1|clbox= 0 : 2 , 0|clcirc= 0 : 3 , 4|claa= 0 : 2 , 1|cc= 1|r2c= 1 : 4|r2a= 1 : 0 , 4|r2s= 1 : 5|bary= 1 : 0.5 , 0.25 , 0.25|orect= 1|quad= 1|hex= 10.3923048454133 , 4 , 3.46410161513775 , 1.73205080756888|sarea= 16|chamfer= 8|haus= 2|a2mk= 0 : 1 , 2 , 5 , 7|r2at= 0 : 5 , 1
' FFI gmbgraphics: GFXSHAPE completo — AABB 2D, vicinanza, intersezioni 2D, poligoni,
' esagono, point-cloud. Solo calcolo, parità 4 backend.
IMPORT GFXSHAPE

SUB Main()
  DIM a(1) AS DOUBLE, b(1) AS DOUBLE, o(1) AS DOUBLE
  a(0) = 0 : a(1) = 0
  b(0) = 3 : b(1) = 4
  PRINT "dist="; GFXSHAPE.DIST(a(), b())

  DIM p(3) AS DOUBLE, box(3) AS DOUBLE
  p(0) = 0 : p(1) = 0 : p(2) = 4 : p(3) = 2
  PRINT "aabb="; GFXSHAPE.AABBFROMPOINTS2D(p(), box()); ":";
  PRINT box(0); ","; box(1); ","; box(2); ","; box(3)
  PRINT "center="; GFXSHAPE.AABBCENTER2D(box(), o()); ":"; o(0); ","; o(1)
  PRINT "ext="; GFXSHAPE.AABBEXTENTS2D(box(), o()); ":"; o(0); ","; o(1)
  PRINT "valid="; GFXSHAPE.AABBVALID(box())

  DIM q(3) AS DOUBLE, b2(3) AS DOUBLE
  q(0) = 2 : q(1) = 0 : q(2) = 6 : q(3) = 3
  PRINT "merge="; GFXSHAPE.AABBMERGE2D(box(), q(), b2()); ":";
  PRINT b2(0); ","; b2(2); ","; b2(3)
  PRINT "aa="; GFXSHAPE.AABBAABB(box(), q());
  PRINT " ac="; GFXSHAPE.AABBCIRCLE(box(), 10, 1, 1)

  DIM ray(3) AS DOUBLE, t(1) AS DOUBLE
  ray(0) = 0 : ray(1) = 1 : ray(2) = 1 : ray(3) = 0
  PRINT "clray="; GFXSHAPE.CLOSESTRAY(ray(), q(), o()); ":"; o(0); ","; o(1)
  PRINT "clbox="; GFXSHAPE.CLOSESTAABB(box(), q(), o()); ":"; o(0); ","; o(1)
  PRINT "clcirc="; GFXSHAPE.CLOSESTCIRCLE(0, 0, 5, b(), o()); ":"; o(0); ","; o(1)
  PRINT "claa="; GFXSHAPE.CLOSESTAABBAABB(box(), q(), o()); ":"; o(0); ","; o(1)

  PRINT "cc="; GFXSHAPE.CIRCLECIRCLE(a(), 5, b(), 2)
  PRINT "r2c="; GFXSHAPE.RAY2CIRCLE(ray(), 5, 1, 1, t()); ":"; t(0)
  PRINT "r2a="; GFXSHAPE.RAY2AABB(ray(), box(), t()); ":"; t(0); ","; t(1)
  DIM s1(1) AS DOUBLE, s2(1) AS DOUBLE
  s1(0) = 5 : s1(1) = -5
  s2(0) = 5 : s2(1) = 5
  PRINT "r2s="; GFXSHAPE.RAY2SEGMENT(ray(), s1(), s2(), t()); ":"; t(0)

  DIM bt1(1) AS DOUBLE, bt2(1) AS DOUBLE, bp(1) AS DOUBLE, bary(2) AS DOUBLE
  bt1(0) = 4 : bt1(1) = 0
  bt2(0) = 0 : bt2(1) = 4
  bp(0) = 1 : bp(1) = 1
  PRINT "bary="; GFXSHAPE.BARYCENTRIC(a(), bt1(), bt2(), bp(), bary()); ":";
  PRINT bary(0); ","; bary(1); ","; bary(2)

  DIM ctr(1) AS DOUBLE, u(1) AS DOUBLE, v(1) AS DOUBLE
  ctr(0) = 5 : ctr(1) = 5
  u(0) = 1 : u(1) = 0
  v(0) = 0 : v(1) = 1
  PRINT "orect="; GFXSHAPE.POINTINORIENTEDRECT(ctr(), u(), v(), 2, 2, b())
  DIM qa(1) AS DOUBLE, qb(1) AS DOUBLE, qc(1) AS DOUBLE, qd(1) AS DOUBLE
  qa(0) = 0 : qa(1) = 0
  qb(0) = 4 : qb(1) = 0
  qc(0) = 4 : qc(1) = 4
  qd(0) = 0 : qd(1) = 4
  PRINT "quad="; GFXSHAPE.POINTINQUAD(qa(), qb(), qc(), qd(), q())

  PRINT "hex="; GFXSHAPE.HEXAREA(2); ","; GFXSHAPE.HEXLONG(2); ","; GFXSHAPE.HEXSHORT(2); ","; GFXSHAPE.HEXINNER(2)
  DIM sq(7) AS DOUBLE
  sq(0)=0:sq(1)=0:sq(2)=4:sq(3)=0:sq(4)=4:sq(5)=4:sq(6)=0:sq(7)=4
  PRINT "sarea="; GFXSHAPE.POLYGONSIGNEDAREA(sq()) 

  DIM c3a(5) AS DOUBLE, c3b(5) AS DOUBLE
  c3a(0)=0:c3a(1)=0:c3a(2)=0 : c3a(3)=1:c3a(4)=0:c3a(5)=0
  c3b(0)=0:c3b(1)=0:c3b(2)=2 : c3b(3)=1:c3b(4)=0:c3b(5)=2
  PRINT "chamfer="; GFXSHAPE.CHAMFER(c3a(), c3b())
  PRINT "haus="; GFXSHAPE.HAUSDORFF(c3a(), c3b())
  DIM amn(1) AS DOUBLE, amx(1) AS DOUBLE, o4(3) AS DOUBLE
  amn(0) = 1 : amn(1) = 2
  amx(0) = 5 : amx(1) = 7
  PRINT "a2mk="; GFXSHAPE.AABB2MAKE(amn(), amx(), o4()); ":";
  PRINT o4(0); ","; o4(1); ","; o4(2); ","; o4(3)
  PRINT "r2at="; GFXSHAPE.RAY2AT(ray(), 5, o()); ":"; o(0); ","; o(1)
END SUB
