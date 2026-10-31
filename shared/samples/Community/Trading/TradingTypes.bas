' ==============================================================================
' FILE: Trading/TradingTypes.bas
' TIPI TRADING dichiarabili: BACKTESTRESULT, REBALANCESTRATEGY,
' PORTFOLIORESULT, PORTFOLIOCONSTRAINTS, GAPARAMETERS.
' ==============================================================================
' Questi tipi sono i ritorni/dichiarazioni delle funzioni di trading:
' - BACKTESTRESULT: risultato di TRADING.BACKTESTS.RUN
' - REBALANCESTRATEGY: strategia di ribilanciamento del backtest
' - PORTFOLIORESULT: risultato di TRADING.PORTFOLIO.OPTIMIZE
' - PORTFOLIOCONSTRAINTS: vincoli dell'ottimizzazione
' - GAPARAMETERS: parametri dell'algoritmo genetico
' ==============================================================================
IMPORT TRADING.BACKTESTS
IMPORT TRADING.PORTFOLIO

PRINT "=== Tipi TRADING dichiarabili ==="

DIM B AS BACKTESTRESULT
PRINT "BACKTESTRESULT: DIM OK"

DIM S AS REBALANCESTRATEGY
PRINT "REBALANCESTRATEGY: DIM OK"

DIM P AS PORTFOLIORESULT
PRINT "PORTFOLIORESULT: DIM OK"

DIM C AS PORTFOLIOCONSTRAINTS
PRINT "PORTFOLIOCONSTRAINTS: DIM OK"

DIM G AS GAPARAMETERS
PRINT "GAPARAMETERS: DIM OK"

PRINT
PRINT "Completato."
