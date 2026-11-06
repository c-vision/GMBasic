' FILE: conic_stdform.bas
' OPTIMIZE (FFI conic) — trasformazione in forma standard di un LP e ritorno.
' Problema utente:  min -2x0 - x1  s.t.  x0 + x1 <= 1,  0 <= x <= 1
' Ottimo: x = (1, 0), obiettivo -2.  La forma standard ha m=3, n=5
' (x0, x1, slack del <=, due slack dei limiti superiori).
' EXPECT: sf=1||info= 0|:3.0,5.0,2.0,1.0,0.0||a= 0|:1.01.01.00.00.0|:1.00.00.01.00.0|:0.01.00.00.01.0||b= 0|:1.0,1.0,1.0||c= 0|:-2.0,-1.0,0.0,0.0,0.0||q=-1||solve= 0||mapx= 0|:1.000000,0.000000||mapy= 0|:1.418212||mapd= 0|:1.000000,0.000000||free= 0||f=0
IMPORT OPTIMIZE

SUB Main()
  DIM ci(1) AS DOUBLE
  ci(0)=-2 : ci(1)=-1
  DIM lx(1) AS DOUBLE, ux(1) AS DOUBLE
  lx(0)=0 : lx(1)=0
  ux(0)=1 : ux(1)=1
  DIM lc(0) AS DOUBLE, uc(0) AS DOUBLE
  lc(0)=-1E308
  uc(0)=1
  DIM colptr(2) AS LONG, srow(1) AS LONG, val(1) AS DOUBLE
  colptr(0)=0 : colptr(1)=1 : colptr(2)=2
  srow(0)=0 : srow(1)=0
  val(0)=1 : val(1)=1
  DIM dqL(0) AS LONG, dqD(0) AS DOUBLE

  h = OPTIMIZE.STDFORMBUILD(2, 1, ci, dqL, dqL, dqD, 0, lx, ux, lc, uc, colptr, srow, val)
  IF h > 0 THEN
    PRINT "sf=1"
  ELSE
    PRINT "sf=0"
  END IF

  DIM info(4) AS DOUBLE
  PRINT "|info="; OPTIMIZE.STDFORMINFO(h, info)
  PRINT ":"; FORMAT(info(0), "#.0"); ","; FORMAT(info(1), "#.0"); ","; FORMAT(info(2), "#.0"); ","; FORMAT(info(3), "#.0"); ","; FORMAT(info(4), "#.0")

  ' A standard 3x5: riga 0 = x0+x1+slack, righe 1/2 = limiti superiori
  DIM Ad(14) AS DOUBLE, bd(2) AS DOUBLE, cd(4) AS DOUBLE
  PRINT "|a="; OPTIMIZE.STDFORMGETA(h, Ad)
  PRINT ":"; FORMAT(Ad(0), "#.0"); FORMAT(Ad(1), "#.0"); FORMAT(Ad(2), "#.0"); FORMAT(Ad(3), "#.0"); FORMAT(Ad(4), "#.0")
  PRINT ":"; FORMAT(Ad(5), "#.0"); FORMAT(Ad(6), "#.0"); FORMAT(Ad(7), "#.0"); FORMAT(Ad(8), "#.0"); FORMAT(Ad(9), "#.0")
  PRINT ":"; FORMAT(Ad(10), "#.0"); FORMAT(Ad(11), "#.0"); FORMAT(Ad(12), "#.0"); FORMAT(Ad(13), "#.0"); FORMAT(Ad(14), "#.0")
  PRINT "|b="; OPTIMIZE.STDFORMGETB(h, bd)
  PRINT ":"; FORMAT(bd(0), "#.0"); ","; FORMAT(bd(1), "#.0"); ","; FORMAT(bd(2), "#.0")
  PRINT "|c="; OPTIMIZE.STDFORMGETC(h, cd)
  PRINT ":"; FORMAT(cd(0), "#.0"); ","; FORMAT(cd(1), "#.0"); ","; FORMAT(cd(2), "#.0"); ","; FORMAT(cd(3), "#.0"); ","; FORMAT(cd(4), "#.0")

  ' Q assente (LP) -> -1
  DIM Qd(24) AS DOUBLE
  PRINT "|q="; OPTIMIZE.STDFORMGETQ(h, Qd)

  ' risolvi il problema standard e torna nello spazio originale
  DIM xt(4) AS DOUBLE, yt(2) AS DOUBLE, zt(4) AS DOUBLE
  rc = OPTIMIZE.IPMLP(Ad, 3, 5, bd, cd, 1E-8, 1E-8, 1E-8, 200, xt, yt, zt)
  PRINT "|solve="; rc
  DIM xo(1) AS DOUBLE, yo(0) AS DOUBLE
  PRINT "|mapx="; OPTIMIZE.STDFORMMAPX(h, xt, xo)
  PRINT ":"; FORMAT(xo(0), "#.000000"); ","; FORMAT(xo(1), "#.000000")
  PRINT "|mapy="; OPTIMIZE.STDFORMMAPY(h, yt, yo)
  PRINT ":"; FORMAT(yo(0), "#.000000")
  DIM xtd(4) AS DOUBLE, dxo(1) AS DOUBLE
  xtd(0)=1 : xtd(1)=0 : xtd(2)=0 : xtd(3)=0 : xtd(4)=0
  PRINT "|mapd="; OPTIMIZE.STDFORMMAPDIR(h, xtd, dxo)
  PRINT ":"; FORMAT(dxo(0), "#.000000"); ","; FORMAT(dxo(1), "#.000000")

  PRINT "|free="; OPTIMIZE.STDFORMFREE(h)
  PRINT "|f=0"
END SUB
