REM FILE: GraphicsTour.bas — tour guidato del dominio grafica.
REM Scenario didattico: un unico programma attraversa tutte le superfici del dominio —
REM CANVAS (12 comandi), GFXCOLOR (11 funzioni), GFXSHAPE (35), GFX2D (44), GFX3D (60),
REM GFXCODEC (4) — mostrando per ognuna un risultato numerico verificabile. Deterministico,
REM headless: nessuna finestra (CANVAS USE/END confinano il disegno nei buffer offscreen).
IMPORT GRAPHICS
IMPORT GFXCOLOR
IMPORT GFXSHAPE
IMPORT GFX2D
IMPORT GFX3D
IMPORT GFXCODEC

WK$ = PROJECTFOLDER() + "/work"
IF IO.DirExists(WK$) = 0 THEN MKDIR WK$
PNG$ = WK$ + "/graphics_tour.png"

SUB Main()
  TourCanvas()
  TourColor()
  TourShape()
  Tour2D()
  Tour3D()
  TourCodec()
END SUB

' ═══ 1. CANVAS — i 12 comandi ═══════════════════════════════════════════
SUB TourCanvas()
  DIM cv AS LONG, cv2 AS LONG, cw AS LONG, ch AS LONG, col AS LONG, ok AS LONG
  ' NEW/USE/END creano e selezionano un buffer; SET/GET/CLEAR/BLIT ci lavorano;
  ' WIDTH/HEIGHT interrogano le dimensioni; SAVE/LOAD scambiano PNG; FREE lo rilascia.
  CANVAS NEW 8, 6 INTO cv
  CANVAS WIDTH cv INTO cw
  CANVAS HEIGHT cv INTO ch
  CANVAS CLEAR cv, 1
  CANVAS SET cv, 2, 2, 2
  CANVAS GET cv, 2, 2 INTO col
  PRINT "canvas="; cv; ","; cw; ","; ch; ","; col
  CANVAS SAVE cv, PNG$ INTO ok
  CANVAS LOAD PNG$ INTO cv2
  CANVAS USE cv2
  CANVAS BLIT cv, 0, 0
  CANVAS END
  CANVAS GET cv2, 2, 2 INTO col
  PRINT "canvas2="; cv2; ","; ok; ","; col
  CANVAS FREE cv
  CANVAS FREE cv2
END SUB

' ═══ 2. GFXCOLOR — conversioni e manipolazione ═════════════════════════
SUB TourColor()
  ' Componenti RGB in 0..255; H in [0,360); S/V/L in [0,1]; CMYK in [0,1].
  DIM c3(2) AS DOUBLE, c4(3) AS DOUBLE
  PRINT "color="; GFXCOLOR.RGB2HSV(255, 128, 0, c3()); ":"; c3(0); ","; c3(1); ","; c3(2)
  PRINT "color="; GFXCOLOR.HSV2RGB(30, 1, 1, c3()); ":"; c3(0); ","; c3(1); ","; c3(2)
  PRINT "color="; GFXCOLOR.RGB2HSL(0, 128, 255, c3()); ":"; c3(0); ","; c3(1); ","; c3(2)
  PRINT "color="; GFXCOLOR.HSL2RGB(210, 1, 0.5, c3()); ":"; c3(0); ","; c3(1); ","; c3(2)
  PRINT "color="; GFXCOLOR.RGB2CMYK(0, 128, 255, c4()); ":"; c4(0); ","; c4(1); ","; c4(2); ","; c4(3)
  PRINT "color="; GFXCOLOR.CMYK2RGB(1, 0.5, 0, 0, c3()); ":"; c3(0); ","; c3(1); ","; c3(2)
  PRINT "color="; GFXCOLOR.MIX(0, 0, 0, 255, 255, 255, 0.5, c3()); ":"; c3(0); ","; c3(1); ","; c3(2)
  PRINT "color="; GFXCOLOR.LIGHTEN(128, 128, 128, 0.2, c3()); ":"; c3(0); ","; c3(1); ","; c3(2)
  PRINT "color="; GFXCOLOR.DARKEN(128, 128, 128, 0.2, c3()); ":"; c3(0); ","; c3(1); ","; c3(2)
  PRINT "color="; GFXCOLOR.SATURATE(120, 80, 40, 0.5, c3()); ":"; c3(0); ","; c3(1); ","; c3(2)
  PRINT "color="; GFXCOLOR.COLORMAP(0.5, c3()); ":"; c3(0); ","; c3(1); ","; c3(2)
END SUB

' ═══ 3. GFXSHAPE — geometria 2D e hit-test ═════════════════════════════
SUB TourShape()
  ' Poligoni = array piatti x0,y0,…; n = numero di VALORI (pari). Predicati: 1 = dentro.
  DIM pg(7) AS DOUBLE, s2(1) AS DOUBLE, s4(3) AS DOUBLE
  pg(0) = 0  : pg(1) = 0  : pg(2) = 10 : pg(3) = 0
  pg(4) = 10 : pg(5) = 10 : pg(6) = 0  : pg(7) = 10
  PRINT "shape="; GFXSHAPE.POINTINPOLYGON(5, 5, pg()) ; ","; GFXSHAPE.POINTINPOLYGON(15, 5, pg()) 
  PRINT "shape="; GFXSHAPE.POINTINCIRCLE(3, 4, 0, 0, 5); ","; GFXSHAPE.POINTINCIRCLE(9, 9, 0, 0, 5)
  PRINT "shape="; GFXSHAPE.POINTINRECT(1, 1, 0, 0, 4, 4); ","; GFXSHAPE.POINTINRECT(9, 9, 0, 0, 4, 4)
  PRINT "shape="; GFXSHAPE.POINTINTRIANGLE(2, 2, 0, 0, 10, 0, 0, 10); ","; GFXSHAPE.POINTINTRIANGLE(9, 9, 0, 0, 10, 0, 0, 10)
  PRINT "shape="; GFXSHAPE.POLYGONAREA(pg()) ; ","; GFXSHAPE.POLYGONSIGNEDAREA(pg()) 
  PRINT "shape="; GFXSHAPE.POLYGONCENTROID(pg(), s2()); ":"; s2(0); ","; s2(1)
  PRINT "shape="; GFXSHAPE.CLOSESTPOINTSEG(5, 5, 0, 0, 10, 0, s2()); ":"; s2(0); ","; s2(1)

  DIM bb(3) AS DOUBLE, bb2(3) AS DOUBLE, mn2(1) AS DOUBLE, mx2(1) AS DOUBLE
  PRINT "shape="; GFXSHAPE.AABBFROMPOINTS2D(pg(), bb()); ":"; bb(0); ","; bb(1); ","; bb(2); ","; bb(3)
  mn2(0) = 15 : mn2(1) = 15 : mx2(0) = 20 : mx2(1) = 20
  PRINT "shape="; GFXSHAPE.AABB2MAKE(mn2(), mx2(), bb2()); ":"; bb2(0); ","; bb2(3)
  PRINT "shape="; GFXSHAPE.AABBMERGE2D(bb(), bb2(), bb2()); ":"; bb2(0); ","; bb2(3)
  PRINT "shape="; GFXSHAPE.AABBCENTER2D(bb(), s2()); ":"; s2(0); ","; s2(1)
  PRINT "shape="; GFXSHAPE.AABBEXTENTS2D(bb(), s2()); ":"; s2(0); ","; s2(1)
  PRINT "shape="; GFXSHAPE.AABBVALID(bb()); ","; GFXSHAPE.AABBVALID(mn2())

  DIM pa(1) AS DOUBLE, pb(1) AS DOUBLE, r2(3) AS DOUBLE, p1(1) AS DOUBLE
  pb(0) = 3 : pb(1) = 4
  PRINT "shape="; GFXSHAPE.DIST(pa(), pb())
  r2(0) = 0 : r2(1) = 5 : r2(2) = 1 : r2(3) = 0
  p1(0) = 10 : p1(1) = 5
  PRINT "shape="; GFXSHAPE.CLOSESTRAY(r2(), p1(), s2()); ":"; s2(0); ","; s2(1)
  PRINT "shape="; GFXSHAPE.CLOSESTAABB(bb(), p1(), s2()); ":"; s2(0); ","; s2(1)
  PRINT "shape="; GFXSHAPE.CLOSESTCIRCLE(0, 0, 5, pb(), s2()); ":"; s2(0); ","; s2(1)
  PRINT "shape="; GFXSHAPE.CLOSESTAABBAABB(bb(), bb2(), s2()); ":"; s2(0); ","; s2(1)

  DIM cc1(1) AS DOUBLE, cc2(1) AS DOUBLE, t2(1) AS DOUBLE
  cc1(0) = 0 : cc1(1) = 0
  cc2(0) = 3 : cc2(1) = 0
  PRINT "shape="; GFXSHAPE.CIRCLECIRCLE(cc1(), 2, cc2(), 2); ","; GFXSHAPE.AABBAABB(bb(), bb2())
  PRINT "shape="; GFXSHAPE.AABBCIRCLE(bb(), 0, 0, 5)
  PRINT "shape="; GFXSHAPE.RAY2CIRCLE(r2(), 0, 0, 5, t2()); ":"; t2(0)
  PRINT "shape="; GFXSHAPE.RAY2AABB(r2(), bb(), t2()); ":"; t2(0); ","; t2(1)
  PRINT "shape="; GFXSHAPE.RAY2SEGMENT(r2(), pa(), cc2(), t2()); ":"; t2(0)
  PRINT "shape="; GFXSHAPE.RAY2AT(r2(), 5, s2()); ":"; s2(0); ","; s2(1)

  DIM tri3(1) AS DOUBLE, tri4(1) AS DOUBLE, tri5(1) AS DOUBLE, bari(2) AS DOUBLE
  tri3(0) = 10 : tri3(1) = 0
  tri4(0) = 0 : tri4(1) = 10
  PRINT "shape="; GFXSHAPE.BARYCENTRIC(pa(), tri3(), tri4(), p1(), bari()); ":"; bari(0); ","; bari(1); ","; bari(2)
  DIM u2(1) AS DOUBLE, v2(1) AS DOUBLE, ctr2(1) AS DOUBLE
  u2(0) = 1 : u2(1) = 0 : v2(0) = 0 : v2(1) = 1
  PRINT "shape="; GFXSHAPE.POINTINORIENTEDRECT(ctr2(), u2(), v2(), 2, 2, p1())
  DIM q1(1) AS DOUBLE, q2(1) AS DOUBLE, q3(1) AS DOUBLE, q4(1) AS DOUBLE
  q1(0) = 0 : q1(1) = 0 : q2(0) = 4 : q2(1) = 0
  q3(0) = 4 : q3(1) = 4 : q4(0) = 0 : q4(1) = 4
  PRINT "shape="; GFXSHAPE.POINTINQUAD(q1(), q2(), q3(), q4(), p1())
  PRINT "shape="; GFXSHAPE.HEXAREA(2); ","; GFXSHAPE.HEXLONG(2); ","; GFXSHAPE.HEXSHORT(2); ","; GFXSHAPE.HEXINNER(2)

  DIM cla(5) AS DOUBLE, clb(5) AS DOUBLE
  cla(3) = 1 : clb(3) = 1 : clb(5) = 2
  PRINT "shape="; GFXSHAPE.CHAMFER(cla(), 6, clb(), 6); ","; GFXSHAPE.HAUSDORFF(cla(), 6, clb(), 6)
END SUB

' ═══ 4. GFX2D — immagini float, filtri, bitmap ═════════════════════════
SUB Tour2D()
  DIM a(15) AS DOUBLE, o(15) AS DOUBLE, gx(15) AS DOUBLE, gy(15) AS DOUBLE
  FOR i = 0 TO 15 : a(i) = i : NEXT i
  DIM k3(8) AS DOUBLE, kx(2) AS DOUBLE, ky(2) AS DOUBLE
  k3(4) = 1 : kx(0) = 0 : kx(1) = 1 : kx(2) = 0 : ky(0) = 0 : ky(1) = 1 : ky(2) = 0
  PRINT "2d="; GFX2D.CONVOLVE(a(), 4, 4, k3(), 3, 3, o()); ":"; o(5)
  PRINT "2d="; GFX2D.CONVOLVESEP(a(), 4, 4, kx(), 3, ky(), 3, o()); ":"; o(6)
  PRINT "2d="; GFX2D.GAUSSIAN(a(), 4, 4, 0.8, o()); ":"; o(0); ","; o(15)
  PRINT "2d="; GFX2D.SOBEL(a(), 4, 4, gx(), gy()); ":"; gx(5); ","; gy(5)
  PRINT "2d="; GFX2D.MEDIAN3(a(), 4, 4, o()); ":"; o(5)
  PRINT "2d="; GFX2D.RESAMPLE(a(), 4, 4, 2, 2, o()); ":"; o(0); ","; o(3)
  PRINT "2d="; GFX2D.TRANSPOSE(a(), 4, 4, o()); ":"; o(1); ","; o(4)
  PRINT "2d="; GFX2D.ROTATE90(a(), 4, 4, 1, o()); ":"; o(0); ","; o(15)
  DIM m5(24) AS DOUBLE, m5o(24) AS DOUBLE
  FOR i = 0 TO 24 : m5(i) = i : NEXT i
  PRINT "2d="; GFX2D.ROTATE(m5(), 5, 5, 0, -1, m5o()); ":"; m5o(12)
  PRINT "2d="; GFX2D.FLIPH(a(), 4, 4, o()); ":"; o(0); ","; o(3)
  PRINT "2d="; GFX2D.FLIPV(a(), 4, 4, o()); ":"; o(0); ","; o(15)
  PRINT "2d="; GFX2D.THRESHOLD(a(), 4, 4, 7.5, 0, 1, o()); ":"; o(0); ","; o(15)
  PRINT "2d="; GFX2D.NORMALIZE(a(), 4, 4, 0, 1, o()); ":"; o(0); ","; o(15)
  DIM hist(3) AS DOUBLE
  PRINT "2d="; GFX2D.HISTOGRAM(a(), 4, 4, 4, 0, 16, hist()); ":"; hist(0); ","; hist(1); ","; hist(2); ","; hist(3)
  PRINT "2d="; GFX2D.HISTEQUALIZE(a(), 4, 4, 4, o()); ":"; o(0); ","; o(15)

  DIM d16(15) AS DOUBLE, e16(15) AS DOUBLE, f64(63) AS DOUBLE
  PRINT "2d="; GFX2D.DRAWLINE(d16(), 4, 4, 0, 0, 3, 3, 2.5); ":"; d16(0); ","; d16(5)
  PRINT "2d="; GFX2D.DRAWRECTF(e16(), 4, 4, 0, 0, 2, 2, 5); ":"; e16(0); ","; e16(15)
  PRINT "2d="; GFX2D.DRAWRECT(e16(), 4, 4, 1, 1, 2, 2, 7); ":"; e16(5); ","; e16(6)
  PRINT "2d="; GFX2D.DRAWCIRCLE(f64(), 8, 8, 3, 3, 2, 1, 0); ":"; f64(11); ","; f64(27)
  DIM va(1) AS DOUBLE, vb(1) AS DOUBLE, vc(1) AS DOUBLE
  va(0) = 0 : va(1) = 0 : vb(0) = 3 : vb(1) = 0 : vc(0) = 0 : vc(1) = 3
  PRINT "2d="; GFX2D.DRAWTRIANGLE(e16(), 4, 4, va(), vb(), vc(), 1.5, 1); ":"; e16(0); ","; e16(15)

  DIM imw(3) AS DOUBLE, fxw(3) AS DOUBLE, fyw(3) AS DOUBLE, wow(3) AS DOUBLE
  imw(0) = 1 : imw(1) = 2 : imw(2) = 3 : imw(3) = 4
  PRINT "2d="; GFX2D.WARP(imw(), 1, 2, 2, fxw(), fyw(), wow()); ":"; wow(0); ","; wow(3)
  DIM tg(8) AS DOUBLE, src(8) AS DOUBLE, msk(8) AS DOUBLE, pout(8) AS DOUBLE
  src(4) = 1 : msk(4) = 1
  PRINT "2d="; GFX2D.POISSON(tg(), src(), msk(), 3, 3, pout(), 30); ":"; pout(4); ","; pout(0)

  DIM vol(7) AS DOUBLE, vk(2) AS DOUBLE, vo(7) AS DOUBLE
  FOR i = 0 TO 7 : vol(i) = i : NEXT i
  vk(0) = 0.25 : vk(1) = 0.5 : vk(2) = 0.25
  PRINT "2d="; GFX2D.IMG3CONVOLVE(vol(), 2, 2, 2, vk(), 3, vo()); ":"; vo(0); ","; vo(7)
  DIM zz(3) AS DOUBLE, hm(3) AS DOUBLE
  zz(0) = 0 : zz(1) = 1 : zz(2) = 2 : zz(3) = 3
  PRINT "2d="; GFX2D.HEATMAP(zz(), 2, 2, 0, 3, hm()); ":"; hm(0); ","; hm(3)

  PRINT "2d="; GFX2D.IMGMIN(a(), 4, 4); ","; GFX2D.IMGMAX(a(), 4, 4); ","; GFX2D.IMGABSMAX(a(), 4, 4)
  DIM sc(15) AS DOUBLE, s2i(15) AS DOUBLE, sb(15) AS DOUBLE
  FOR i = 0 TO 15 : sc(i) = a(i) : s2i(i) = 1 : NEXT i
  PRINT "2d="; GFX2D.IMGSCALE(sc(), 4, 4, 2); ":"; sc(0); ","; sc(15)
  PRINT "2d="; GFX2D.IMGSUB(a(), s2i(), 4, 4, sb()); ":"; sb(0); ","; sb(15)

  DIM hp(15) AS DOUBLE, hq(15) AS DOUBLE
  FOR i = 0 TO 15 : hp(i) = a(i) : NEXT i
  PRINT "2d="; GFX2D.IMGGET(hp(), 4, 4, 1, 2); ","; GFX2D.IMGGET(hp(), 4, 4, 9, 9)
  PRINT "2d="; GFX2D.IMGSET(hp(), 4, 4, 1, 2, 99); ":"; GFX2D.IMGGET(hp(), 4, 4, 1, 2)
  PRINT "2d="; GFX2D.IMGFILL(hq(), 4, 4, 3); ":"; hq(0); ","; hq(15)
  PRINT "2d="; GFX2D.IMGCLONE(hp(), 4, 4, hq()); ":"; hq(9); ","; hq(0)

  DIM iv3(7) AS DOUBLE
  PRINT "2d="; GFX2D.IMG3SET(iv3(), 2, 2, 2, 1, 1, 1, 7.5); ":"; GFX2D.IMG3GET(iv3(), 2, 2, 2, 1, 1, 1)
  PRINT "2d="; GFX2D.IMG3FILL(iv3(), 2, 2, 2, 2); ":"; iv3(0); ","; iv3(7)

  DIM px(3) AS DOUBLE, gb(3) AS DOUBLE, bi(3) AS DOUBLE, fi(3) AS DOUBLE
  px(0) = 16711680 : px(1) = 65280 : px(2) = 255 : px(3) = 16777215
  PRINT "2d="; GFX2D.GRAYNAIVE(px(), 2, 2, gb()); ":"; gb(0); ","; gb(3)
  PRINT "2d="; GFX2D.GRAYLUM(px(), 2, 2, gb()); ":"; gb(0); ","; gb(3)
  DIM ik(8) AS DOUBLE
  ik(4) = 1
  PRINT "2d="; GFX2D.CONVOLVE3(px(), 2, 2, ik(), 1, bi()); ":"; bi(0); ","; bi(3)
  PRINT "2d="; GFX2D.BRIGHTNESS(px(), 2, 2, 10, 1, bi()); ":"; bi(0)
  PRINT "2d="; GFX2D.TOIMAGE(px(), 2, 2, fi()); ":"; fi(0); ","; fi(3)
  PRINT "2d="; GFX2D.BMPGET(px(), 2, 2, 0, 1); ","; GFX2D.BMPGET(px(), 2, 2, 9, 9)
  PRINT "2d="; GFX2D.BMPSET(px(), 2, 2, 1, 1, 8421504); ":"; px(3)
  PRINT "2d="; GFX2D.BMPFILL(px(), 2, 2, 255); ":"; px(0); ","; px(3)
END SUB

' ═══ 5. GFX3D — geometria, raggi, mesh, raster ═════════════════════════
SUB Tour3D()
  DIM mm(15) AS DOUBLE, oo(15) AS DOUBLE, o3(2) AS DOUBLE
  PRINT "3d="; GFX3D.PERSPECTIVE(1.5707963267948966, 1.5, 0.1, 100, mm); ":"; mm(0); ","; mm(5); ","; mm(11); ","; mm(15)
  PRINT "3d="; GFX3D.ORTHO(-2, 2, -1, 1, 0.1, 100, mm); ":"; mm(0); ","; mm(5); ","; mm(15)
  DIM eye(2) AS DOUBLE, ctr(2) AS DOUBLE, upv(2) AS DOUBLE
  eye(2) = 5 : upv(1) = 1
  PRINT "3d="; GFX3D.LOOKAT(eye(), ctr(), upv(), mm); ":"; mm(0); ","; mm(5); ","; mm(15)
  DIM pts(8) AS DOUBLE, ab(5) AS DOUBLE, ab2(5) AS DOUBLE
  FOR i = 0 TO 8 : pts(i) = i - 4 : NEXT i
  PRINT "3d="; GFX3D.AABBFROMPOINTS(pts(), ab()); ":"; ab(0); ","; ab(3)
  ab2(0) = 2 : ab2(1) = 2 : ab2(2) = 2 : ab2(3) = 3 : ab2(4) = 3 : ab2(5) = 3
  PRINT "3d="; GFX3D.AABBMERGE(ab(), ab2(), oo()); ":"; oo(0); ","; oo(3)
  PRINT "3d="; GFX3D.AABBCENTER(ab(), o3()); ":"; o3(0); ","; o3(1); ","; o3(2)
  PRINT "3d="; GFX3D.AABBEXTENTS(ab(), o3()); ":"; o3(0); ","; o3(1)
  DIM apt(2) AS DOUBLE
  PRINT "3d="; GFX3D.AABBTESTPOINT(ab(), apt()); ","; GFX3D.AABBINTERSECT(ab(), ab2())

  DIM planes(23) AS DOUBLE, farp(2) AS DOUBLE, bx(5) AS DOUBLE
  farp(2) = -100
  bx(0) = -1 : bx(1) = -1 : bx(2) = -1 : bx(3) = 1 : bx(4) = 1 : bx(5) = 1
  DIM scale(2) AS DOUBLE, rot4(3) AS DOUBLE, pos3(2) AS DOUBLE
  PRINT "3d="; GFX3D.DECOMPOSESRT(mm, scale(), rot4(), pos3()); ":"; scale(0); ","; pos3(2)

  ' camera: vista, proiezione e view-projection; il frustum nasce dalla view-projection
  DIM vp(15) AS DOUBLE
  PRINT "3d="; GFX3D.CAMERAVIEW(eye(), ctr(), upv(), vp); ":"; vp(0); ","; vp(15)
  PRINT "3d="; GFX3D.CAMERAPROJECTION(1.5707963, 1, 0.1, 100, vp); ":"; vp(0); ","; vp(5)
  PRINT "3d="; GFX3D.CAMERAVIEWPROJECTION(eye(), ctr(), upv(), 1.5707963, 1, 0.1, 100, vp); ":"; vp(5); ","; vp(15)
  PRINT "3d="; GFX3D.FRUSTUMFROM(vp(), planes()); ":"; planes(0)
  PRINT "3d="; GFX3D.FRUSTUMTESTPOINT(planes(), apt()); ","; GFX3D.FRUSTUMTESTPOINT(planes(), farp())
  PRINT "3d="; GFX3D.FRUSTUMTESTAABB(planes(), bx())
  DIM orb(2) AS DOUBLE, dl(2) AS DOUBLE
  dl(0) = 1
  PRINT "3d="; GFX3D.CAMERAORBIT(eye(), ctr(), 0, 0.5, 6, orb()); ":"; orb(1); ","; orb(2)
  PRINT "3d="; GFX3D.CAMERATRANSLATE(eye(), ctr(), dl(), orb()); ":"; orb(0)
  PRINT "3d="; GFX3D.CAMERANDCRAY(eye(), ctr(), upv(), 1.5707963, 1.5, 0, 0, orb()); ":"; orb(0); ","; orb(2)

  DIM rr(5) AS DOUBLE, sph(3) AS DOUBLE, tt(1) AS DOUBLE, tuv(2) AS DOUBLE
  rr(2) = 5 : rr(5) = -1 : sph(3) = 1
  PRINT "3d="; GFX3D.RAYSPHERE(rr(), sph(), tt()); ":"; tt(0)
  DIM pl(5) AS DOUBLE, tri9(8) AS DOUBLE
  pl(5) = 1
  PRINT "3d="; GFX3D.RAYPLANE(rr(), pl(), tt()); ":"; tt(0)
  tri9(0) = -1 : tri9(1) = -1 : tri9(3) = 1 : tri9(4) = -1 : tri9(6) = 0 : tri9(7) = 1
  PRINT "3d="; GFX3D.RAYTRIANGLE(rr(), tri9(), tuv()); ":"; tuv(0); ","; tuv(1); ","; tuv(2)
  PRINT "3d="; GFX3D.RAYAABB(rr(), bx(), tt()); ":"; tt(0); ","; tt(1)
  PRINT "3d="; GFX3D.RAYBOX(rr(), bx(), tt()); ":"; tt(0)
  DIM vdir(2) AS DOUBLE, nrm(2) AS DOUBLE, refr(2) AS DOUBLE
  vdir(2) = -1 : nrm(2) = 1
  PRINT "3d="; GFX3D.RAYREFLECT(vdir(), nrm(), refr()); ":"; refr(2)
  PRINT "3d="; GFX3D.RAYREFRACT(vdir(), nrm(), 0.75, refr()); ":"; refr(2)
  DIM shp(2) AS DOUBLE, shn(2) AS DOUBLE, shv(2) AS DOUBLE, shl(2) AS DOUBLE, php(9) AS DOUBLE
  shp(2) = 1 : shn(2) = 1 : shv(2) = 1 : shl(2) = 1
  php(0) = 1 : php(3) = 1 : php(6) = 0.2 : php(7) = 0.6 : php(8) = 0.2 : php(9) = 16
  PRINT "3d="; GFX3D.SHADEPHONG(shp(), shn(), shv(), shl(), php(), o3()); ":"; o3(0); ","; o3(1); ","; o3(2)

  DIM sc1(2) AS DOUBLE, sc2(2) AS DOUBLE, sdist(0) AS DOUBLE
  sc2(0) = 1.5
  PRINT "3d="; GFX3D.SPHERESPHERE(sc1(), 1, sc2(), 1); ","; GFX3D.SPHEREBOX(sc1(), 1, bx())
  sc2(2) = 0.5
  PRINT "3d="; GFX3D.SPHEREPLANE(sc2(), 1, pl(), sdist()); ":"; sdist(0)
  DIM sa(2) AS DOUBLE, segB(2) AS DOUBLE, st(0) AS DOUBLE
  sa(2) = -1 : segB(2) = 1
  PRINT "3d="; GFX3D.SEGMENTPLANE(sa(), segB(), pl(), st()); ":"; st(0)
  sa(2) = -2 : segB(2) = 2
  PRINT "3d="; GFX3D.SEGMENTSPHERE(sa(), segB(), sc1(), 1, st()); ":"; st(0)
  PRINT "3d="; GFX3D.SEGMENTTRIANGLE(sa(), segB(), tri9(), st()); ":"; st(0)
  DIM t2v(8) AS DOUBLE
  t2v(1) = -2 : t2v(2) = -2 : t2v(4) = 2 : t2v(5) = -2 : t2v(7) = 0 : t2v(8) = 2
  PRINT "3d="; GFX3D.TRIANGLETRIANGLE(tri9(), t2v())

  DIM vsg(2) AS DOUBLE, vrgb(8) AS DOUBLE, vdt(2) AS DOUBLE, vout(2) AS DOUBLE, vacc(0) AS DOUBLE
  vsg(0) = 1 : vsg(1) = 1 : vsg(2) = 1
  vrgb(1) = 1 : vrgb(5) = 1 : vrgb(6) = 1
  vdt(0) = 1 : vdt(1) = 1 : vdt(2) = 1
  PRINT "3d="; GFX3D.VOLUMERENDER(3, vsg(), vrgb(), vdt(), vout(), vacc()); ":"; vout(1); ","; vacc(0)

  DIM hsph AS LONG, hbox AS LONG, hpln AS LONG, hcyl AS LONG
  hsph = GFX3D.MESHSPHERE(8, 6, 1)
  PRINT "mesh="; hsph; ":"; GFX3D.MESHNV(hsph); ","; GFX3D.MESHNT(hsph)
  DIM vpos(188) AS DOUBLE, vidx(287) AS DOUBLE
  PRINT "mesh="; GFX3D.MESHPOS(hsph, vpos()); ":"; vpos(0); ","; vpos(2)
  PRINT "mesh="; GFX3D.MESHTRI(hsph, vidx()); ":"; vidx(0); ","; vidx(2)
  ok = GFX3D.MESHFREE(hsph)
  hbox = GFX3D.MESHBOX(1, 1, 1)
  ok = GFX3D.MESHFREE(hbox)
  hpln = GFX3D.MESHPLANE(2, 2, 1, 1)
  ok = GFX3D.MESHFREE(hpln)
  hcyl = GFX3D.MESHCYLINDER(8, 1, 2)
  ok = GFX3D.MESHFREE(hcyl)
  PRINT "mesh="; hbox; ","; hpln; ","; hcyl; ","; ok

  DIM hras AS LONG, rcol(2) AS DOUBLE, rtri(8) AS DOUBLE, rcin(8) AS DOUBLE
  hras = GFX3D.RASTERNEW(4, 4)
  PRINT "raster="; hras; ":"; GFX3D.RASTERCLEAR(hras, 1.0)
  rtri(0) = 0 : rtri(1) = 0 : rtri(2) = 0 : rtri(3) = 3 : rtri(4) = 0 : rtri(5) = 0 : rtri(6) = 0 : rtri(7) = 3 : rtri(8) = 0
  DIM rcol9(8) AS DOUBLE
  rcol9(0) = 1 : rcol9(2) = 1 : rcol9(4) = 1
  PRINT "raster="; GFX3D.RASTERTRI(hras, rtri(), rcol9()); ":"; GFX3D.RASTERGET(hras, 0, 0, rcol())
  rcin(0) = 0 : rcin(1) = 0 : rcin(2) = -1 : rcin(3) = 1 : rcin(4) = 0 : rcin(5) = 0.5 : rcin(6) = 0 : rcin(7) = 1 : rcin(8) = 0.5
  DIM cout(11) AS DOUBLE
  PRINT "raster="; GFX3D.RASTERCLIPNEAR(rcin(), 0.1, cout()); ":"; cout(2); ","; cout(5)
  ok = GFX3D.RASTERFREE(hras)

  DIM zf(3) AS DOUBLE, hsurf AS LONG
  zf(0) = 0 : zf(1) = 1 : zf(2) = 2 : zf(3) = 3
  hsurf = GFX3D.MESHSURFACE(zf(), 2, 2, 1, 1, 1)
  PRINT "surface="; hsurf; ":"; GFX3D.MESHNV(hsurf); ","; GFX3D.MESHNT(hsurf)
  ok = GFX3D.MESHFREE(hsurf)

  DIM gmap(15) AS DOUBLE, hit(5) AS DOUBLE, dg(63) AS DOUBLE, dray(4) AS DOUBLE
  gmap(2) = 1 : gmap(6) = 1 : gmap(10) = 1 : gmap(14) = 1
  PRINT "dda="; GFX3D.RAYCASTDDA(gmap(), 4, 4, 0.5, 0.5, 1, 0, hit()); ":"; hit(1); ","; hit(4)
  dg(38) = 1
  dray(0) = 0.5 : dray(1) = 4.5 : dray(2) = 1 : dray(4) = 10
  PRINT "dda="; GFX3D.RAYCASTDDAEX(dg(), 8, 8, dray(), hit()); ":"; hit(1); ","; hit(2)

  DIM baseC(2) AS DOUBLE, axisC(2) AS DOUBLE, rcyl(5) AS DOUBLE, tc(1) AS DOUBLE
  axisC(1) = 1
  rcyl(0) = 5 : rcyl(1) = 1 : rcyl(2) = 0 : rcyl(3) = -1 : rcyl(4) = 0 : rcyl(5) = 0
  PRINT "solid="; GFX3D.RAYCYLINDER(rcyl(), baseC(), axisC(), 1, 5, tc()); ":"; tc(0)
  PRINT "solid="; GFX3D.RAYCONE(rcyl(), baseC(), axisC(), 1, 5, tc()); ":"; tc(0)
  DIM npnt(2) AS DOUBLE, nnrm(2) AS DOUBLE
  npnt(0) = 1 : npnt(1) = 2
  PRINT "solid="; GFX3D.CYLINDERNORMAL(baseC(), axisC(), npnt(), nnrm()); ":"; nnrm(0); ","; nnrm(1)
  PRINT "solid="; GFX3D.CONENORMAL(baseC(), axisC(), 2, npnt(), nnrm()); ":"; nnrm(0); ","; nnrm(1)

  DIM trm(15) AS DOUBLE, abx(5) AS DOUBLE
  PRINT "3d="; GFX3D.AABBTRANSFORM(bx(), mm, abx()); ":"; abx(0); ","; abx(3)
  DIM mn3(2) AS DOUBLE, mx3(2) AS DOUBLE
  mn3(0) = -1 : mn3(1) = -1 : mn3(2) = -1
  mx3(0) = 1 : mx3(1) = 1 : mx3(2) = 1
  PRINT "3d="; GFX3D.AABBMAKE(mn3(), mx3(), abx()); ":"; abx(0); ","; abx(3)
  DIM rat(2) AS DOUBLE
  PRINT "3d="; GFX3D.RAYAT(rr(), 4, rat()); ":"; rat(0); ","; rat(1); ","; rat(2)
END SUB

' ═══ 6. GFXCODEC — BC1/DXT1 ═══════════════════════════════════════════
SUB TourCodec()
  DIM rgb(47) AS DOUBLE, enc(7) AS DOUBLE, dec(47) AS DOUBLE
  FOR i = 0 TO 15
    rgb(i * 3 + 0) = i * 16
    rgb(i * 3 + 1) = i * 16
    rgb(i * 3 + 2) = i * 16
  NEXT i
  PRINT "codec="; GFXCODEC.BC1ENCODEBLOCK(rgb(), enc()); ":"; enc(0); ","; enc(7)
  PRINT "codec="; GFXCODEC.BC1DECODEBLOCK(enc(), dec()); ":"; dec(0); ","; dec(47)
  DIM img(47) AS DOUBLE, enc2(11) AS DOUBLE, dec2(47) AS DOUBLE
  FOR i = 0 TO 15
    img(i * 3 + 0) = (i * 16) MOD 256
    img(i * 3 + 1) = 255 - ((i * 16) MOD 256)
    img(i * 3 + 2) = 128
  NEXT i
  PRINT "codec="; GFXCODEC.BC1ENCODE(img(), 4, 4, enc2()); ":"; enc2(0); ","; enc2(11)
  PRINT "codec="; GFXCODEC.BC1DECODE(enc2(), 4, 4, dec2()); ":"; dec2(0); ","; dec2(47)
END SUB
