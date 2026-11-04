IMPORT TIMESERIES
SUB Main()
  DIM s(14) AS DOUBLE = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 9, 8, 7, 6, 5}
  DIM ar AS TIMESERIES.ArimaResult
  ar = TIMESERIES.Arima(s, 1, 1, 1)
  PRINT ROUND(ar.AIC, 6)
  PRINT ROUND(ar.ArCoeffs(0), 6)
  PRINT ROUND(ar.MaCoeffs(0), 6)
  PRINT ROUND(ar.Fitted(13), 6)
  DIM hw AS TIMESERIES.HoltWintersResult
  hw = TIMESERIES.HoltWinters(s, 0.2, 0.1, 0.1, 4)
  PRINT ROUND(hw.Forecast(14), 6)
  PRINT ROUND(hw.Mse, 6)
END SUB
