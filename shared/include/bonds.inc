' bonds.inc — namespace BONDS: obbligazioni (FFI, libgmbquant).
'
' Erano TRADING.BONDS (9 funzioni). Le date sono DATETIME (come il vecchio builtin):
' il wrapper le converte in OADate (il seriale delle DateTime, giorni dal 1899-12-30)
' e passa il DOUBLE alla lib. Si puo' quindi scrivere direttamente
' BONDS.BondPrice(DATETIME.DATEVALUE("2024-01-01"), ...).
'
'   BondPrice(settle, mat, rate, yld, redemption, frequency[, model])
'   BondYield(settle, mat, rate, price, redemption, frequency[, model])
'   DURATION(settle, mat, rate, yld, frequency, basis, calcMethod)
'   BondEquivalentYield(face, price, giorni)
'   CurrentYield(cedoleAnnuali, prezzo)
'   YieldToMaturity(cedola, nominale, prezzo, anni)
'   ZeroCouponBondValue(nominale, tasso, tempo)
'   ZeroCouponBondYield(nominale, prezzo, tempo)
'   PRICESIMPLE(settle, mat, rate, yld, redemption, frequency)
'
' model 0 = Trading.Bonds (cedole reali), model 1 = Numerics.Price (basis 0).
' DURATION ignora basis/calcMethod: le quattro gambe del builtin usano sempre la
' duration di Macaulay (parita' conservata).
NAMESPACE BONDS

DECLARE FUNCTION BONDPRICE_NAT LIB "gmbquant" ALIAS "ofx_q_bond_price" (settlement AS DOUBLE, maturity AS DOUBLE, rate AS DOUBLE, yld AS DOUBLE, redemption AS DOUBLE, frequency AS LONG, model AS LONG) AS DOUBLE
PUBLIC FUNCTION BondPrice(settlement AS DATETIME, maturity AS DATETIME, rate AS DOUBLE, yld AS DOUBLE, redemption AS DOUBLE, frequency AS LONG, OPTIONAL model AS LONG = 0) AS DOUBLE
  DIM s AS DOUBLE, m AS DOUBLE
  s = settlement : m = maturity
  RETURN BONDPRICE_NAT(s, m, rate, yld, redemption, frequency, model)
END FUNCTION

DECLARE FUNCTION BONDYIELD_NAT LIB "gmbquant" ALIAS "ofx_q_bond_yield" (settlement AS DOUBLE, maturity AS DOUBLE, rate AS DOUBLE, price AS DOUBLE, redemption AS DOUBLE, frequency AS LONG, model AS LONG) AS DOUBLE
PUBLIC FUNCTION BondYield(settlement AS DATETIME, maturity AS DATETIME, rate AS DOUBLE, price AS DOUBLE, redemption AS DOUBLE, frequency AS LONG, OPTIONAL model AS LONG = 0) AS DOUBLE
  DIM s AS DOUBLE, m AS DOUBLE
  s = settlement : m = maturity
  RETURN BONDYIELD_NAT(s, m, rate, price, redemption, frequency, model)
END FUNCTION

DECLARE FUNCTION DURATION_NAT LIB "gmbquant" ALIAS "ofx_q_duration" (settlement AS DOUBLE, maturity AS DOUBLE, rate AS DOUBLE, yld AS DOUBLE, frequency AS LONG, basis AS LONG, calcMethod AS LONG) AS DOUBLE
PUBLIC FUNCTION DURATION(settlement AS DATETIME, maturity AS DATETIME, rate AS DOUBLE, yld AS DOUBLE, frequency AS LONG, basis AS LONG, calcMethod AS LONG) AS DOUBLE
  DIM s AS DOUBLE, m AS DOUBLE
  s = settlement : m = maturity
  RETURN DURATION_NAT(s, m, rate, yld, frequency, basis, calcMethod)
END FUNCTION

DECLARE FUNCTION BondEquivalentYield LIB "gmbquant" ALIAS "ofx_q_bond_equiv" (faceValue AS DOUBLE, bondPrice AS DOUBLE, daysToMaturity AS DOUBLE) AS DOUBLE
DECLARE FUNCTION CurrentYield LIB "gmbquant" ALIAS "ofx_q_current_yield" (annualCoupons AS DOUBLE, bondPrice AS DOUBLE) AS DOUBLE
DECLARE FUNCTION YieldToMaturity LIB "gmbquant" ALIAS "ofx_q_ytm" (couponPayment AS DOUBLE, faceValue AS DOUBLE, price AS DOUBLE, years AS DOUBLE) AS DOUBLE
DECLARE FUNCTION ZeroCouponBondValue LIB "gmbquant" ALIAS "ofx_q_zcb_value" (faceValue AS DOUBLE, rate AS DOUBLE, time AS DOUBLE) AS DOUBLE
DECLARE FUNCTION ZeroCouponBondYield LIB "gmbquant" ALIAS "ofx_q_zcb_yield" (faceValue AS DOUBLE, price AS DOUBLE, time AS DOUBLE) AS DOUBLE

DECLARE FUNCTION PRICESIMPLE_NAT LIB "gmbquant" ALIAS "ofx_q_price_simple" (settlement AS DOUBLE, maturity AS DOUBLE, rate AS DOUBLE, yld AS DOUBLE, redemption AS DOUBLE, frequency AS LONG) AS DOUBLE
PUBLIC FUNCTION PRICESIMPLE(settlement AS DATETIME, maturity AS DATETIME, rate AS DOUBLE, yld AS DOUBLE, redemption AS DOUBLE, frequency AS LONG) AS DOUBLE
  DIM s AS DOUBLE, m AS DOUBLE
  s = settlement : m = maturity
  RETURN PRICESIMPLE_NAT(s, m, rate, yld, redemption, frequency)
END FUNCTION
