' FILE: Trading/TradingBacktest.bas
' =================================================================================================
'       TEST UNITARIO: BACKTEST - SIMULAZIONE STORICA
' =================================================================================================
' Il backtest FFI lavora a pesi uguali: il rendimento di ogni periodo e' la media degli
' asset della riga. RUN ritorna un ARRAY:
'   [TotalReturn%, AnnualizedReturn%, MaxDrawdown, SharpeRatio, TradesCount, Turnover,
'    curva dei rendimenti per periodo...]
'
' La vecchia RebalanceStrategy (UDT) non esiste piu': la strategia non cambia il risultato.
' =================================================================================================

IMPORT BACKTEST AS BT
IMPORT MATH

SUB Main()

    CLS
    PRINT "============================================================"
    PRINT "BACKTEST - Simulazione Strategia"
    PRINT "============================================================"
    PRINT

    ' --- 1. GENERAZIONE DATI STORICI (SINTETICI, DETERMINISTICI) ---
    ' 60 periodi, 2 asset: serie cicliche (output riproducibile).
    DIM N_Asset = 2
    DIM N_Periodi = 60

    DIM Returns(59) AS ARRAY OF DOUBLE

    FOR i = 0 TO 59
        DIM retA = (0.06 * SIN(i / 5.0)) - 0.005
        DIM retB = (0.02 * SIN(i / 7.0)) + 0.002
        Returns(i) = {retA, retB}
    NEXT i

    PRINT "Dati storici generati: "; N_Periodi; " periodi, "; N_Asset; " asset."
    PRINT

    ' --- 2. ESECUZIONE BACKTEST ---
    PRINT "--- Esecuzione Backtest (RUN) ---"

    DIM Result() AS DOUBLE
    Result = BT.RUN(Returns, "EQUALWEIGHT")

    PRINT "Backtest completato."
    PRINT

    ' --- 3. ANALISI RISULTATI ---
    PRINT "--- Analisi Performance ---"

    PRINT "Total Return:      "; FORMAT(Result(0), "0.00"); " %"
    PRINT "Annualized Return: "; FORMAT(Result(1), "0.00"); " %"
    PRINT "Max Drawdown:      "; FORMAT(Result(2), "0.0000")
    PRINT "Sharpe Ratio:      "; FORMAT(Result(3), "0.00")
    PRINT "Trades Count:      "; Result(4)
    PRINT "Turnover:          "; FORMAT(Result(5), "0.00")
    PRINT "Curva (primi 3):   "; FORMAT(Result(6), "0.0000"); " "; FORMAT(Result(7), "0.0000"); " "; FORMAT(Result(8), "0.0000")

    PRINT
    PRINT "Test Completato."

END SUB
