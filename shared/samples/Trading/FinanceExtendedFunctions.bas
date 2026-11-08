IMPORT FINANCE
IMPORT BONDS AS BND
' ==============================================================================
' FILE: Trading/FinanceExtendedFunctions.bas
' Funzioni FINANCE estese (2026-08-06): PMTEXT/PVEXT/NPEREXT/FVEXT/YIELD/PRICESIMPLE
' ==============================================================================
' PMTEXT(rate, nper, pv [, fv [, type]])     — rata del prestito
' FVEXT(rate, nper, pmt [, pv [, type]])     — valore futuro
' PVEXT(rate, nper, pmt [, fv [, type]])     — valore attuale
' NPEREXT(rate, pmt, pv [, fv [, type]])     — numero di periodi
' YIELD(settlement, maturity, price, redemption, rate, frequency [, basis])
' PRICESIMPLE(settlement, maturity, rate, yld, redemption, frequency)
' ==============================================================================

PRINT "=== FINANCE estese ==="
PRINT

' --- PMTEXT: rata mensile per un prestito ---
DIM RATE AS DOUBLE
DIM PMT AS DOUBLE
RATE = 0.005          ' 0.5% mensile
PMT = FINANCE.PMTEXT(RATE, 60, 10000)
PRINT "PMTEXT(0.005, 60, 10000) = " + STR(PMT) + " (atteso ~-198.02)"
PRINT

' --- FVEXT: valore futuro di un investimento ---
DIM FV AS DOUBLE
FV = FINANCE.FVEXT(0.005, 60, -100)
PRINT "FVEXT(0.005, 60, -100) = " + STR(FV) + " (atteso ~7011.88)"
PRINT

' --- PVEXT: valore attuale ---
DIM PV AS DOUBLE
PV = FINANCE.PVEXT(0.005, 60, -100)
PRINT "PVEXT(0.005, 60, -100) = " + STR(PV) + " (atteso ~5172.56)"
PRINT

' --- NPEREXT: periodi per raggiungere un obiettivo ---
DIM N AS DOUBLE
N = FINANCE.NPEREXT(0.005, -100, 0, 10000)
PRINT "NPEREXT(0.005, -100, 0, 10000) = " + STR(N) + " (atteso ~91.9)"
PRINT

' --- YIELD: rendimento di un'obbligazione ---
DIM Y AS DOUBLE
Y = BND.BONDYIELD(CTOD("01/01/2024"), CTOD("01/01/2029"), 0.05, 98, 100, 1, 1)
PRINT "YIELD(...) = " + STR(Y)
PRINT

' --- PRICESIMPLE: prezzo di un'obbligazione ---
DIM P AS DOUBLE
P = BND.PRICESIMPLE(CTOD("01/01/2024"), CTOD("01/01/2025"), 0.05, 0.04, 100, 1)
PRINT "PRICESIMPLE(...) = " + STR(P)

PRINT
PRINT "Completato."
