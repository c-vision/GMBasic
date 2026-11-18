' EXPECT: ortho= 0 : 0.5 , 1 , 1|merge= 0 : 0 , 3 , 3|ext= 0 : 1 , 1|inbox= 1|outbox= 0|aabbx= 1 , 0|camvp= 0 : 1.0000000267949 , 1.0000000267949 , 5|camv= 0 : 1 , 1|camp= 0 : 1.0000000267949 , 1.0000000267949|frustum= 0 : 0.707106790659974 ,-0.707106771713121 , 3.5355338585656|fpoint= 1 , 0|faabb= 1|rayaabb= 1 : 4 , 6|raybox= 1 : 4|refract= 1 : 0 , 0 ,-1|sph2= 1 , 0|sphbox= 1|sphpl= 1 : 0.5|segpl= 1 : 0.5|segsph= 1 : 0.25|segtri= 1 : 0.5|tritri= 1 , 0|vol= 0 : 0.0855482148687488 , 0.632120558828558 , 0.23254415793483 , 0.950212931632136|msph= 1 : 63 , 96 ;mpln= 1 : 9 , 8 ;mcyl= 1 : 20 , 32 ;clipn= 4 : 0.733333333333333 , 0.1 , 1 , 0.5
' FFI gmbgraphics: GFX3D — AABB/frustum/camera, intersezioni, volume rendering,
' mesh aggiuntive e clipping near. Solo calcolo, parità 4 backend.
IMPORT GFX3D

SUB Main()
  DIM m(15) AS DOUBLE, vp(15) AS DOUBLE
  PRINT "ortho="; GFX3D.ORTHO(-2, 2, -1, 1, 0.1, 100, m); ":";
  PRINT m(0); ","; m(5); ","; m(15)

  DIM ba(5) AS DOUBLE, bb(5) AS DOUBLE, bo(5) AS DOUBLE
  ba(0) = 0 : ba(1) = 0 : ba(2) = 0 : ba(3) = 1 : ba(4) = 1 : ba(5) = 1
  bb(0) = 2 : bb(1) = 2 : bb(2) = 2 : bb(3) = 3 : bb(4) = 3 : bb(5) = 3
  PRINT "merge="; GFX3D.AABBMERGE(ba(), bb(), bo()); ":";
  PRINT bo(0); ","; bo(3); ","; bo(5)
  PRINT "ext="; GFX3D.AABBEXTENTS(ba(), bo()); ":"; bo(0); ","; bo(2)

  DIM p(2) AS DOUBLE
  p(0) = 0.5 : p(1) = 0.5 : p(2) = 0.5
  PRINT "inbox="; GFX3D.AABBTESTPOINT(ba(), p())
  p(0) = 2
  PRINT "outbox="; GFX3D.AABBTESTPOINT(ba(), p())
  DIM bc(5) AS DOUBLE
  bc(0) = 0.5 : bc(1) = 0.5 : bc(2) = 0.5 : bc(3) = 1.5 : bc(4) = 1.5 : bc(5) = 1.5
  PRINT "aabbx="; GFX3D.AABBINTERSECT(ba(), bc()); ","; GFX3D.AABBINTERSECT(ba(), bb())

  DIM eye(2) AS DOUBLE, ctr(2) AS DOUBLE, upv(2) AS DOUBLE
  eye(0) = 0 : eye(1) = 0 : eye(2) = 5
  upv(0) = 0 : upv(1) = 1 : upv(2) = 0
  PRINT "camvp="; GFX3D.CAMERAVIEWPROJECTION(eye(), ctr(), upv(), 1.5707963, 1, 0.1, 100, vp); ":";
  PRINT vp(0); ","; vp(5); ","; vp(15)
  PRINT "camv="; GFX3D.CAMERAVIEW(eye(), ctr(), upv(), m); ":"; m(0); ","; m(15)
  PRINT "camp="; GFX3D.CAMERAPROJECTION(1.5707963, 1, 0.1, 100, m); ":"; m(0); ","; m(5)

  DIM planes(23) AS DOUBLE, origin(2) AS DOUBLE, farp(2) AS DOUBLE, bx(5) AS DOUBLE
  farp(2) = -100
  bx(0) = -1 : bx(1) = -1 : bx(2) = -1 : bx(3) = 1 : bx(4) = 1 : bx(5) = 1
  PRINT "frustum="; GFX3D.FRUSTUMFROM(vp(), planes()); ":";
  PRINT planes(0); ","; planes(2); ","; planes(3)
  PRINT "fpoint="; GFX3D.FRUSTUMTESTPOINT(planes(), origin()); ",";
  PRINT GFX3D.FRUSTUMTESTPOINT(planes(), farp())
  PRINT "faabb="; GFX3D.FRUSTUMTESTAABB(planes(), bx())

  DIM r(5) AS DOUBLE, abx(5) AS DOUBLE, t(1) AS DOUBLE
  r(2) = 5 : r(5) = -1
  abx(0) = -1 : abx(1) = -1 : abx(2) = -1 : abx(3) = 1 : abx(4) = 1 : abx(5) = 1
  PRINT "rayaabb="; GFX3D.RAYAABB(r(), abx(), t()); ":"; t(0); ","; t(1)
  PRINT "raybox="; GFX3D.RAYBOX(r(), abx(), t()); ":"; t(0)

  DIM vv(2) AS DOUBLE, nn(2) AS DOUBLE, ro(2) AS DOUBLE
  vv(2) = -1 : nn(2) = 1
  PRINT "refract="; GFX3D.RAYREFRACT(vv(), nn(), 0.75, ro()); ":";
  PRINT ro(0); ","; ro(1); ","; ro(2)

  DIM c1(2) AS DOUBLE, c2(2) AS DOUBLE, pl(5) AS DOUBLE, dist(0) AS DOUBLE
  c2(0) = 1.5
  PRINT "sph2="; GFX3D.SPHERESPHERE(c1(), 1, c2(), 1); ",";
  c2(0) = 5
  PRINT GFX3D.SPHERESPHERE(c1(), 1, c2(), 1)
  DIM sbx(5) AS DOUBLE
  sbx(0) = 0.5 : sbx(1) = -0.5 : sbx(2) = -0.5 : sbx(3) = 1.5 : sbx(4) = 0.5 : sbx(5) = 0.5
  PRINT "sphbox="; GFX3D.SPHEREBOX(c1(), 1, sbx())
  DIM sc(2) AS DOUBLE, sp(5) AS DOUBLE
  sc(2) = 0.5
  sp(2) = 0 : sp(5) = 1
  PRINT "sphpl="; GFX3D.SPHEREPLANE(sc(), 1, sp(), dist()); ":"; dist(0)

  DIM sa(2) AS DOUBLE, sb(2) AS DOUBLE, st(0) AS DOUBLE, tri(8) AS DOUBLE
  sa(2) = -1 : sb(2) = 1
  PRINT "segpl="; GFX3D.SEGMENTPLANE(sa(), sb(), sp(), st()); ":"; st(0)
  sa(2) = -2 : sb(2) = 2
  PRINT "segsph="; GFX3D.SEGMENTSPHERE(sa(), sb(), c1(), 1, st()); ":"; st(0)
  tri(0) = -1 : tri(1) = -1 : tri(3) = 1 : tri(4) = -1 : tri(6) = 0 : tri(7) = 1
  PRINT "segtri="; GFX3D.SEGMENTTRIANGLE(sa(), sb(), tri(), st()); ":"; st(0)

  DIM t2(8) AS DOUBLE
  t2(1) = -2 : t2(2) = -2
  t2(4) = 2 : t2(5) = -2
  t2(7) = 0 : t2(8) = 2
  PRINT "tritri="; GFX3D.TRIANGLETRIANGLE(tri(), t2()); ",";
  t2(2) = 3 : t2(5) = 3 : t2(8) = 7
  PRINT GFX3D.TRIANGLETRIANGLE(tri(), t2())

  ' vout(): VOLUMERENDER scrive n*3 valori (n=3 → 9), non 3: con vout(2) scriveva 6
  ' double oltre la fine dell'array (overflow silenzioso, ora errore esplicito).
  DIM sg(2) AS DOUBLE, vr(8) AS DOUBLE, dt(2) AS DOUBLE, vout(8) AS DOUBLE, acc(0) AS DOUBLE
  sg(0) = 1 : sg(1) = 1 : sg(2) = 1
  vr(1) = 1 : vr(5) = 1 : vr(6) = 1
  dt(0) = 1 : dt(1) = 1 : dt(2) = 1
  PRINT "vol="; GFX3D.VOLUMERENDER(3, sg(), vr(), dt(), vout(), acc()); ":";
  PRINT vout(0); ","; vout(1); ","; vout(2); ","; acc(0)

  DIM hs AS LONG, hp AS LONG, hc AS LONG, huh AS LONG
  hs = GFX3D.MESHSPHERE(8, 6, 1)
  PRINT "msph="; hs; ":"; GFX3D.MESHNV(hs); ","; GFX3D.MESHNT(hs); ";";
  huh = GFX3D.MESHFREE(hs)
  hp = GFX3D.MESHPLANE(2, 2, 1, 1)
  PRINT "mpln="; hp; ":"; GFX3D.MESHNV(hp); ","; GFX3D.MESHNT(hp); ";";
  huh = GFX3D.MESHFREE(hp)
  hc = GFX3D.MESHCYLINDER(8, 1, 2)
  PRINT "mcyl="; hc; ":"; GFX3D.MESHNV(hc); ","; GFX3D.MESHNT(hc); ";";
  huh = GFX3D.MESHFREE(hc)

  DIM ct(8) AS DOUBLE, co(11) AS DOUBLE
  ct(0) = 0 : ct(1) = 0 : ct(2) = -1
  ct(3) = 1 : ct(4) = 0 : ct(5) = 0.5
  ct(6) = 0 : ct(7) = 1 : ct(8) = 0.5
  PRINT "clipn="; GFX3D.RASTERCLIPNEAR(ct(), 0.1, co()); ":";
  PRINT co(0); ","; co(2); ","; co(3); ","; co(5)
END SUB
