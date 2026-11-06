' NEURAL (FFI) — matrici 2-D di BASIC passate direttamente alle funzioni del pacchetto.
' L'ABI C riceve il buffer contiguo row-major (zero-copy): le dimensioni restano
' parametri espliciti perché l'FFI non le passa.
' EXPECT: fw= 0 = 3 , 7 |loss= 0 =1.0000,-1 ,-1 |bw= 0 :-1 ,-1 ,-1 ,-1 |st= 0 : 0 =1.4,1.6:0.2|y2=4.80,10.80|f= 0
IMPORT NEURAL

SUB Main()
  ' x: 2 righe × 2 colonne = [1,2 ; 3,4]
  DIM x(1, 1) AS DOUBLE
  x(0,0)=1 : x(0,1)=2
  x(1,0)=3 : x(1,1)=4
  ' y/t/grad: 2 righe × 1 colonna (dichiarate 4 righe per riuso)
  DIM t(3, 0) AS DOUBLE
  DIM y(3, 0) AS DOUBLE
  DIM g(3, 0) AS DOUBLE
  DIM gx(1, 1) AS DOUBLE
  DIM loss(0) AS DOUBLE, W(1) AS DOUBLE, b(0) AS DOUBLE
  W(0)=1 : W(1)=1 : b(0)=0

  h = NEURAL.DENSEINIT(2, 1, 7)
  NEURAL.DENSESETPARAMS(h, W, b)
  PRINT "fw="; NEURAL.DENSEFORWARD(h, x, 2, y); "="; y(0,0); ","; y(1,0);

  ' perdita su matrici: bersaglio [4,8] → errori -1,-1 → MSE 1.0
  t(0,0)=4 : t(1,0)=8
  PRINT "|loss="; NEURAL.LOSSMSE(y, t, 2, 1, g, loss); "="; FORMAT(loss(0), "#.0000");
  PRINT ","; g(0,0); ","; g(1,0);

  ' backward + SGD: gradW = gyᵀx = [-4,-6], gradb = -2
  PRINT "|bw="; NEURAL.DENSEBACKWARD(h, g, 2, gx); ":"; gx(0,0); ","; gx(0,1); ","; gx(1,0); ","; gx(1,1);
  PRINT "|st="; NEURAL.DENSESTEP(h, 0, 0.1); ":";
  PRINT NEURAL.DENSEPARAMS(h, W, b); "=";
  PRINT FORMAT(W(0), "#.0"); ","; FORMAT(W(1), "#.0"); ":"; FORMAT(b(0), "#.0");

  ' una matrice 2-D funziona anche con lo strato denso in inferenza (y = x·Wᵀ + b)
  NEURAL.DENSEFORWARD(h, x, 2, y)
  PRINT "|y2="; FORMAT(y(0,0), "#.00"); ","; FORMAT(y(1,0), "#.00");
  PRINT "|f="; NEURAL.DENSEFREE(h)
END SUB
