' MiniTest NAMESPACE TIMESERIES (FFI) — tutti i 32 simboli della superficie.
'
' Superficie (shared/spec/registry/surface.csv): TSAUTOCORR, TSBANDPASS, TSBANDSTOP,
' TSCOVARIANCEMATRIX, TSDECOMPOSE, TSDESEASONALIZE, TSDIFFERENCE, TSEMA, TSHIGHPASS,
' TSLOWPASS, TSMA, TSMINMAX, TSOUTLIERS, TSRETURNS, TSROLLAUTOCORR, TSROLLCORR,
' TSROLLCOVARIANCE, TSROLLDELTA, TSROLLKURTOSIS, TSROLLMAD, TSROLLMEAN, TSROLLMEDIAN,
' TSROLLMINMAX, TSROLLQUANTILE, TSROLLREGRESSION, TSROLLREGRESSIONSINGLE, TSROLLSHARPE,
' TSROLLSKEWNESS, TSROLLSORTINO, TSROLLSTDDEV, TSROLLZSCORE, TSVOLATILITY.
'
' Scopo: verificare che i 4 backend supportino la STESSA versione del namespace,
' non un'applicazione. Serie deterministica 1..10 (e 1..20 per i filtri).
' EXPECT: autocorr= 0.7|volatility= 0.268853|difference= 9 | 1|ema= 10 | 9.001953|ma= 8 | 9|minmax= 2 | 10|returns= 9 | 0.111111|deseasonalize= 10 | 0.5|lowpass= 0.1311|highpass= 0.505|bandpass= 0.0675|bandstop= 0.6389|rollmean= 8 | 9|rollcorr= 8 | 0.981981|rollcov= 8 | 1.5|rollquantile= 8 | 9|rollmedian= 8 | 9|rollmad= 8 | 1|rollautocorr= 8 | 0|rolldelta= 9 | 1|rollkurtosis= 6 |-1.2|rollskewness= 8 | 0|rollstddev= 8 | 0.816497|rollzscore= 8 | 1.224745|rollsharpe= 8 | 11.022704|rollsortino= 8 | 0|rollminmax= 1 , 3|rollregression= 1|rollregressionsingle= 1|decompose= 3|covmatrix= 1|outliers= 0
IMPORT TIMESERIES

SUB Main()
  DIM s(9) AS DOUBLE = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}
  DIM t(9) AS DOUBLE = {2, 4, 5, 7, 8, 10, 11, 13, 14, 16}
  DIM x(19) AS DOUBLE = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20}

  ' ── statistiche scalari ──────────────────────────────────────────────────
  PRINT "autocorr="; ROUND(TIMESERIES.TSAUTOCORR(s, 1), 6)
  PRINT "volatility="; ROUND(TIMESERIES.TSVOLATILITY(s), 6)

  ' ── serie trasformate ────────────────────────────────────────────────────
  DIM d1() = TIMESERIES.TSDIFFERENCE(s, 1)
  PRINT "difference="; LEN(d1()); "|"; ROUND(d1(UBOUND(d1)), 6)
  DIM em() = TIMESERIES.TSEMA(s, 3)
  PRINT "ema="; LEN(em()); "|"; ROUND(em(UBOUND(em)), 6)
  DIM ma() = TIMESERIES.TSMA(s, 3)
  PRINT "ma="; LEN(ma()); "|"; ROUND(ma(UBOUND(ma)), 6)
  DIM mm() = TIMESERIES.TSMINMAX(s)
  PRINT "minmax="; LEN(mm()); "|"; ROUND(mm(UBOUND(mm)), 6)
  DIM rt() = TIMESERIES.TSRETURNS(s)
  PRINT "returns="; LEN(rt()); "|"; ROUND(rt(UBOUND(rt)), 6)
  DIM ds() = TIMESERIES.TSDESEASONALIZE(s, 2)
  PRINT "deseasonalize="; LEN(ds()); "|"; ROUND(ds(UBOUND(ds)), 6)

  ' ── filtri ───────────────────────────────────────────────────────────────
  DIM lp() AS DOUBLE = TIMESERIES.TSLOWPASS(x, 0.3, 2)
  PRINT "lowpass="; ROUND(lp(0), 4)
  DIM hp() AS DOUBLE = TIMESERIES.TSHIGHPASS(x, 0.3, 2)
  PRINT "highpass="; ROUND(hp(0), 4)
  DIM bp() AS DOUBLE = TIMESERIES.TSBANDPASS(x, 0.1, 0.3, 2)
  PRINT "bandpass="; ROUND(bp(0), 4)
  DIM bs() AS DOUBLE = TIMESERIES.TSBANDSTOP(x, 0.1, 0.3, 2)
  PRINT "bandstop="; ROUND(bs(0), 4)

  ' ── finestre mobili ──────────────────────────────────────────────────────
  DIM rm() = TIMESERIES.TSROLLMEAN(s, 3)
  PRINT "rollmean="; LEN(rm()); "|"; ROUND(rm(UBOUND(rm)), 6)
  DIM rc() = TIMESERIES.TSROLLCORR(s, t, 3)
  PRINT "rollcorr="; LEN(rc()); "|"; ROUND(rc(UBOUND(rc)), 6)
  DIM rcov() = TIMESERIES.TSROLLCOVARIANCE(s, t, 3)
  PRINT "rollcov="; LEN(rcov()); "|"; ROUND(rcov(UBOUND(rcov)), 6)
  DIM rq() = TIMESERIES.TSROLLQUANTILE(s, 3, 0.5)
  PRINT "rollquantile="; LEN(rq()); "|"; ROUND(rq(UBOUND(rq)), 6)
  DIM rmd() = TIMESERIES.TSROLLMEDIAN(s, 3)
  PRINT "rollmedian="; LEN(rmd()); "|"; ROUND(rmd(UBOUND(rmd)), 6)
  DIM rmad() = TIMESERIES.TSROLLMAD(s, 3)
  PRINT "rollmad="; LEN(rmad()); "|"; ROUND(rmad(UBOUND(rmad)), 6)
  DIM rac() = TIMESERIES.TSROLLAUTOCORR(s, 3, 1)
  PRINT "rollautocorr="; LEN(rac()); "|"; ROUND(rac(UBOUND(rac)), 6)
  DIM rd() = TIMESERIES.TSROLLDELTA(s, 2)
  PRINT "rolldelta="; LEN(rd()); "|"; ROUND(rd(UBOUND(rd)), 6)
  DIM rk() = TIMESERIES.TSROLLKURTOSIS(s, 5)
  PRINT "rollkurtosis="; LEN(rk()); "|"; ROUND(rk(UBOUND(rk)), 6)
  DIM rs() = TIMESERIES.TSROLLSKEWNESS(s, 3)
  PRINT "rollskewness="; LEN(rs()); "|"; ROUND(rs(UBOUND(rs)), 6)
  DIM rsd() = TIMESERIES.TSROLLSTDDEV(s, 3)
  PRINT "rollstddev="; LEN(rsd()); "|"; ROUND(rsd(UBOUND(rsd)), 6)
  DIM rz() = TIMESERIES.TSROLLZSCORE(s, 3)
  PRINT "rollzscore="; LEN(rz()); "|"; ROUND(rz(UBOUND(rz)), 6)
  DIM rsh() = TIMESERIES.TSROLLSHARPE(s, 3, 0)
  PRINT "rollsharpe="; LEN(rsh()); "|"; ROUND(rsh(UBOUND(rsh)), 6)
  DIM rso() = TIMESERIES.TSROLLSORTINO(s, 3, 0, 0)
  PRINT "rollsortino="; LEN(rso()); "|"; ROUND(rso(UBOUND(rso)), 6)

  ' ── risultati a matrice ──────────────────────────────────────────────────
  DIM rmm() AS DOUBLE = TIMESERIES.TSROLLMINMAX(s, 3)
  PRINT "rollminmax="; ROUND(rmm(0), 4); ","; ROUND(rmm(1), 4)

  DIM rr() AS DOUBLE = TIMESERIES.TSROLLREGRESSION(s, s, 3)
  PRINT "rollregression="; ROUND(rr(0), 4)
  DIM rrs() AS DOUBLE = TIMESERIES.TSROLLREGRESSIONSINGLE(s, 3)
  PRINT "rollregressionsingle="; ROUND(rrs(0), 4)

  DIM dec() AS DOUBLE = TIMESERIES.TSDECOMPOSE(s, 2)
  PRINT "decompose="; (UBOUND(dec) + 1) / (UBOUND(s) + 1)

  DIM pair(1) AS ARRAY OF DOUBLE
  pair(0) = {1.0, 2.0, 3.0, 4.0}
  pair(1) = {2.0, 4.0, 5.0, 8.0}
  DIM cv() AS DOUBLE = TIMESERIES.TSCOVARIANCEMATRIX(pair)
  PRINT "covmatrix="; ROUND(cv(1), 4)

  DIM ol(9) AS LONG
  DIM noc AS LONG = TIMESERIES.TSOUTLIERS(s, 2.0, ol)
  PRINT "outliers="; noc
END SUB
