REM FILE: Gfx3dLab.bas — GFX3D: camera, mesh, raggi, raster software, raycast su griglia.
REM Scenario: si costruisce una scena minima (camera prospettica, sfera mesh, raggio che la
REM colpisce), si rasterizza un triangolo e si lancia un raggio DDA su una griglia. Deterministico.
IMPORT GFX3D

SUB Main()
  DIM m(15) AS DOUBLE, planes(23) AS DOUBLE
  PRINT "persp="; GFX3D.PERSPECTIVE(1.5707963, 1.5, 0.1, 100, m);
  PRINT " frust="; GFX3D.FRUSTUMFROM(m(), planes())

  DIM eye(2) AS DOUBLE, ctr(2) AS DOUBLE, upv(2) AS DOUBLE
  eye(0) = 0 : eye(1) = 0 : eye(2) = 8
  upv(0) = 0 : upv(1) = 1 : upv(2) = 0
  PRINT "vp="; GFX3D.CAMERAVIEWPROJECTION(eye(), ctr(), upv(), 1.5707963, 1.5, 0.1, 100, m)

  DIM sph AS LONG
  sph = GFX3D.MESHSPHERE(8, 6, 1)
  PRINT "sph="; sph; ":"; GFX3D.MESHNV(sph); ":"; GFX3D.MESHNT(sph)

  DIM r(5) AS DOUBLE, sc(3) AS DOUBLE, t(1) AS DOUBLE
  r(0) = 0 : r(1) = 0 : r(2) = 8 : r(3) = 0 : r(4) = 0 : r(5) = -1
  sc(0) = 0 : sc(1) = 0 : sc(2) = 0 : sc(3) = 1
  PRINT "hit="; GFX3D.RAYSPHERE(r(), sc(), t()); ":"; t(0)
  PRINT "sfree="; GFX3D.MESHFREE(sph)

  DIM rh AS LONG
  rh = GFX3D.RASTERNEW(16, 16)
  ok = GFX3D.RASTERCLEAR(rh, 1.0)
  DIM p(8) AS DOUBLE, cc(8) AS DOUBLE, rgbv(2) AS DOUBLE
  p(0) = 1 : p(1) = 1 : p(2) = 0.5
  p(3) = 14 : p(4) = 1 : p(5) = 0.5
  p(6) = 1 : p(7) = 14 : p(8) = 0.5
  cc(0) = 1 : cc(1) = 0 : cc(2) = 0
  cc(3) = 0 : cc(4) = 1 : cc(5) = 0
  cc(6) = 0 : cc(7) = 0 : cc(8) = 1
  PRINT "rtri="; GFX3D.RASTERTRI(rh, p(), cc())
  PRINT "rpix="; GFX3D.RASTERGET(rh, 2, 2, rgbv()); ":";
  DIM r0 AS DOUBLE, r1 AS DOUBLE, r2 AS DOUBLE
  r0 = rgbv(0) : r1 = rgbv(1) : r2 = rgbv(2)
  PRINT r0; ","; r1; ","; r2
  PRINT "rfree="; GFX3D.RASTERFREE(rh)

  DIM gmap(15) AS DOUBLE, hit(5) AS DOUBLE
  gmap(2) = 1 : gmap(6) = 1 : gmap(10) = 1 : gmap(14) = 1
  PRINT "dda="; GFX3D.RAYCASTDDA(gmap(), 4, 4, 0.5, 0.5, 1, 0, hit()); ":";
  PRINT hit(1); ","; hit(2); ","; hit(4)
END SUB
