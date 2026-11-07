' backtest.inc — namespace BACKTEST: backtest a pesi uguali (FFI, libgmbquant).
'
' Era TRADING.BACKTESTS.RUN (1 funzione). Il modello e' quello del runtime condiviso:
' il rendimento di ogni periodo e' la MEDIA degli asset della riga, poi si calcolano
' TotalReturn (percentuale), AnnualizedReturn, MaxDrawdown, Sharpe (252/anno) e la
' curva dei rendimenti.
'
' ABI (differenza voluta rispetto al builtin, che restituiva un BacktestResult):
' RUN/RUNFLAT ritornano un ARRAY OF DOUBLE con questo layout
'
'     o(0) = TotalReturn (%)          o(1) = AnnualizedReturn (%)
'     o(2) = MaxDrawdown              o(3) = SharpeRatio
'     o(4) = TradesCount              o(5) = Turnover
'     o(6 .. 6+periodi-1) = curva dei rendimenti per periodo
'
' La strategia e il portfolio iniziale del builtin non sono riprodotti: il modello
' e' a pesi uguali (nessun ribilanciamento), come le gambe C del runtime.
'
' ATTENZIONE: l'FFI non accetta una matrice 2-D (`DIM m(1,1)`) dove la firma dichiara
' un array 1-D o jagged. Usare RUN con un array JAGGED (righe = periodi) oppure
' RUNFLAT con il buffer piatto row-major.

NAMESPACE BACKTEST

DECLARE FUNCTION BTN_RUN LIB "gmbquant" ALIAS "ofx_q_bt_run" (data() AS DOUBLE, cols AS LONG, out() AS DOUBLE) AS LONG

' RUN(data(periodi)(asset)[, strategia]) -> ARRAY [6 metriche + curva].
PUBLIC FUNCTION RUN(data() AS ARRAY OF DOUBLE, OPTIONAL strategia AS STRING = "") AS ARRAY OF DOUBLE
    DIM rows AS LONG = UBOUND(data) + 1
    DIM cols AS LONG = UBOUND(data(0)) + 1
    DIM flat((rows * cols) - 1) AS DOUBLE
    FOR i = 0 TO rows - 1
        FOR j = 0 TO cols - 1
            flat(i * cols + j) = data(i)(j)
        NEXT j
    NEXT i
    DIM o(5 + rows) AS DOUBLE
    DIM k AS LONG
    k = BTN_RUN(flat, cols, o)
    RETURN o
END FUNCTION

' RUNFLAT(flat, periodi, asset) -> stesso risultato di RUN, buffer piatto row-major.
PUBLIC FUNCTION RUNFLAT(flat() AS DOUBLE, periodi AS LONG, asset AS LONG) AS ARRAY OF DOUBLE
    DIM o(5 + periodi) AS DOUBLE
    DIM k AS LONG
    k = BTN_RUN(flat, asset, o)
    RETURN o
END FUNCTION
