' EXPECT: lu_l10= 0.5|lu_u00= 4|u00b= 4|su01= 2|tr10= 2|ub= 3|len= 4|u=[4, 2, 0, 2]|in= 2 , 2|m10= 0.5|qr_q00= 0.8944|qr_r01= 3.1305|sv_s00= 5.5616|ev_d00= 1.4384|ch_l11= 1.4142|sc_t00= 4|sc_z00= 1|po_u00= 1|po_p00= 4|eg_d0= 5.5616|eg_v00= 1
' Accesso ai FATTORI di decomposizione via FFI: LINALG.FACTOR(a, rows, cols, kind, out, meta).
' Sostituisce i campi `lu.L`/`sv.S` del vecchio oggetto builtin con buffer piatti.
IMPORT LINALG
IMPORT SYSTEM

SUB Main()
  DIM a(3) AS DOUBLE
  a(0) = 4 : a(1) = 2
  a(2) = 2 : a(3) = 3

  DIM u(3) AS DOUBLE, meta(1) AS LONG
  LINALG.FACTOR(a, 2, 2, "LU.U", u, meta)
  DIM l(3) AS DOUBLE
  LINALG.FACTOR(a, 2, 2, "LU.L", l, meta)
  PRINT "lu_l10="; MATH.ROUND(l(2), 4)
  PRINT "lu_u00="; MATH.ROUND(u(0), 4)
  PRINT "u00b="; MATH.ROUND(u(0), 4)
  PRINT "su01="; MATH.ROUND(u(1), 4)
  ' trasposta del fattore U (buffer piatto 2×2): tr(1,0) = U(0,1)
  PRINT "tr10="; MATH.ROUND(u(1), 4)
  PRINT "ub="; UBOUND(u, 1)
  PRINT "len="; LEN(u)
  PRINT "u="; u
  PRINT "in="; meta(0); ","; meta(1)
  PRINT "m10="; MATH.ROUND(l(2), 4)

  DIM q(3) AS DOUBLE, r(3) AS DOUBLE
  LINALG.FACTOR(a, 2, 2, "QR.Q", q, meta)
  LINALG.FACTOR(a, 2, 2, "QR.R", r, meta)
  PRINT "qr_q00="; MATH.ROUND(q(0), 4)
  PRINT "qr_r01="; MATH.ROUND(r(1), 4)

  DIM s(3) AS DOUBLE
  LINALG.FACTOR(a, 2, 2, "SVD.S", s, meta)
  PRINT "sv_s00="; MATH.ROUND(s(0), 4)

  DIM ev(3) AS DOUBLE
  LINALG.FACTOR(a, 2, 2, "EVD.D", ev, meta)
  PRINT "ev_d00="; MATH.ROUND(ev(0), 4)

  DIM ch(3) AS DOUBLE
  LINALG.FACTOR(a, 2, 2, "CHOL.L", ch, meta)
  PRINT "ch_l11="; MATH.ROUND(ch(3), 4)

  DIM sc(3) AS DOUBLE, sz(3) AS DOUBLE
  LINALG.FACTOR(a, 2, 2, "SCHUR.T", sc, meta)
  LINALG.FACTOR(a, 2, 2, "SCHUR.Z", sz, meta)
  PRINT "sc_t00="; MATH.ROUND(sc(0), 4)
  PRINT "sc_z00="; MATH.ROUND(sz(0), 4)

  DIM po(3) AS DOUBLE, pp(3) AS DOUBLE
  LINALG.FACTOR(a, 2, 2, "POLAR.U", po, meta)
  LINALG.FACTOR(a, 2, 2, "POLAR.P", pp, meta)
  PRINT "po_u00="; MATH.ROUND(po(0), 4)
  PRINT "po_p00="; MATH.ROUND(pp(0), 4)

  DIM eg(3) AS DOUBLE, egv(3) AS DOUBLE
  LINALG.FACTOR(a, 2, 2, "EIG.DREAL", eg, meta)
  LINALG.FACTOR(a, 2, 2, "EIG.VREAL", egv, meta)
  PRINT "eg_d0="; MATH.ROUND(eg(0), 4)
  PRINT "eg_v00="; MATH.ROUND(egv(0), 4)
END SUB
