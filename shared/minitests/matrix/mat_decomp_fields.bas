' EXPECT: lu_l10= 0.5|lu_u00= 4|u00b= 4|su01= 2|tr10= 2|ub= 1 , 1|len= 4|u=[4, 2, 0, 2]|in= 2 , 2|m10= 0.5|qr_q00=-0.8944|qr_r01=-3.1305|sv_s00= 5.5616|ev_d00= 1.4384|ch_l11= 1.4142|sc_t00= 5.5616|sc_z00= 0.7882|po_u00= 1|po_p00= 4|eg_d0= 5.5616|eg_v00= 0.7882
' Decomposizioni come VALORI con i FATTORI come CAMPI matrice (ADR-MATRIX F4):
' `lu.L`, `qr.Q`, `sv.S`, `ev.D`, `ch.L` sono MATRIX VALUE (indicizzabili e
' utilizzabili con MATRIX.*), non handle. LINALG.FACTOR resta l'alias esplicito.
IMPORT SYSTEM

SUB Show(a AS MATRIX OF (INT, INT, DOUBLE))
  PRINT "in="; MATRIX.ROWCOUNT(a); ","; a(0, 1)
END SUB

SUB Main()
  DIM a(1, 1) AS DOUBLE
  a(0, 0) = 4 : a(0, 1) = 2
  a(1, 0) = 2 : a(1, 1) = 3

  DIM lu = LINALG.LU(a)
  PRINT "lu_l10="; MATH.ROUND(lu.L(1, 0), 4)
  PRINT "lu_u00="; MATH.ROUND(lu.U(0, 0), 4)
  ' letture ripetute dello stesso fattore (l'handle interno non va liberato),
  ' fattore come valore, MATRIX.* e UBOUND/LEN del fattore
  PRINT "u00b="; MATH.ROUND(lu.U(0, 0), 4)
  DIM su = lu.U
  PRINT "su01="; MATH.ROUND(su(0, 1), 4)
  DIM luT = MATRIX.TRANSPOSE(lu.U)
  PRINT "tr10="; MATH.ROUND(luT(1, 0), 4)
  PRINT "ub="; UBOUND(lu.U, 1); ","; UBOUND(lu.U, 2)
  PRINT "len="; LEN(lu.U)
  PRINT "u="; lu.U
  CALL Show(lu.U)
  DIM m AS MATRIX OF (INT, INT, DOUBLE)
  m = lu.L
  PRINT "m10="; MATH.ROUND(m(1, 0), 4)

  DIM qr = LINALG.QR(a)
  PRINT "qr_q00="; MATH.ROUND(qr.Q(0, 0), 4)
  PRINT "qr_r01="; MATH.ROUND(qr.R(0, 1), 4)

  DIM sv = LINALG.SVD(a)
  PRINT "sv_s00="; MATH.ROUND(sv.S(0, 0), 4)

  DIM ev = LINALG.EVD(a)
  PRINT "ev_d00="; MATH.ROUND(ev.D(0, 0), 4)

  DIM ch = LINALG.CHOLESKY(a)
  PRINT "ch_l11="; MATH.ROUND(ch.L(1, 1), 4)

  DIM sc = LINALG.SCHUR(a)
  PRINT "sc_t00="; MATH.ROUND(sc.T(0, 0), 4)
  PRINT "sc_z00="; MATH.ROUND(sc.Z(0, 0), 4)

  DIM po = LINALG.POLAR(a)
  PRINT "po_u00="; MATH.ROUND(po.U(0, 0), 4)
  PRINT "po_p00="; MATH.ROUND(po.P(0, 0), 4)

  DIM eg = LINALG.EIG(a)
  PRINT "eg_d0="; MATH.ROUND(eg.D(0, 0), 4)
  PRINT "eg_v00="; MATH.ROUND(eg.V(0, 0), 4)
END SUB
