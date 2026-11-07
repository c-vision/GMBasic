' SPECTRAL (FFI) — trasformate: 10 funzioni, array piatti di double.
' La lunghezza del segnale è quella DELL'ARRAY: dichiarare l'array della misura esatta.
' Le identità (DCT/IDCT, DCT2/IDCT2, STFT/ISTFT) si verificano per ricostruzione;
' i valori esatti sono su segnali costanti, dove si calcolano a mano.
' EXPECT: dct-cost= 4|:2.000000,0.000000,-0.000000,-0.000000||dct= 4|:5.000000,-2.230442,-0.000000,-0.158513||idct= 4|:1.000000,2.000000,3.000000,4.000000||dst= 4|:1.847759,-0.000000,0.765367,0.000000||dct2= 9|:15.000000,-2.449490,-0.000000||idct2= 9|:1.000000,5.000000,9.000000||haar= 4|:4.000000,2.000000,1.414214,0.000000||hilbert= 8|:0.000000,-0.707107,-1.000000||z1= 2|:4.000000,0.000000||zi= 2|:0.000000,0.000000||z2= 2|:1.875000,0.000000||stft= 3|:2.000000,-0.000000||istft= 16|:0.000000,0.707107,-1.000000,1.000000||guard=-1|:-1|:-1||f=0
IMPORT SPECTRAL

CONST PIVAL = 3.14159265358979323846

SUB Main()
  DIM x4(3) AS DOUBLE
  DIM y4(3) AS DOUBLE
  DIM z4(3) AS DOUBLE
  DIM m9(8) AS DOUBLE
  DIM n9(8) AS DOUBLE
  DIM h2(1) AS DOUBLE
  DIM x8(7) AS DOUBLE
  DIM y8(7) AS DOUBLE
  DIM w8(7) AS DOUBLE
  DIM x16(15) AS DOUBLE
  DIM st(29) AS DOUBLE
  DIM rec(15) AS DOUBLE
  DIM corto(1) AS DOUBLE
  DIM uno(0) AS DOUBLE
  DIM i AS LONG

  ' ── DCT di una costante [1,1,1,1] ─────────────────────────────────────────
  x4(0) = 1 : x4(1) = 1 : x4(2) = 1 : x4(3) = 1
  PRINT "dct-cost="; SPECTRAL.DCT(x4, y4)
  PRINT ":"; FORMAT(y4(0), "#.000000"); ","; FORMAT(y4(1), "#.000000"); ","; FORMAT(y4(2), "#.000000"); ","; FORMAT(y4(3), "#.000000")

  ' ── DCT/IDCT su [1,2,3,4]: ricostruzione esatta ───────────────────────────
  x4(0) = 1 : x4(1) = 2 : x4(2) = 3 : x4(3) = 4
  PRINT "|dct="; SPECTRAL.DCT(x4, y4)
  PRINT ":"; FORMAT(y4(0), "#.000000"); ","; FORMAT(y4(1), "#.000000"); ","; FORMAT(y4(2), "#.000000"); ","; FORMAT(y4(3), "#.000000")
  PRINT "|idct="; SPECTRAL.IDCT(y4, z4)
  PRINT ":"; FORMAT(z4(0), "#.000000"); ","; FORMAT(z4(1), "#.000000"); ","; FORMAT(z4(2), "#.000000"); ","; FORMAT(z4(3), "#.000000")

  ' ── DST di una costante ───────────────────────────────────────────────────
  x4(0) = 1 : x4(1) = 1 : x4(2) = 1 : x4(3) = 1
  PRINT "|dst="; SPECTRAL.DST(x4, y4)
  PRINT ":"; FORMAT(y4(0), "#.000000"); ","; FORMAT(y4(1), "#.000000"); ","; FORMAT(y4(2), "#.000000"); ","; FORMAT(y4(3), "#.000000")

  ' ── DCT 2D e inversa su una matrice 3x3 ───────────────────────────────────
  FOR i = 0 TO 8
    m9(i) = i + 1
  NEXT i
  PRINT "|dct2="; SPECTRAL.DCT2(m9, 3, 3, n9)
  PRINT ":"; FORMAT(n9(0), "#.000000"); ","; FORMAT(n9(1), "#.000000"); ","; FORMAT(n9(4), "#.000000")
  PRINT "|idct2="; SPECTRAL.IDCT2(n9, 3, 3, m9)
  PRINT ":"; FORMAT(m9(0), "#.000000"); ","; FORMAT(m9(4), "#.000000"); ","; FORMAT(m9(8), "#.000000")

  ' ── Haar su [4,2,1,1] (4 campioni) ────────────────────────────────────────
  x4(0) = 4 : x4(1) = 2 : x4(2) = 1 : x4(3) = 1
  PRINT "|haar="; SPECTRAL.HAAR(x4, y4)
  PRINT ":"; FORMAT(y4(0), "#.000000"); ","; FORMAT(y4(1), "#.000000"); ","; FORMAT(y4(2), "#.000000"); ","; FORMAT(y4(3), "#.000000")

  ' ── Hilbert di un coseno (8 campioni, 1 periodo) ──────────────────────────
  ' x[n] = cos(2*pi*n/8); il risultato atteso e' sin(2*pi*n/8)
  FOR i = 0 TO 7
    x8(i) = MATH.COS(2 * PIVAL * i / 8)
  NEXT i
  PRINT "|hilbert="; SPECTRAL.HILBERT(x8, y8)
  PRINT ":"; FORMAT(y8(0), "#.000000"); ","; FORMAT(y8(1), "#.000000"); ","; FORMAT(y8(2), "#.000000")

  ' ── Z-trasformata di [1,1,1,1] ────────────────────────────────────────────
  x4(0) = 1 : x4(1) = 1 : x4(2) = 1 : x4(3) = 1
  PRINT "|z1="; SPECTRAL.ZTRANSFORM(x4, 1, 0, h2)
  PRINT ":"; FORMAT(h2(0), "#.000000"); ","; FORMAT(h2(1), "#.000000")
  PRINT "|zi="; SPECTRAL.ZTRANSFORM(x4, 0, 1, h2)
  PRINT ":"; FORMAT(h2(0), "#.000000"); ","; FORMAT(h2(1), "#.000000")
  PRINT "|z2="; SPECTRAL.ZTRANSFORM(x4, 2, 0, h2)
  PRINT ":"; FORMAT(h2(0), "#.000000"); ","; FORMAT(h2(1), "#.000000")

  ' ── STFT di un coseno con finestra di Hann (N=8, hop=4, 16 campioni) ──────
  FOR i = 0 TO 7
    w8(i) = 0.5 * (1 - MATH.COS(2 * PIVAL * i / 8))
  NEXT i
  ' tono esattamente sul bin 1 della finestra di 8 campioni
  FOR i = 0 TO 15
    x16(i) = MATH.COS(2 * PIVAL * i / 8)
  NEXT i
  PRINT "|stft="; SPECTRAL.STFT(x16, 8, w8, 4, st)
  PRINT ":"; FORMAT(st(2), "#.000000"); ","; FORMAT(st(3), "#.000000")

  ' ── iSTFT: ricostruzione del segnale ──────────────────────────────────────
  PRINT "|istft="; SPECTRAL.ISTFT(st, 3, 8, w8, 4, rec)
  PRINT ":"; FORMAT(rec(0), "#.000000"); ","; FORMAT(rec(1), "#.000000"); ","; FORMAT(rec(4), "#.000000"); ","; FORMAT(rec(8), "#.000000")

  ' ── guardie: segnale più corto della finestra / out troppo corto ─────────
  corto(0) = 1 : corto(1) = 1
  PRINT "|guard="; SPECTRAL.STFT(corto, 8, w8, 4, st)
  PRINT ":"; SPECTRAL.ZTRANSFORM(x4, 1, 0, uno)
  PRINT ":"; SPECTRAL.DCT2(m9, 3, 3, corto)
  PRINT "|f=0"
END SUB
