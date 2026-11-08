' Lambda param ARRAY inferito dall'USO (`y(0)`) nella forma FN(...): prima la risoluzione
' cadeva sul builtin MOUSE.y (errore C99Native: "simbolo non trovato: 'y' è una funzione di
' libreria (namespace MOUSE)") — bug AnalysisMiniTest (SolveIvp). L'inferenza dall'uso ora
' vale anche quando lambda_params_arr è popolato (forma FN), non solo per i codeblock `{|..|}`.
'
' Oracolo: FORMA e VALORE della soluzione, NON l'handle di SolveIvp (è un contatore di
' registry: dipende da quante matrici hanno già memorizzato i test precedenti nello stesso
' processo — in isolamento valeva 1, dopo altri test 3; non è un oracolo valido).
' dy/dt = -y, y(0)=1, t∈[0,1] → y(1) = e^-1 ≈ 0.367879.
' EXPECT: rows= 6|yend= 0.36788
SUB Main()
  DIM y0(0) AS DOUBLE = {1}
  DIM h = NUMERICAL.SolveIvp(FN(t, y) => {-y(0)}, y0, 0.0, 1.0)
  DIM rows = MATRIX.ROWCOUNT(h)
  PRINT "rows="; rows
  PRINT "yend="; MATH.ROUND(MATRIX.GETVALUE(h, rows - 1, 1), 6)
END SUB
