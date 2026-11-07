' fundamentals.inc — namespace FUNDAMENTALS: metriche fondamentali (FFI, libgmbquant).
'
' Erano 11 funzioni di TRADING.METRICS:
'
'   BookValuePerShare(equity, azioni)          equity/azioni
'   DilutedEPS(netIncome, azioni, strumenti)   ni/(azioni+strumenti)
'   DividendPayoutRatio(dividendi, netIncome)  div/ni
'   DividendYield(dividendi, prezzo)           div/prezzo*100  (PERCENTUALE, canone StdLib)
'   DPS(dividendi, azioni)                     div/azioni
'   EPS(netIncome, azioni)                     ni/azioni
'   EquityMultiplier(attivo, equity)           attivo/equity
'   NAV(attivo, passivo, azioni)               (attivo-passivo)/azioni
'   PBRatio(prezzo, bookValue)                 P/B
'   PERatio(prezzo, eps)                       P/E
'   PSRatio(prezzo, salesPerShare)             P/S
'
' Input non valido -> 0.
NAMESPACE FUNDAMENTALS

DECLARE FUNCTION BookValuePerShare LIB "gmbquant" ALIAS "ofx_q_bvps" (totalEquity AS DOUBLE, commonShares AS DOUBLE) AS DOUBLE
DECLARE FUNCTION DilutedEPS LIB "gmbquant" ALIAS "ofx_q_diluted_eps" (netIncome AS DOUBLE, avgShares AS DOUBLE, otherInstruments AS DOUBLE) AS DOUBLE
DECLARE FUNCTION DividendPayoutRatio LIB "gmbquant" ALIAS "ofx_q_payout_ratio" (dividends AS DOUBLE, netIncome AS DOUBLE) AS DOUBLE
DECLARE FUNCTION DividendYield LIB "gmbquant" ALIAS "ofx_q_dividend_yield" (dividends AS DOUBLE, price AS DOUBLE) AS DOUBLE
DECLARE FUNCTION DPS LIB "gmbquant" ALIAS "ofx_q_dps" (dividends AS DOUBLE, numShares AS DOUBLE) AS DOUBLE
DECLARE FUNCTION EPS LIB "gmbquant" ALIAS "ofx_q_eps" (netIncome AS DOUBLE, avgShares AS DOUBLE) AS DOUBLE
DECLARE FUNCTION EquityMultiplier LIB "gmbquant" ALIAS "ofx_q_equity_multiplier" (totalAssets AS DOUBLE, equity AS DOUBLE) AS DOUBLE
DECLARE FUNCTION NAV LIB "gmbquant" ALIAS "ofx_q_nav" (fundAssets AS DOUBLE, fundLiabilities AS DOUBLE, outstandingShares AS DOUBLE) AS DOUBLE
DECLARE FUNCTION PBRatio LIB "gmbquant" ALIAS "ofx_q_pb_ratio" (price AS DOUBLE, bookValue AS DOUBLE) AS DOUBLE
DECLARE FUNCTION PERatio LIB "gmbquant" ALIAS "ofx_q_pe_ratio" (price AS DOUBLE, eps AS DOUBLE) AS DOUBLE
DECLARE FUNCTION PSRatio LIB "gmbquant" ALIAS "ofx_q_ps_ratio" (price AS DOUBLE, salesPerShare AS DOUBLE) AS DOUBLE
