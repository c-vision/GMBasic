' QUANT (FFI, libgmbquant) — PORTFOLIO (19) + BACKTEST (1).
' Portafoglio a 3 asset: cov definita positiva, rendimenti attesi {0.10,0.05,0.02}.
' I risultati composti sono ARRAY: [rendimento, volatilita', Sharpe, pesi...].
' EXPECT: er=0.069000||sd=0.111803||sh=0.590000||lo=-1||mad=0.026667||smad=0.015000||mv=0.026316:0.184211:0.789474||ms=0.040000:1.000000||mvar=1.000000||rp=1.000000||ew=0.333333||mr=0.100000: 1  0  0||kelly=0.000000||mcvar=-0.020000: 1||mcdar=0.000000: 1||ev=1.000000||bl=1.000000:0.092964||pvar=2.000000||pcvar=0.020000||bt=5.060000:34.469682:56.124861: 2 :0.020000:0.030000||btf=5.060000||f=0
IMPORT PORTFOLIO
IMPORT BACKTEST

SUB Main()
  DIM w(2) AS DOUBLE : w = {0.5, 0.3, 0.2}
  DIM m(2) AS DOUBLE : m = {0.10, 0.05, 0.02}
  DIM cov(2) AS ARRAY OF DOUBLE
  cov(0) = {0.04, 0.005, 0.0} : cov(1) = {0.005, 0.01, 0.0} : cov(2) = {0.0, 0.0, 0.0025}
  DIM o() AS DOUBLE

  PRINT "er="; FORMAT(PORTFOLIO.EXPECTEDRETURN(w, m), "#.######")
  PRINT "|sd="; FORMAT(PORTFOLIO.STDEV(w, cov), "#.######")
  PRINT "|sh="; FORMAT(PORTFOLIO.SHARPERATIO(0.069, 0.1, 0.01), "#.######")
  PRINT "|lo="; PORTFOLIO.ISLONGONLY(w)
  PRINT "|mad="; FORMAT(PORTFOLIO.PORTFOLIOMAD(m), "#.######")
  PRINT "|smad="; FORMAT(PORTFOLIO.PORTFOLIOSEMIMAD(m, 0.06), "#.######")

  o = PORTFOLIO.MINVOLATILITY(cov)
  PRINT "|mv="; FORMAT(o(3), "#.######"); ":"; FORMAT(o(4), "#.######"); ":"; FORMAT(o(5), "#.######")
  o = PORTFOLIO.MAXSHARPE(m, cov, 0.0)
  PRINT "|ms="; FORMAT(o(0), "#.######"); ":"; FORMAT(o(3) + o(4) + o(5), "#.######")
  o = PORTFOLIO.MEANVARIANCE(m, cov)
  PRINT "|mvar="; FORMAT(o(3) + o(4) + o(5), "#.######")
  o = PORTFOLIO.RISKPARITY(cov)
  PRINT "|rp="; FORMAT(o(3) + o(4) + o(5), "#.######")
  o = PORTFOLIO.EQUALLYWEIGHTED(3)
  PRINT "|ew="; FORMAT(o(3), "#.######")
  o = PORTFOLIO.MAXRETURN(m)
  PRINT "|mr="; FORMAT(o(0), "#.######"); ":"; o(3); o(4); o(5)
  DIM hm(0) AS ARRAY OF DOUBLE : hm(0) = {0.10, 0.05, 0.02}
  o = PORTFOLIO.KELLYCRITERION(hm, 0.01)
  PRINT "|kelly="; FORMAT(o(0), "#.######")
  o = PORTFOLIO.MINCVAR(hm, 0.95, 0.01)
  PRINT "|mcvar="; FORMAT(o(0), "#.######"); ":"; o(3)
  o = PORTFOLIO.MINCDAR(hm, 0.95, 0.01)
  PRINT "|mcdar="; FORMAT(o(0), "#.######"); ":"; o(3)
  o = PORTFOLIO.EVOLVE(m, cov)
  PRINT "|ev="; FORMAT(o(3) + o(4) + o(5), "#.######")

  DIM caps(2) AS DOUBLE : caps = {100, 200, 300}
  DIM P(2) AS ARRAY OF DOUBLE
  P(0) = {1.0, 0.0, 0.0} : P(1) = {0.0, 1.0, 0.0} : P(2) = {0.0, 0.0, 1.0}
  DIM Q(2) AS DOUBLE : Q = {0.10, 0.05, 0.02}
  DIM om(2) AS ARRAY OF DOUBLE
  om(0) = {0.0001, 0.0, 0.0} : om(1) = {0.0, 0.0001, 0.0} : om(2) = {0.0, 0.0, 0.0001}
  o = PORTFOLIO.BLACKLITTERMAN(caps, cov, 2.5, 0.05, P, Q, om, 0.01)
  PRINT "|bl="; FORMAT(o(3) + o(4) + o(5), "#.######"); ":"; FORMAT(o(1), "#.######")

  DIM hist(2) AS ARRAY OF DOUBLE
  hist(0) = {0.02, -0.01, 0.03, 0.01, -0.02}
  hist(1) = {0.01, 0.005, -0.005, 0.02, 0.01}
  hist(2) = {0.005, 0.01, -0.01, 0.015, 0.005}
  PRINT "|pvar="; FORMAT(PORTFOLIO.PVAR(hist, w, 0.95), "#.######")
  PRINT "|pcvar="; FORMAT(PORTFOLIO.CVAR(hist, w, 0.95), "#.######")

  ' BACKTEST: due periodi x due asset; rendimento per periodo = media della riga.
  DIM data(1, 1) AS DOUBLE
  data(0, 0) = 0.01 : data(0, 1) = 0.03
  data(1, 0) = 0.02 : data(1, 1) = 0.04
  DIM bo(7) AS DOUBLE
  DIM kb AS LONG
  kb = BACKTEST.SIMULATE(data, bo)
  PRINT "|bt="; FORMAT(bo(0), "#.######"); ":"; FORMAT(bo(1), "#.######"); ":"; FORMAT(bo(3), "#.######"); ":"; bo(4); ":"; FORMAT(bo(6), "#.######"); ":"; FORMAT(bo(7), "#.######")
  DIM flat(3) AS DOUBLE
  flat(0) = 0.01 : flat(1) = 0.03 : flat(2) = 0.02 : flat(3) = 0.04
  DIM fr() AS DOUBLE
  fr = BACKTEST.RUNFLAT(flat, 2, 2)
  PRINT "|btf="; FORMAT(fr(0), "#.######")
  PRINT "|f=0"
END SUB
