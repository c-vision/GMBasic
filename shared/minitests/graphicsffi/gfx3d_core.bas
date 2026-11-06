' EXPECT: persp= 0 : 0.666666666666667 , 1 ,-1 , 0 |look= 0 : 1 , 1 ,-5 |aabb= 0 :-1 ,-1 , 0 , 1 , 2 , 2 |center= 0 : 0 , 0.5 , 1 |raysph= 1 : 4 |raypl= 1 : 5 |raytri= 1 : 5 |reflect= 0 : 0 , 0 , 1 |phong= 0 : 0.5 , 0.5 , 0.5 |decomp= 1 : 1 , 1 , 1 : 1 : 0 |mesh= 1 : 8 : 12 : 0 :-0.5 ,-0.5 ,-0.5 : 0 : 0 , 1 , 2 |mfree= 0 |raster= 1 : 0 |rtri= 0 |rget= 0 : 0.285714285714286 , 0.214285714285714 , 0.5 |rfree= 0 |surf= 1 : 4 : 2 |sfree= 0 |dda= 1 : 2 , 0 , 0 , 1.5 |cyl= 1 : 4 |cone= 1 : 4.2 |cyln= 0 : 1 , 0 , 0 |conen= 0 : 0.992876838486922 , 0.119145220618431 , 0 |abtr= 0 :-1 , 1 |orbit= 0 : 0 , 2.87655323162522 , 5.26549537134224 |camtr= 0 : 1 |ndc= 0 : 0 , 0 ,-1 |rayat= 0 : 0 , 0 , 1 |aabbmk= 0 :-1 ,-1 
' FFI gmbgraphics: GFX3D — geometria, raggi/intersezioni, mesh, raster software.
' Solo calcolo: nessun display, parità 4 backend.
IMPORT GFX3D

SUB Main()
  DIM m(15) AS DOUBLE, f(23) AS DOUBLE, o(15) AS DOUBLE, o3(2) AS DOUBLE

  PRINT "persp="; GFX3D.PERSPECTIVE(1.5707963267948966, 1.5, 0.1, 100, m); ":";
  PRINT m(0); ","; m(5); ","; m(11); ","; m(15)

  DIM eye(2) AS DOUBLE, ctr(2) AS DOUBLE, upv(2) AS DOUBLE
  eye(0) = 0 : eye(1) = 0 : eye(2) = 5
  ctr(0) = 0 : ctr(1) = 0 : ctr(2) = 0
  upv(0) = 0 : upv(1) = 1 : upv(2) = 0
  PRINT "look="; GFX3D.LOOKAT(eye(), ctr(), upv(), m); ":";
  PRINT m(0); ","; m(5); ","; m(14)

  DIM pts(8) AS DOUBLE, a(5) AS DOUBLE
  pts(0) = -1 : pts(1) = -1 : pts(2) = 0
  pts(3) = 1  : pts(4) = 1  : pts(5) = 2
  pts(6) = 0  : pts(7) = 2  : pts(8) = 1
  PRINT "aabb="; GFX3D.AABBFROMPOINTS(pts(), 9, a); ":";
  PRINT a(0); ","; a(1); ","; a(2); ","; a(3); ","; a(4); ","; a(5)
  PRINT "center="; GFX3D.AABBCENTER(a(), o3); ":"; o3(0); ","; o3(1); ","; o3(2)

  DIM r(5) AS DOUBLE, sph(3) AS DOUBLE, t(2) AS DOUBLE, tuv(3) AS DOUBLE
  r(0) = 0 : r(1) = 0 : r(2) = 5 : r(3) = 0 : r(4) = 0 : r(5) = -1
  sph(0) = 0 : sph(1) = 0 : sph(2) = 0 : sph(3) = 1
  PRINT "raysph="; GFX3D.RAYSPHERE(r(), sph(), t); ":"; t(0)

  DIM pl(5) AS DOUBLE
  pl(0) = 0 : pl(1) = 0 : pl(2) = 0 : pl(3) = 0 : pl(4) = 0 : pl(5) = 1
  PRINT "raypl="; GFX3D.RAYPLANE(r(), pl(), t); ":"; t(0)

  DIM tri(8) AS DOUBLE
  tri(0) = -1 : tri(1) = -1 : tri(2) = 0
  tri(3) = 1  : tri(4) = -1 : tri(5) = 0
  tri(6) = 0  : tri(7) = 1  : tri(8) = 0
  PRINT "raytri="; GFX3D.RAYTRIANGLE(r(), tri(), tuv); ":"; tuv(0)

  DIM dir(2) AS DOUBLE, nrm(2) AS DOUBLE
  dir(0) = 0 : dir(1) = 0 : dir(2) = -1
  nrm(0) = 0 : nrm(1) = 0 : nrm(2) = 1
  PRINT "reflect="; GFX3D.RAYREFLECT(dir(), nrm(), o3); ":"; o3(0); ","; o3(1); ","; o3(2)

  DIM par(9) AS DOUBLE, oc3(2) AS DOUBLE, r0 AS DOUBLE, r1 AS DOUBLE, r2 AS DOUBLE
  DIM light(2) AS DOUBLE, vv(2) AS DOUBLE, nn(2) AS DOUBLE
  light(0) = 0 : light(1) = 0 : light(2) = 5
  vv(0) = 0 : vv(1) = 0 : vv(2) = 1
  nn(0) = 0 : nn(1) = 0 : nn(2) = 1
  par(0) = 1 : par(1) = 1 : par(2) = 1
  par(3) = 0.5 : par(4) = 0.5 : par(5) = 0.5
  par(6) = 0.1 : par(7) = 0.9 : par(8) = 0 : par(9) = 1
  PRINT "phong="; GFX3D.SHADEPHONG(sph(), nn(), vv(), light(), par(), oc3); ":";
  r0 = oc3(0) : r1 = oc3(1) : r2 = oc3(2)
  PRINT r0; ","; r1; ","; r2

  DIM idm(15) AS DOUBLE, sc(2) AS DOUBLE, rq(3) AS DOUBLE, ps(2) AS DOUBLE
  idm(0) = 1 : idm(5) = 1 : idm(10) = 1 : idm(15) = 1
  PRINT "decomp="; GFX3D.DECOMPOSESRT(idm(), sc(), rq(), ps()); ":";
  PRINT sc(0); ","; sc(1); ","; sc(2); ":"; rq(3); ":"; ps(0)

  DIM mh AS LONG, nv AS LONG, nt AS LONG
  mh = GFX3D.MESHBOX(1, 1, 1)
  nv = GFX3D.MESHNV(mh) : nt = GFX3D.MESHNT(mh)
  DIM mp(71) AS DOUBLE, mt(35) AS DOUBLE
  PRINT "mesh="; mh; ":"; nv; ":"; nt; ":";
  PRINT GFX3D.MESHPOS(mh, mp); ":"; mp(0); ","; mp(1); ","; mp(2); ":";
  PRINT GFX3D.MESHTRI(mh, mt); ":"; mt(0); ","; mt(1); ","; mt(2)
  PRINT "mfree="; GFX3D.MESHFREE(mh)

  DIM rh AS LONG
  rh = GFX3D.RASTERNEW(8, 4)
  PRINT "raster="; rh; ":"; GFX3D.RASTERCLEAR(rh, 1.0)
  DIM pr(8) AS DOUBLE, cr(8) AS DOUBLE, col3(2) AS DOUBLE
  pr(0) = 0 : pr(1) = 0 : pr(2) = 0.5
  pr(3) = 7 : pr(4) = 0 : pr(5) = 0.5
  pr(6) = 0 : pr(7) = 3 : pr(8) = 0.5
  cr(0) = 1 : cr(1) = 0 : cr(2) = 0
  cr(3) = 0 : cr(4) = 1 : cr(5) = 0
  cr(6) = 0 : cr(7) = 0 : cr(8) = 1
  PRINT "rtri="; GFX3D.RASTERTRI(rh, pr(), cr())
  PRINT "rget="; GFX3D.RASTERGET(rh, 1, 1, col3); ":";
  r0 = col3(0) : r1 = col3(1) : r2 = col3(2)
  PRINT r0; ","; r1; ","; r2
  PRINT "rfree="; GFX3D.RASTERFREE(rh)
  DIM zs(3) AS DOUBLE, hit(5) AS DOUBLE
  zs(0) = 0 : zs(1) = 1 : zs(2) = 1 : zs(3) = 2
  DIM sh AS LONG
  sh = GFX3D.MESHSURFACE(zs(), 2, 2, 1, 1, 1)
  PRINT "surf="; sh; ":"; GFX3D.MESHNV(sh); ":"; GFX3D.MESHNT(sh)
  PRINT "sfree="; GFX3D.MESHFREE(sh)

  DIM gmap(15) AS DOUBLE
  gmap(2) = 1 : gmap(6) = 1 : gmap(10) = 1 : gmap(14) = 1
  PRINT "dda="; GFX3D.RAYCASTDDA(gmap(), 4, 4, 0.5, 0.5, 1, 0, hit); ":";
  PRINT hit(1); ","; hit(2); ","; hit(3); ","; hit(4)
  DIM cbase(2) AS DOUBLE, axis(2) AS DOUBLE, ryv(5) AS DOUBLE, t2(1) AS DOUBLE, nrm2(2) AS DOUBLE
  axis(0) = 0 : axis(1) = 1 : axis(2) = 0
  ryv(0) = 5 : ryv(1) = 1 : ryv(2) = 0 : ryv(3) = -1 : ryv(4) = 0 : ryv(5) = 0
  PRINT "cyl="; GFX3D.RAYCYLINDER(ryv(), cbase(), axis(), 1, 5, t2()); ":"; t2(0)
  DIM apex(2) AS DOUBLE
  apex(0) = 0 : apex(1) = 5 : apex(2) = 0
  DIM caxis(2) AS DOUBLE
  caxis(0) = 0 : caxis(1) = -1 : caxis(2) = 0
  PRINT "cone="; GFX3D.RAYCONE(ryv(), apex(), caxis(), 1, 5, t2()); ":"; t2(0)
  DIM cp(2) AS DOUBLE
  cp(0) = 1 : cp(1) = 2 : cp(2) = 0
  PRINT "cyln="; GFX3D.CYLINDERNORMAL(cbase(), axis(), cp(), nrm2()); ":"; nrm2(0); ","; nrm2(1); ","; nrm2(2)
  PRINT "conen="; GFX3D.CONENORMAL(apex(), caxis(), 0.2, cp(), nrm2()); ":"; nrm2(0); ","; nrm2(1); ","; nrm2(2)
  DIM ibox(5) AS DOUBLE, mtx(15) AS DOUBLE, obox(5) AS DOUBLE
  ibox(0) = -1 : ibox(1) = -1 : ibox(2) = -1 : ibox(3) = 1 : ibox(4) = 1 : ibox(5) = 1
  mtx(0) = 1 : mtx(5) = 1 : mtx(10) = 1 : mtx(15) = 1
  PRINT "abtr="; GFX3D.AABBTRANSFORM(ibox(), mtx(), obox()); ":";
  PRINT obox(0); ","; obox(3)
  DIM op(2) AS DOUBLE, tp(2) AS DOUBLE, dl(2) AS DOUBLE, amn(2) AS DOUBLE, amx(2) AS DOUBLE
  PRINT "orbit="; GFX3D.CAMERAORBIT(eye(), ctr(), 0, 0.5, 6, op()); ":";
  PRINT op(0); ","; op(1); ","; op(2)
  dl(0) = 1 : dl(1) = 0 : dl(2) = 0
  PRINT "camtr="; GFX3D.CAMERATRANSLATE(eye(), ctr(), dl(), op()); ":"; op(0)
  PRINT "ndc="; GFX3D.CAMERANDCRAY(eye(), ctr(), upv(), 1.5707963, 1.5, 0, 0, op()); ":";
  PRINT op(0); ","; op(1); ","; op(2)
  PRINT "rayat="; GFX3D.RAYAT(r(), 4, tp()); ":"; tp(0); ","; tp(1); ","; tp(2)
  amn(0) = -1 : amn(1) = -1 : amn(2) = -1
  amx(0) = 1 : amx(1) = 1 : amx(2) = 1
  PRINT "aabbmk="; GFX3D.AABBMAKE(amn(), amx(), tp()); ":"; tp(0); ","; tp(2)
END SUB
