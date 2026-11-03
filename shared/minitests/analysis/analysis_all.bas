' MiniTest NAMESPACE ANALYSIS — tutte le 44 funzioni della superficie.
'
' Copre: descrittive, regressione/PCA/KMeans, serie temporali (ARIMA/Holt-Winters/
' ADF/Yule-Walker/MA), interpolazioni, integrazione/derivata, ODE (SolveOde/SolveIvp/
' Integrate/Simpson), FFT/IFFT (reali/complesse/rfft/irfft), spline (Splrep/Splev).
' Derivato dai MiniTest per-funzione (stessi input e oracoli).
' Scopo: verificare la STESSA versione del namespace sui 4 backend, non un'applicazione.
' EXPECT: interp:  3 , 3 , 2|interp2:  3 , 3 , 5|euclid:  30|yw:  1.33333333333333 , 0.333333333333333|ma:  0.5 , 0.057143|4.8|5|3.3466401061363|11.2|0.088041393098|-1.975446428571|3|-0.058600903867|9.5|2.25|6.5|4|6.25|6.21875|9|6.5|40|0.5  3  9.5|0  0.166666666666667  0.333333333333333 -0.5 -0.333333333333333 -0.166666666666667|2  3  0  1|2  3  0  1|4  1|0.25 -0.3|1.96|0.2|0.999375650364|1|10.5|-4.220358|0.792388|0.024221|-0.792388|9.16044|11.023513|F= 4.8|adf_stat= 0  adf_p= 0.75|pca= ok|0.333333333333333|0.333333333333333|0.367879774412498|rows= 2|cols= 2|yend= 0|splev= 12.25|splrep_ok= 1|f00= 3|y00= 0|0  0  1  0|0  1  0  1|rfft_ok= 1|irfft0= 0.25|d= 6
IMPORT ANALYSIS
IMPORT MATH
IMPORT MATRIX


SUB A_analysis_interp_real()
    DIM x() = {0, 1, 2, 3, 4}
    DIM y() = {0, 2, 4, 6, 8}
    PRINT "interp: "; ANALYSIS.INTERPOLATELINEAR(x, y, 1.5); ","; ANALYSIS.CUBICSPLINE(x, y, 1.5); ","; ANALYSIS.INTERPOLATESTEP(x, y, 1.5)
    PRINT "interp2: "; ANALYSIS.INTERPOLATEPCHIP(x, y, 1.5); ","; ANALYSIS.INTERPOLATEPOLYNOMIAL(x, y, 1.5); ","; ANALYSIS.INTERPOLATESPLINE(x, y, 2.5)
    PRINT "euclid: "; ANALYSIS.EUCLIDEANDISTANCESQUARED(x, y)
    DIM d() = {1, 2, 3, 4, 5, 6}
    DIM yw() = ANALYSIS.YULEWALKER(d, 2)
    PRINT "yw: "; yw(0); ","; yw(1)
    DIM ma() = ANALYSIS.MACOEFFICIENTS(d, 2)
    PRINT "ma: "; ma(0); ","; MATH.ROUND(ma(1), 6)
END SUB


SUB A_desc()
  DIM d(4) AS DOUBLE = {5, 2, 9, 1, 7}
  PRINT ANALYSIS.MEAN(d)
  PRINT ANALYSIS.MEDIAN(d)
  PRINT ANALYSIS.STDDEV(d)
  PRINT ANALYSIS.VARIANCE(d)
  PRINT ROUND(ANALYSIS.SKEWNESS(d), 12)
  PRINT ROUND(ANALYSIS.KURTOSIS(d), 12)
  PRINT ROUND(ANALYSIS.MAD(d), 12)
  DIM e(4) AS DOUBLE = {9, 4, 3, 6, 8}
  PRINT ROUND(ANALYSIS.CORRELATION(d, e), 12)
END SUB


SUB A_num()
  DIM x(3) AS DOUBLE = {0, 1, 2, 3}
  DIM y(3) AS DOUBLE = {0, 1, 4, 9}
  PRINT ANALYSIS.TRAPEZOID(y, x)
  PRINT ANALYSIS.CUBICSPLINE(x, y, 1.5)
  PRINT ANALYSIS.INTERPOLATELINEAR(x, y, 2.5)
  PRINT ANALYSIS.INTERPOLATESTEP(x, y, 2.5)
  PRINT ANALYSIS.INTERPOLATEPOLYNOMIAL(x, y, 2.5)
  PRINT ANALYSIS.INTERPOLATEPCHIP(x, y, 2.5)
  PRINT ANALYSIS.INTERPOLATECLAMP(x, y, 5)
  PRINT ANALYSIS.INTERPOLATEGENERIC("linear", x, y, 2.5)
  PRINT ANALYSIS.EUCLIDEANDISTANCESQUARED(x, y)
END SUB


SUB A_arr()
  DIM y(3) AS DOUBLE = {0, 1, 4, 9}
  DIM x(3) AS DOUBLE = {0, 1, 2, 3}
  DIM ct() = ANALYSIS.CUMTRAPEZOID(y, x)
  PRINT ct(0); ct(1); ct(2)
  DIM f6() = ANALYSIS.FFTFREQ(6, 1)
  PRINT f6(0); f6(1); f6(2); f6(3); f6(4); f6(5)
  DIM fs() = ANALYSIS.FFTSHIFT(x)
  PRINT fs(0); fs(1); fs(2); fs(3)
  DIM ifs() = ANALYSIS.IFFTSHIFT(x)
  PRINT ifs(0); ifs(1); ifs(2); ifs(3)
  DIM yw() = ANALYSIS.YULEWALKER(y, 2)
  PRINT yw(0); yw(1)
  DIM mc() = ANALYSIS.MACOEFFICIENTS(x, 2)
  PRINT mc(0); mc(1)
END SUB


SUB A_udt()
  DIM x(3) AS DOUBLE = {1, 2, 3, 4}
  DIM y(3) AS DOUBLE = {2.1, 4.2, 6.1, 8.0}
  DIM r AS Analysis.RegressionResult
  r = ANALYSIS.REGRESS(x, y)
  PRINT ROUND(r.Slope, 12)
  PRINT ROUND(r.Intercept, 12)
  PRINT ROUND(r.R2, 12)
  DIM d(4) AS DOUBLE = {1, 2, 10, 11}
  DIM k AS Analysis.KMeansResult
  k = ANALYSIS.KMEANS(d, 2)
  PRINT k.Centroids(0)
  PRINT k.Centroids(1)
END SUB


SUB A_arima_hw()
  DIM s(14) AS DOUBLE = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 9, 8, 7, 6, 5}
  DIM ar AS Analysis.ArimaResult
  ar = ANALYSIS.ARIMA(s, 1, 1, 1)
  PRINT ROUND(ar.AIC, 6)
  PRINT ROUND(ar.ArCoeffs(0), 6)
  PRINT ROUND(ar.MaCoeffs(0), 6)
  PRINT ROUND(ar.Fitted(13), 6)
  DIM hw AS Analysis.HoltWintersResult
  hw = ANALYSIS.HOLTWINTERS(s, 0.2, 0.1, 0.1, 4)
  PRINT ROUND(hw.Forecast(14), 6)
  PRINT ROUND(hw.Mse, 6)
END SUB


SUB A_anova()
  DIM g(1) AS ARRAY OF DOUBLE
  g(0) = {2.0, 3.0, 4.0, 5.0}
  g(1) = {4.0, 5.0, 6.0, 7.0}
  DIM aov AS ANALYSIS.AnovaResult
  aov = ANALYSIS.ANOVA(g)
  PRINT "F="; MATH.ROUND(aov.FValue, 4)
END SUB


SUB A_adftest()
  DIM d(49) AS DOUBLE
  FOR i = 0 TO 49: d(i) = i: NEXT
  DIM r AS ANALYSIS.AdfTestResult
  r = ANALYSIS.AdfTest(d)
  PRINT "adf_stat="; ROUND(r.Statistic, 6); " adf_p="; ROUND(r.PValue, 6)
END SUB


SUB A_pca()
  DIM data(3, 1) AS DOUBLE
  data(0,0) = 1: data(0,1) = 2
  data(1,0) = 2: data(1,1) = 3
  data(2,0) = 3: data(2,1) = 4
  data(3,0) = 4: data(3,1) = 5
  DIM r AS ANALYSIS.PcaResult
  r = ANALYSIS.Pca(data)
  PRINT "pca= ok"
END SUB


SUB A_ode()
  PRINT ANALYSIS.INTEGRATE(FN(x) => x * x, 0, 1)
  PRINT ANALYSIS.SIMPSON(FN(x) => x * x, 0, 1, 256)
  DIM ys() = ANALYSIS.SOLVEODE({ |t, y| RETURN -y }, 1.0, 0.0, 1.0, 10)
  PRINT ys(10)
END SUB


SUB A_solveivp()
  DIM y0(0) AS DOUBLE = {1}
  DIM h = ANALYSIS.SOLVEIVP(FN(t AS DOUBLE, y() AS DOUBLE) AS ARRAY OF DOUBLE => {-1}, y0, 0.0, 1.0)
  DIM rows = MATRIX.ROWCOUNT(h)
  PRINT "rows="; rows
  PRINT "cols="; MATRIX.COLUMNCOUNT(h)
  PRINT "yend="; MATH.ROUND(MATRIX.GETVALUE(h, rows - 1, 1), 6)
END SUB


SUB A_splev()
  DIM xs(4) AS DOUBLE = {0, 1, 2, 3, 4}
  DIM ys(4) AS DOUBLE = {1, 4, 9, 16, 25}
  DIM sp = ANALYSIS.Splrep(xs, ys)
  DIM xq(0) AS DOUBLE = {2.5}
  DIM sv() = ANALYSIS.Splev(sp, xq)
  PRINT "splev="; ROUND(sv(0), 6)
END SUB


SUB A_splrep()
  DIM xs(4) AS DOUBLE = {0, 1, 2, 3, 4}
  DIM ys(4) AS DOUBLE = {1, 4, 9, 16, 25}
  DIM sp = ANALYSIS.Splrep(xs, ys)
  PRINT "splrep_ok="; IIF(sp <> 0, 1, 0)
END SUB


SUB A_fft_ifft()
  DIM x(3) AS DOUBLE = {0, 1, 2, 3}
  DIM f = ANALYSIS.FFT(x)
  PRINT "f00="; MATH.ROUND(f(0,0), 4)
  DIM y = ANALYSIS.IFFT(f)
  PRINT "y00="; MATH.ROUND(y(0,0), 4)
END SUB


SUB A_fftc()
  DIM x(3) AS DOUBLE = {1, 0, -1, 0}
  DIM imz(3) AS DOUBLE = {0, 0, 0, 0}
  DIM f() = ANALYSIS.FFTCOMPLEX(x, imz)
  PRINT f(0,0); f(0,1); f(1,0); f(1,1)
  DIM r() = ANALYSIS.IFFTCOMPLEX(x, imz)
  PRINT r(0,0); r(1,0); r(2,0); r(3,0)
END SUB


SUB A_rfft()
  DIM x(7) AS DOUBLE
  FOR i = 0 TO 7: x(i) = COS(2 * 3.141592653589793 * i / 8): NEXT
  DIM rf = ANALYSIS.Rfft(x)
  PRINT "rfft_ok="; IIF(rf <> 0, 1, 0)
END SUB


SUB A_irfft()
  DIM re(4) AS DOUBLE = {1, 0.5, 0, 0, 0}
  DIM im(4) AS DOUBLE = {0, 0, 0, 0, 0}
  DIM ir() = ANALYSIS.Irfft(re, im)
  PRINT "irfft0="; ROUND(ir(0), 6)
END SUB


SUB A_differentiate()
  DIM d = ANALYSIS.DIFFERENTIATE(FN(x AS DOUBLE) AS DOUBLE => x * x, 3.0)
  PRINT "d="; MATH.ROUND(d, 6)
END SUB


SUB Main()
  A_analysis_interp_real()
  A_desc()
  A_num()
  A_arr()
  A_udt()
  A_arima_hw()
  A_anova()
  A_adftest()
  A_pca()
  A_ode()
  A_solveivp()
  A_splev()
  A_splrep()
  A_fft_ifft()
  A_fftc()
  A_rfft()
  A_irfft()
  A_differentiate()
END SUB
