' Lambda param ARRAY inferito dall'USO (`y(0)`) nella forma FN(...): prima la risoluzione
' cadeva sul builtin MOUSE.y (errore C99Native: "simbolo non trovato: 'y' è una funzione di
' libreria (namespace MOUSE)") — bug AnalysisMiniTest (SolveIvp). L'inferenza dall'uso ora
' vale anche quando lambda_params_arr è popolato (forma FN), non solo per i codeblock `{|..|}`.
' EXPECT: n= 1
SUB Main()
  DIM y0(0) AS DOUBLE = {1}
  DIM n = NUMERICAL.SolveIvp(FN(t, y) => {-y(0)}, y0, 0.0, 1.0)
  PRINT "n="; n
END SUB
