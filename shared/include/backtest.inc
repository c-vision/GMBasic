' backtest.inc — namespace BACKTEST: backtest a pesi uguali (FFI, libgmbquant).
'
' Era TRADING.BACKTESTS.RUN (1 funzione). Il modello e' quello del runtime condiviso:
' il rendimento di ogni periodo e' la MEDIA degli asset della riga, poi si calcolano
' TotalReturn (percentuale), AnnualizedReturn, MaxDrawdown, Sharpe (252/anno) e la
' curva dei rendimenti.
'
' Layout del risultato (buffer `out` fornito dal chiamante):
'
'     out(0) = TotalReturn (%)        out(1) = AnnualizedReturn (%)
'     out(2) = MaxDrawdown            out(3) = SharpeRatio
'     out(4) = TradesCount            out(5) = Turnover
'     out(6 .. 6+periodi-1) = curva dei rendimenti per periodo
'
' La strategia e il portfolio iniziale del builtin non sono riprodotti: il modello
' e' a pesi uguali (nessun ribilanciamento), come le gambe C del runtime.
'
' INPUT: SIMULATE accetta direttamente una MATRICE 2-D
' `DIM data(periodi, asset) AS DOUBLE` — l'FFI la passa come buffer contiguo
' row-major e le dimensioni arrivano esplicite. Il nome `RUN` non si usa perche' e'
' una parola chiave del linguaggio: su C99/VM-C la chiamata verrebbe instradata sul
' builtin omonimo. Per un buffer piatto 1-D c'e' RUNFLAT.

NAMESPACE BACKTEST

' Matrice 2-D: `data` porta le due estensioni di rango, `out` quella del buffer.
DECLARE FUNCTION SIMULATE LIB "gmbquant" ALIAS "ofx_q_bt_run2" (data() AS DOUBLE, out() AS DOUBLE) AS LONG

DECLARE FUNCTION BTN_RUN LIB "gmbquant" ALIAS "ofx_q_bt_run" (data() AS DOUBLE, cols AS LONG, out() AS DOUBLE) AS LONG

' RUNFLAT(flat, periodi, asset) -> ARRAY [6 metriche + curva] da un buffer piatto row-major.
PUBLIC FUNCTION RUNFLAT(flat() AS DOUBLE, periodi AS LONG, asset AS LONG) AS ARRAY OF DOUBLE
    DIM o(5 + periodi) AS DOUBLE
    DIM k AS LONG
    k = BTN_RUN(flat, asset, o)
    RETURN o
END FUNCTION
