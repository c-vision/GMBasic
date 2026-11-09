' MiniTest NAMESPACE TRADING.PORTFOLIO — tutte le funzioni della superficie (EVOLVE a parte).
' Derivato dai MiniTest per-funzione (stessi input e oracoli).
' EXPECT: 3|0.02|3|0.069|-1|1|3|3|3|1|1|3|0.02666667|0|2|3|0.7|0.1118034|3
IMPORT PORTFOLIO

SUB P_blacklitterman()
  DIM caps(2) AS DOUBLE : caps = {100.0, 200.0, 300.0}
  DIM cov(2) AS ARRAY OF DOUBLE
  cov(0) = {0.04, 0.005, 0.0} : cov(1) = {0.005, 0.01, 0.0} : cov(2) = {0.0, 0.0, 0.0025}
  DIM P(2) AS ARRAY OF DOUBLE
  P(0) = {1.0, 0.0, 0.0} : P(1) = {0.0, 1.0, 0.0} : P(2) = {0.0, 0.0, 1.0}
  DIM Q(2) AS DOUBLE : Q = {0.10, 0.05, 0.02}
  DIM omega(2) AS ARRAY OF DOUBLE
  omega(0) = {0.0001, 0.0, 0.0} : omega(1) = {0.0, 0.0001, 0.0} : omega(2) = {0.0, 0.0, 0.0001}
  DIM x() AS DOUBLE
  x = PORTFOLIO.BLACKLITTERMAN(caps, cov, 2.5, 0.05, P, Q, omega, 0.01)
  PRINT UBOUND(x) - 2
END SUB

SUB P_cvar()
  DIM w(2) AS DOUBLE : w = {0.5, 0.3, 0.2}
  DIM r(2) AS DOUBLE : r = {0.10, 0.05, 0.02}
  DIM cov(2, 2) AS DOUBLE
  cov = {{0.04, 0.005, 0.0}, {0.005, 0.01, 0.0}, {0.0, 0.0, 0.0025}}
  DIM hist(2, 4) AS DOUBLE
  hist = {{0.02, -0.01, 0.03, 0.01, -0.02}, {0.01, 0.005, -0.005, 0.02, 0.01}, {0.005, 0.01, -0.01, 0.015, 0.005}}
  PRINT ROUND(PORTFOLIO.CVAR(hist, w, 0.95), 8)
END SUB

SUB P_equallyweighted()
  DIM w(2) AS DOUBLE : w = {0.5, 0.3, 0.2}
  DIM r(2) AS DOUBLE : r = {0.10, 0.05, 0.02}
  DIM cov(2) AS ARRAY OF DOUBLE
  cov(0) = {0.04, 0.005, 0.0} : cov(1) = {0.005, 0.01, 0.0} : cov(2) = {0.0, 0.0, 0.0025}
  DIM hist(2) AS ARRAY OF DOUBLE
  hist(0) = {0.02, -0.01, 0.03, 0.01, -0.02} : hist(1) = {0.01, 0.005, -0.005, 0.02, 0.01} : hist(2) = {0.005, 0.01, -0.01, 0.015, 0.005}
  DIM x() AS DOUBLE
  x = PORTFOLIO.EQUALLYWEIGHTED(3)
  PRINT UBOUND(x) - 2
END SUB

SUB P_expectedreturn()
  DIM w(2) AS DOUBLE : w = {0.5, 0.3, 0.2}
  DIM r(2) AS DOUBLE : r = {0.10, 0.05, 0.02}
  DIM cov(2) AS ARRAY OF DOUBLE
  cov(0) = {0.04, 0.005, 0.0} : cov(1) = {0.005, 0.01, 0.0} : cov(2) = {0.0, 0.0, 0.0025}
  DIM hist(2) AS ARRAY OF DOUBLE
  hist(0) = {0.02, -0.01, 0.03, 0.01, -0.02} : hist(1) = {0.01, 0.005, -0.005, 0.02, 0.01} : hist(2) = {0.005, 0.01, -0.01, 0.015, 0.005}
  PRINT ROUND(PORTFOLIO.EXPECTEDRETURN(w, r), 8)
END SUB

SUB P_islongonly()
  DIM w(2) AS DOUBLE : w = {0.5, 0.3, 0.2}
  DIM r(2) AS DOUBLE : r = {0.10, 0.05, 0.02}
  DIM cov(2) AS ARRAY OF DOUBLE
  cov(0) = {0.04, 0.005, 0.0} : cov(1) = {0.005, 0.01, 0.0} : cov(2) = {0.0, 0.0, 0.0025}
  DIM hist(2) AS ARRAY OF DOUBLE
  hist(0) = {0.02, -0.01, 0.03, 0.01, -0.02} : hist(1) = {0.01, 0.005, -0.005, 0.02, 0.01} : hist(2) = {0.005, 0.01, -0.01, 0.015, 0.005}
  PRINT ROUND(PORTFOLIO.ISLONGONLY(w), 8)
END SUB

SUB P_kellycriterion()
  DIM w(2) AS DOUBLE : w = {0.5, 0.3, 0.2}
  DIM r(2) AS DOUBLE : r = {0.10, 0.05, 0.02}
  DIM cov(2) AS ARRAY OF DOUBLE
  cov(0) = {0.04, 0.005, 0.0} : cov(1) = {0.005, 0.01, 0.0} : cov(2) = {0.0, 0.0, 0.0025}
  DIM hist(2) AS ARRAY OF DOUBLE
  hist(0) = {0.02, -0.01, 0.03, 0.01, -0.02} : hist(1) = {0.01, 0.005, -0.005, 0.02, 0.01} : hist(2) = {0.005, 0.01, -0.01, 0.015, 0.005}
  DIM rh(0) AS ARRAY OF DOUBLE : rh(0) = r
  DIM x() AS DOUBLE
  x = PORTFOLIO.KELLYCRITERION(rh, 0.01)
  PRINT UBOUND(x) - 2
END SUB

SUB P_maxreturn()
  DIM w(2) AS DOUBLE : w = {0.5, 0.3, 0.2}
  DIM r(2) AS DOUBLE : r = {0.10, 0.05, 0.02}
  DIM cov(2) AS ARRAY OF DOUBLE
  cov(0) = {0.04, 0.005, 0.0} : cov(1) = {0.005, 0.01, 0.0} : cov(2) = {0.0, 0.0, 0.0025}
  DIM hist(2) AS ARRAY OF DOUBLE
  hist(0) = {0.02, -0.01, 0.03, 0.01, -0.02} : hist(1) = {0.01, 0.005, -0.005, 0.02, 0.01} : hist(2) = {0.005, 0.01, -0.01, 0.015, 0.005}
  DIM x() AS DOUBLE
  x = PORTFOLIO.MAXRETURN(r)
  PRINT UBOUND(x) - 2
END SUB

SUB P_maxsharpe()
  DIM w(2) AS DOUBLE : w = {0.5, 0.3, 0.2}
  DIM r(2) AS DOUBLE : r = {0.10, 0.05, 0.02}
  DIM cov(2, 2) AS DOUBLE
  cov = {{0.04, 0.005, 0.0}, {0.005, 0.01, 0.0}, {0.0, 0.0, 0.0025}}
  DIM hist(2) AS ARRAY OF DOUBLE
  hist(0) = {0.02, -0.01, 0.03, 0.01, -0.02} : hist(1) = {0.01, 0.005, -0.005, 0.02, 0.01} : hist(2) = {0.005, 0.01, -0.01, 0.015, 0.005}
  DIM x() AS DOUBLE
  x = PORTFOLIO.MAXSHARPE(r, cov, 0.01)
  PRINT UBOUND(x) - 2
END SUB

SUB P_meanvariance()
  DIM w(2) AS DOUBLE : w = {0.5, 0.3, 0.2}
  DIM r(2) AS DOUBLE : r = {0.10, 0.05, 0.02}
  DIM cov(2, 2) AS DOUBLE
  cov = {{0.04, 0.005, 0.0}, {0.005, 0.01, 0.0}, {0.0, 0.0, 0.0025}}
  DIM hist(2) AS ARRAY OF DOUBLE
  hist(0) = {0.02, -0.01, 0.03, 0.01, -0.02} : hist(1) = {0.01, 0.005, -0.005, 0.02, 0.01} : hist(2) = {0.005, 0.01, -0.01, 0.015, 0.005}
  DIM x() AS DOUBLE
  x = PORTFOLIO.MEANVARIANCE(r, cov)
  PRINT UBOUND(x) - 2
END SUB

SUB P_mincdar()
  DIM w(2) AS DOUBLE : w = {0.5, 0.3, 0.2}
  DIM r(2) AS DOUBLE : r = {0.10, 0.05, 0.02}
  DIM cov(2) AS ARRAY OF DOUBLE
  cov(0) = {0.04, 0.005, 0.0} : cov(1) = {0.005, 0.01, 0.0} : cov(2) = {0.0, 0.0, 0.0025}
  DIM hist(2) AS ARRAY OF DOUBLE
  hist(0) = {0.02, -0.01, 0.03, 0.01, -0.02} : hist(1) = {0.01, 0.005, -0.005, 0.02, 0.01} : hist(2) = {0.005, 0.01, -0.01, 0.015, 0.005}
  DIM x() AS DOUBLE
  x = PORTFOLIO.MINCDAR(hist, 0.95, 0.01)
  PRINT UBOUND(x) - 2
END SUB

SUB P_mincvar()
  DIM w(2) AS DOUBLE : w = {0.5, 0.3, 0.2}
  DIM r(2) AS DOUBLE : r = {0.10, 0.05, 0.02}
  DIM cov(2) AS ARRAY OF DOUBLE
  cov(0) = {0.04, 0.005, 0.0} : cov(1) = {0.005, 0.01, 0.0} : cov(2) = {0.0, 0.0, 0.0025}
  DIM hist(2) AS ARRAY OF DOUBLE
  hist(0) = {0.02, -0.01, 0.03, 0.01, -0.02} : hist(1) = {0.01, 0.005, -0.005, 0.02, 0.01} : hist(2) = {0.005, 0.01, -0.01, 0.015, 0.005}
  DIM x() AS DOUBLE
  x = PORTFOLIO.MINCVAR(hist, 0.95, 0.01)
  PRINT UBOUND(x) - 2
END SUB

SUB P_minvolatility()
  DIM w(2) AS DOUBLE : w = {0.5, 0.3, 0.2}
  DIM r(2) AS DOUBLE : r = {0.10, 0.05, 0.02}
  DIM cov(2, 2) AS DOUBLE
  cov = {{0.04, 0.005, 0.0}, {0.005, 0.01, 0.0}, {0.0, 0.0, 0.0025}}
  DIM hist(2) AS ARRAY OF DOUBLE
  hist(0) = {0.02, -0.01, 0.03, 0.01, -0.02} : hist(1) = {0.01, 0.005, -0.005, 0.02, 0.01} : hist(2) = {0.005, 0.01, -0.01, 0.015, 0.005}
  DIM x() AS DOUBLE
  x = PORTFOLIO.MINVOLATILITY(cov)
  PRINT UBOUND(x) - 2
END SUB

SUB P_portfoliomad()
  DIM w(2) AS DOUBLE : w = {0.5, 0.3, 0.2}
  DIM r(2) AS DOUBLE : r = {0.10, 0.05, 0.02}
  DIM cov(2) AS ARRAY OF DOUBLE
  cov(0) = {0.04, 0.005, 0.0} : cov(1) = {0.005, 0.01, 0.0} : cov(2) = {0.0, 0.0, 0.0025}
  DIM hist(2) AS ARRAY OF DOUBLE
  hist(0) = {0.02, -0.01, 0.03, 0.01, -0.02} : hist(1) = {0.01, 0.005, -0.005, 0.02, 0.01} : hist(2) = {0.005, 0.01, -0.01, 0.015, 0.005}
  PRINT ROUND(PORTFOLIO.PORTFOLIOMAD(r), 8)
END SUB

SUB P_portfoliosemimad()
  DIM w(2) AS DOUBLE : w = {0.5, 0.3, 0.2}
  DIM r(2) AS DOUBLE : r = {0.10, 0.05, 0.02}
  DIM cov(2) AS ARRAY OF DOUBLE
  cov(0) = {0.04, 0.005, 0.0} : cov(1) = {0.005, 0.01, 0.0} : cov(2) = {0.0, 0.0, 0.0025}
  DIM hist(2) AS ARRAY OF DOUBLE
  hist(0) = {0.02, -0.01, 0.03, 0.01, -0.02} : hist(1) = {0.01, 0.005, -0.005, 0.02, 0.01} : hist(2) = {0.005, 0.01, -0.01, 0.015, 0.005}
  PRINT ROUND(PORTFOLIO.PORTFOLIOSEMIMAD(r), 8)
END SUB

SUB P_pvar()
  DIM w(2) AS DOUBLE : w = {0.5, 0.3, 0.2}
  DIM r(2) AS DOUBLE : r = {0.10, 0.05, 0.02}
  DIM cov(2, 2) AS DOUBLE
  cov = {{0.04, 0.005, 0.0}, {0.005, 0.01, 0.0}, {0.0, 0.0, 0.0025}}
  DIM hist(2, 4) AS DOUBLE
  hist = {{0.02, -0.01, 0.03, 0.01, -0.02}, {0.01, 0.005, -0.005, 0.02, 0.01}, {0.005, 0.01, -0.01, 0.015, 0.005}}
  PRINT ROUND(PORTFOLIO.PVAR(hist, w, 0.95), 8)
END SUB

SUB P_riskparity()
  DIM w(2) AS DOUBLE : w = {0.5, 0.3, 0.2}
  DIM r(2) AS DOUBLE : r = {0.10, 0.05, 0.02}
  DIM cov(2, 2) AS DOUBLE
  cov = {{0.04, 0.005, 0.0}, {0.005, 0.01, 0.0}, {0.0, 0.0, 0.0025}}
  DIM hist(2) AS ARRAY OF DOUBLE
  hist(0) = {0.02, -0.01, 0.03, 0.01, -0.02} : hist(1) = {0.01, 0.005, -0.005, 0.02, 0.01} : hist(2) = {0.005, 0.01, -0.01, 0.015, 0.005}
  DIM x() AS DOUBLE
  x = PORTFOLIO.RISKPARITY(cov)
  PRINT UBOUND(x) - 2
END SUB

SUB P_sharperatio()
  DIM pr AS DOUBLE = 0.15 : DIM rk AS DOUBLE = 0.2 : DIM rf AS DOUBLE = 0.01
  PRINT ROUND(PORTFOLIO.SHARPERATIO(pr, rk, rf), 8)
END SUB

SUB P_stdev()
  DIM w(2) AS DOUBLE : w = {0.5, 0.3, 0.2}
  DIM r(2) AS DOUBLE : r = {0.10, 0.05, 0.02}
  DIM cov(2, 2) AS DOUBLE
  cov = {{0.04, 0.005, 0.0}, {0.005, 0.01, 0.0}, {0.0, 0.0, 0.0025}}
  DIM hist(2, 4) AS DOUBLE
  hist = {{0.02, -0.01, 0.03, 0.01, -0.02}, {0.01, 0.005, -0.005, 0.02, 0.01}, {0.005, 0.01, -0.01, 0.015, 0.005}}
  PRINT ROUND(PORTFOLIO.STDEV(w, cov), 8)
END SUB

SUB P_evolve()
  DIM r(2) AS DOUBLE : r = {0.10, 0.05, 0.02}
  DIM cov(2) AS ARRAY OF DOUBLE
  cov(0) = {0.04, 0.005, 0.0} : cov(1) = {0.005, 0.01, 0.0} : cov(2) = {0.0, 0.0, 0.0025}
  DIM x() AS DOUBLE
  x = PORTFOLIO.EVOLVE(r, cov)
  PRINT UBOUND(x) - 2
END SUB

SUB Main()
  P_blacklitterman()
  P_cvar()
  P_equallyweighted()
  P_expectedreturn()
  P_islongonly()
  P_kellycriterion()
  P_maxreturn()
  P_maxsharpe()
  P_meanvariance()
  P_mincdar()
  P_mincvar()
  P_minvolatility()
  P_portfoliomad()
  P_portfoliosemimad()
  P_pvar()
  P_riskparity()
  P_sharperatio()
  P_stdev()
  P_evolve()
END SUB
