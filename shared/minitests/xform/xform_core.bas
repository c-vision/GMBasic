' XFORM (FFI) — trasformazioni 3D: 18 funzioni, matrici piatte row-major.
' 3x3 = 9 double, 4x4 = 16 double. Rotazioni di 90°: valori esatti, verificabili a mano.
' EXPECT: r3z= 0|:0.0,-1.0,1.0,0.0||r3x= 0|:0.0,-1.0,1.0,0.0||mul3= 0|:0.0,-0.0,1.0,1.0,1.0||ident= 0|:1.0,0.0,1.0,1.0||normal= 0|:0.500,1.000,1.000||normal0=-1||i4= 0|:1.0,1.0,1.0,1.0||tr4= 0|:10.0,20.0,30.0||sc4= 0|:2.0,3.0,4.0||rz4= 0|:0.0,-1.0,1.0,1.0||rx4=0.0|:0.0,-1.0,1.0,0.0||ry4=0.0|:0.0,1.0,-1.0,0.0||mul4= 0|:0.0,-1.0,10.0,20.0,30.0||pt= 0|:10.0,21.0,30.0||dir= 0|:0.0,1.0,0.0||ptw= 0|:5.0,10.0,15.0||inv= 0|:0.0,1.0,-20.0,10.0,-30.0||back= 0|:1.000,0.000,0.000||from3= 0|:0.0,-1.0,0.0,1.0||from4= 0|:0.0,-1.0,1.0,0.0||short=-1|:-1||invsing= 0|:-1||f=0
IMPORT XFORM

CONST D2R = 0.017453292519943295

SUB Main()
  DIM m3(8) AS DOUBLE
  DIM n3(8) AS DOUBLE
  DIM a3(8) AS DOUBLE
  DIM b3(8) AS DOUBLE
  DIM m4(15) AS DOUBLE
  DIM n4(15) AS DOUBLE
  DIM a4(15) AS DOUBLE
  DIM b4(15) AS DOUBLE
  DIM p(2) AS DOUBLE

  ' ── 3x3: rotazione di 90° attorno a Z ─────────────────────────────────────
  ' [0 -1 0; 1 0 0; 0 0 1]
  PRINT "r3z="; XFORM.MAT3ROTZ(90 * D2R, m3)
  PRINT ":"; FORMAT(m3(0), "#.0"); ","; FORMAT(m3(1), "#.0"); ","; FORMAT(m3(3), "#.0"); ","; FORMAT(m3(4), "#.0")
  ' rotazione di 90° attorno a X: [1 0 0; 0 0 -1; 0 1 0]
  PRINT "|r3x="; XFORM.MAT3ROTX(90 * D2R, a3)
  PRINT ":"; FORMAT(a3(4), "#.0"); ","; FORMAT(a3(5), "#.0"); ","; FORMAT(a3(7), "#.0"); ","; FORMAT(a3(8), "#.0")
  ' prodotto r3z * r3x = [0 0 1; 1 0 0; 0 1 0]
  PRINT "|mul3="; XFORM.MAT3MUL(m3, a3, n3)
  PRINT ":"; FORMAT(n3(0), "#.0"); ","; FORMAT(n3(1), "#.0"); ","; FORMAT(n3(2), "#.0"); ","; FORMAT(n3(3), "#.0"); ","; FORMAT(n3(7), "#.0")

  ' normal matrix di diag(2,1,1) = diag(0.5,1,1)
  PRINT "|ident="; XFORM.MAT3IDENTITY(a3)
  PRINT ":"; FORMAT(a3(0), "#.0"); ","; FORMAT(a3(1), "#.0"); ","; FORMAT(a3(4), "#.0"); ","; FORMAT(a3(8), "#.0")
  a3(0) = 2 : a3(4) = 1 : a3(8) = 1
  PRINT "|normal="; XFORM.MAT3NORMAL(a3, n3)
  PRINT ":"; FORMAT(n3(0), "#.000"); ","; FORMAT(n3(4), "#.000"); ","; FORMAT(n3(8), "#.000")
  ' matrice singolare -> -1
  a3(0) = 0 : a3(4) = 0 : a3(8) = 0
  PRINT "|normal0="; XFORM.MAT3NORMAL(a3, n3)

  ' ── 4x4: identità, traslazione, scala, rotazione ──────────────────────────
  PRINT "|i4="; XFORM.MAT4IDENTITY(m4)
  PRINT ":"; FORMAT(m4(0), "#.0"); ","; FORMAT(m4(5), "#.0"); ","; FORMAT(m4(10), "#.0"); ","; FORMAT(m4(15), "#.0")
  PRINT "|tr4="; XFORM.MAT4TRANSLATE(10, 20, 30, a4)
  PRINT ":"; FORMAT(a4(3), "#.0"); ","; FORMAT(a4(7), "#.0"); ","; FORMAT(a4(11), "#.0")
  PRINT "|sc4="; XFORM.MAT4SCALE(2, 3, 4, b4)
  PRINT ":"; FORMAT(b4(0), "#.0"); ","; FORMAT(b4(5), "#.0"); ","; FORMAT(b4(10), "#.0")
  PRINT "|rz4="; XFORM.MAT4ROTZ(90 * D2R, m4)
  PRINT ":"; FORMAT(m4(0), "#.0"); ","; FORMAT(m4(1), "#.0"); ","; FORMAT(m4(4), "#.0"); ","; FORMAT(m4(15), "#.0")
  PRINT "|rx4="; FORMAT(XFORM.MAT4ROTX(90 * D2R, n4), "#.0")
  PRINT ":"; FORMAT(n4(5), "#.0"); ","; FORMAT(n4(6), "#.0"); ","; FORMAT(n4(9), "#.0"); ","; FORMAT(n4(10), "#.0")
  PRINT "|ry4="; FORMAT(XFORM.MAT4ROTY(90 * D2R, n4), "#.0")
  PRINT ":"; FORMAT(n4(0), "#.0"); ","; FORMAT(n4(2), "#.0"); ","; FORMAT(n4(8), "#.0"); ","; FORMAT(n4(10), "#.0")

  ' ── composizione: M = T(10,20,30) * Rz(90) ────────────────────────────────
  PRINT "|mul4="; XFORM.MAT4MUL(a4, m4, n4)
  PRINT ":"; FORMAT(n4(0), "#.0"); ","; FORMAT(n4(1), "#.0"); ","; FORMAT(n4(3), "#.0"); ","; FORMAT(n4(7), "#.0"); ","; FORMAT(n4(11), "#.0")

  ' punto (1,0,0): Rz lo porta in (0,1,0), poi +T = (10,21,30)
  PRINT "|pt="; XFORM.MAT4POINT(n4, 1, 0, 0, p)
  PRINT ":"; FORMAT(p(0), "#.0"); ","; FORMAT(p(1), "#.0"); ","; FORMAT(p(2), "#.0")
  ' direzione (1,0,0): la traslazione non la tocca -> (0,1,0)
  PRINT "|dir="; XFORM.MAT4DIR(n4, 1, 0, 0, p)
  PRINT ":"; FORMAT(p(0), "#.0"); ","; FORMAT(p(1), "#.0"); ","; FORMAT(p(2), "#.0")
  ' w=2 sulla traslazione: punto diviso per 2 -> (5, 10.5, 15)
  a4(15) = 2
  PRINT "|ptw="; XFORM.MAT4POINT(a4, 0, 0, 0, p)
  PRINT ":"; FORMAT(p(0), "#.0"); ","; FORMAT(p(1), "#.0"); ","; FORMAT(p(2), "#.0")

  ' ── inversa affine: riporta il punto trasformato all'origine ──────────────
  PRINT "|inv="; XFORM.MAT4INVERSEAFFINE(n4, a4)
  PRINT ":"; FORMAT(a4(0), "#.0"); ","; FORMAT(a4(1), "#.0"); ","; FORMAT(a4(3), "#.0"); ","; FORMAT(a4(7), "#.0"); ","; FORMAT(a4(11), "#.0")
  ' applico l'inversa a (10,21,30) -> (1,0,0)
  PRINT "|back="; XFORM.MAT4POINT(a4, 10, 21, 30, p)
  PRINT ":"; FORMAT(p(0), "#.000"); ","; FORMAT(p(1), "#.000"); ","; FORMAT(p(2), "#.000")

  ' ── conversioni 3x3 <-> 4x4 ───────────────────────────────────────────────
  PRINT "|from3="; XFORM.MAT4FROM3(m3, b4)
  PRINT ":"; FORMAT(b4(0), "#.0"); ","; FORMAT(b4(1), "#.0"); ","; FORMAT(b4(3), "#.0"); ","; FORMAT(b4(15), "#.0")
  PRINT "|from4="; XFORM.MAT3FROM4(b4, n3)
  PRINT ":"; FORMAT(n3(0), "#.0"); ","; FORMAT(n3(1), "#.0"); ","; FORMAT(n3(3), "#.0"); ","; FORMAT(n3(4), "#.0")

  ' ── guardie: array troppo corto / matrice singolare ───────────────────────
  DIM short3(2) AS DOUBLE
  PRINT "|short="; XFORM.MAT3ROTZ(1, short3)
  PRINT ":"; XFORM.MAT4IDENTITY(short3)
  ' inversa affine di una matrice singolare (prima colonna nulla) -> -1
  PRINT "|invsing="; XFORM.MAT4IDENTITY(a4)
  a4(0) = 0
  PRINT ":"; XFORM.MAT4INVERSEAFFINE(a4, b4)
  PRINT "|f=0"
END SUB
