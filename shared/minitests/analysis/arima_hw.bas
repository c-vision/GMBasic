' ARIMA/HoltWinters via TIMESERIES (FFI). ARIMA ha out piatto:
'   out(0)=AIC, out(1..p)=AR, out(1+p..p+q)=MA, out(1+p+q..)=Fitted(nd).
IMPORT TIMESERIES
SUB Main()
  DIM s(14) AS DOUBLE = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 9, 8, 7, 6, 5}
  DIM o(16) AS DOUBLE
  CALL TIMESERIES.ARIMA(s, 1, 1, 1, o)
  PRINT ROUND(o(0), 6)
  PRINT ROUND(o(1), 6)
  PRINT ROUND(o(2), 6)
  PRINT ROUND(o(3 + 13), 6)
  DIM fc(14) AS DOUBLE
  DIM mse AS DOUBLE = TIMESERIES.HOLTWINTERS(s, 0.2, 0.1, 0.1, 4, fc)
  PRINT ROUND(fc(14), 6)
  PRINT ROUND(mse, 6)
END SUB
