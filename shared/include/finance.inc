' finance.inc — namespace FINANCE via FFI (lib `finance`, libgmbfinance).
'
' 23 funzioni finanziarie pure: rendite (PMT/FV/PV/NPER/RATE e varianti EXT),
' ammortamenti (SLN/SYD/DB/DDB), flussi di cassa (NPV/MNPV/IRR/MIRR/XIRR/XNPV),
' ratei (CUMIPMT/CUMPRINC) e date (DAYS360/YEARFRAC).
'
' Date: DATETIME (il wrapper le converte in OADate, il seriale della lib).
' Le firme replicano la superficie storica, inclusi i parametri opzionali.
' Uso: IMPORT FINANCE   (shared/libs: nessun REQUIRE; export in shared/include)

NAMESPACE FINANCE

DECLARE NATIVE FUNCTION FINN_PMT LIB "gmbfinance" ALIAS "ofx_fin_pmt" (rate AS DOUBLE, nper AS DOUBLE, pv AS DOUBLE, fv AS DOUBLE, type AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION FINN_FV LIB "gmbfinance" ALIAS "ofx_fin_fv" (rate AS DOUBLE, nper AS DOUBLE, pmt AS DOUBLE, pv AS DOUBLE, type AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION FINN_FVEXT LIB "gmbfinance" ALIAS "ofx_fin_fvext" (rate AS DOUBLE, nper AS DOUBLE, pmt AS DOUBLE, pv AS DOUBLE, type AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION FINN_PVEXT LIB "gmbfinance" ALIAS "ofx_fin_pvext" (rate AS DOUBLE, nper AS DOUBLE, pmt AS DOUBLE, fv AS DOUBLE, type AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION FINN_NPEREXT LIB "gmbfinance" ALIAS "ofx_fin_nper_ext" (rate AS DOUBLE, pmt AS DOUBLE, pv AS DOUBLE, fv AS DOUBLE, type AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION FINN_RATE LIB "gmbfinance" ALIAS "ofx_fin_rate" (nper AS DOUBLE, pmt AS DOUBLE, pv AS DOUBLE, fv AS DOUBLE, type AS LONG, guess AS DOUBLE) AS DOUBLE
DECLARE NATIVE FUNCTION FINN_CUMIPMT LIB "gmbfinance" ALIAS "ofx_fin_cumipmt" (rate AS DOUBLE, nper AS DOUBLE, pv AS DOUBLE, startPeriod AS LONG, endPeriod AS LONG, type AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION FINN_CUMPRINC LIB "gmbfinance" ALIAS "ofx_fin_cumprinc" (rate AS DOUBLE, nper AS DOUBLE, pv AS DOUBLE, startPeriod AS LONG, endPeriod AS LONG, type AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION FINN_SLN LIB "gmbfinance" ALIAS "ofx_fin_sln" (cost AS DOUBLE, salvage AS DOUBLE, life AS DOUBLE) AS DOUBLE
DECLARE NATIVE FUNCTION FINN_SYD LIB "gmbfinance" ALIAS "ofx_fin_syd" (cost AS DOUBLE, salvage AS DOUBLE, life AS DOUBLE, period AS DOUBLE) AS DOUBLE
DECLARE NATIVE FUNCTION FINN_DB LIB "gmbfinance" ALIAS "ofx_fin_db" (cost AS DOUBLE, salvage AS DOUBLE, life AS DOUBLE, period AS DOUBLE, month AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION FINN_DDB LIB "gmbfinance" ALIAS "ofx_fin_ddb" (cost AS DOUBLE, salvage AS DOUBLE, life AS DOUBLE, period AS DOUBLE, factor AS DOUBLE) AS DOUBLE
DECLARE NATIVE FUNCTION FINN_NPV LIB "gmbfinance" ALIAS "ofx_fin_npv" (rate AS DOUBLE, cf() AS DOUBLE, n AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION FINN_MNPV LIB "gmbfinance" ALIAS "ofx_fin_mnpv" (rate AS DOUBLE, cf() AS DOUBLE, n AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION FINN_IRR LIB "gmbfinance" ALIAS "ofx_fin_irr" (cf() AS DOUBLE, n AS LONG, guess AS DOUBLE) AS DOUBLE
DECLARE NATIVE FUNCTION FINN_MIRR LIB "gmbfinance" ALIAS "ofx_fin_mirr" (cf() AS DOUBLE, n AS LONG, fr AS DOUBLE, rr AS DOUBLE) AS DOUBLE
DECLARE NATIVE FUNCTION FINN_XIRR LIB "gmbfinance" ALIAS "ofx_fin_xirr" (cf() AS DOUBLE, dates() AS DOUBLE, n AS LONG, guess AS DOUBLE) AS DOUBLE
DECLARE NATIVE FUNCTION FINN_XNPV LIB "gmbfinance" ALIAS "ofx_fin_xnpv" (rate AS DOUBLE, cf() AS DOUBLE, dates() AS DOUBLE, n AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION FINN_DAYS360 LIB "gmbfinance" ALIAS "ofx_fin_days360" (oaStart AS DOUBLE, oaEnd AS DOUBLE, basis AS LONG) AS DOUBLE
DECLARE NATIVE FUNCTION FINN_YEARFRAC LIB "gmbfinance" ALIAS "ofx_fin_yearfrac" (oaStart AS DOUBLE, oaEnd AS DOUBLE, basis AS LONG) AS DOUBLE

' PMT(rate, nper, pv[, fv, type]) — rata di un prestito/rendita.
PUBLIC FUNCTION PMT(rate AS DOUBLE, nper AS DOUBLE, pv AS DOUBLE, OPTIONAL fv AS DOUBLE = 0, OPTIONAL type AS LONG = 0) AS DOUBLE
    RETURN FINN_PMT(rate, nper, pv, fv, type)
END FUNCTION
PUBLIC FUNCTION PMTEXT(rate AS DOUBLE, nper AS DOUBLE, pv AS DOUBLE, OPTIONAL fv AS DOUBLE = 0, OPTIONAL type AS LONG = 0) AS DOUBLE
    RETURN FINN_PMT(rate, nper, pv, fv, type)
END FUNCTION

' FV(rate, nper, pmt[, pv, type]) — valore futuro di una rendita.
PUBLIC FUNCTION FV(rate AS DOUBLE, nper AS DOUBLE, pmt AS DOUBLE, OPTIONAL pv AS DOUBLE = 0, OPTIONAL type AS LONG = 0) AS DOUBLE
    RETURN FINN_FV(rate, nper, pmt, pv, type)
END FUNCTION
PUBLIC FUNCTION FVEXT(rate AS DOUBLE, nper AS DOUBLE, pmt AS DOUBLE, OPTIONAL pv AS DOUBLE = 0, OPTIONAL type AS LONG = 0) AS DOUBLE
    RETURN FINN_FVEXT(rate, nper, pmt, pv, type)
END FUNCTION

' PV(rate, nper, pmt[, fv, type]) — valore attuale.
PUBLIC FUNCTION PV(rate AS DOUBLE, nper AS DOUBLE, pmt AS DOUBLE, OPTIONAL fv AS DOUBLE = 0, OPTIONAL type AS LONG = 0) AS DOUBLE
    RETURN FINN_PVEXT(rate, nper, pmt, fv, type)
END FUNCTION
PUBLIC FUNCTION PVEXT(rate AS DOUBLE, nper AS DOUBLE, pmt AS DOUBLE, OPTIONAL fv AS DOUBLE = 0, OPTIONAL type AS LONG = 0) AS DOUBLE
    RETURN FINN_PVEXT(rate, nper, pmt, fv, type)
END FUNCTION

' NPER(rate, pmt, pv[, fv, type]) — numero di periodi.
PUBLIC FUNCTION NPER(rate AS DOUBLE, pmt AS DOUBLE, pv AS DOUBLE, OPTIONAL fv AS DOUBLE = 0, OPTIONAL type AS LONG = 0) AS DOUBLE
    RETURN FINN_NPEREXT(rate, pmt, pv, fv, type)
END FUNCTION
PUBLIC FUNCTION NPEREXT(rate AS DOUBLE, pmt AS DOUBLE, pv AS DOUBLE, OPTIONAL fv AS DOUBLE = 0, OPTIONAL type AS LONG = 0) AS DOUBLE
    RETURN FINN_NPEREXT(rate, pmt, pv, fv, type)
END FUNCTION

' RATE(nper, pmt, pv[, fv, type, guess]) — tasso per periodo.
PUBLIC FUNCTION RATE(nper AS DOUBLE, pmt AS DOUBLE, pv AS DOUBLE, OPTIONAL fv AS DOUBLE = 0, OPTIONAL type AS LONG = 0, OPTIONAL guess AS DOUBLE = 0.1) AS DOUBLE
    RETURN FINN_RATE(nper, pmt, pv, fv, type, guess)
END FUNCTION

' CUMIPMT/CUMPRINC — interessi/quota capitale cumulati tra due periodi.
PUBLIC FUNCTION CUMIPMT(rate AS DOUBLE, nper AS DOUBLE, pv AS DOUBLE, startPeriod AS LONG, endPeriod AS LONG, type AS LONG) AS DOUBLE
    RETURN FINN_CUMIPMT(rate, nper, pv, startPeriod, endPeriod, type)
END FUNCTION
PUBLIC FUNCTION CUMPRINC(rate AS DOUBLE, nper AS DOUBLE, pv AS DOUBLE, startPeriod AS LONG, endPeriod AS LONG, type AS LONG) AS DOUBLE
    RETURN FINN_CUMPRINC(rate, nper, pv, startPeriod, endPeriod, type)
END FUNCTION

' Ammortamenti.
PUBLIC FUNCTION SLN(cost AS DOUBLE, salvage AS DOUBLE, life AS DOUBLE) AS DOUBLE
    RETURN FINN_SLN(cost, salvage, life)
END FUNCTION
PUBLIC FUNCTION SYD(cost AS DOUBLE, salvage AS DOUBLE, life AS DOUBLE, period AS DOUBLE) AS DOUBLE
    RETURN FINN_SYD(cost, salvage, life, period)
END FUNCTION
PUBLIC FUNCTION DB(cost AS DOUBLE, salvage AS DOUBLE, life AS DOUBLE, period AS DOUBLE, OPTIONAL month AS LONG = 12) AS DOUBLE
    RETURN FINN_DB(cost, salvage, life, period, month)
END FUNCTION
PUBLIC FUNCTION DDB(cost AS DOUBLE, salvage AS DOUBLE, life AS DOUBLE, period AS DOUBLE, OPTIONAL factor AS DOUBLE = 2) AS DOUBLE
    RETURN FINN_DDB(cost, salvage, life, period, factor)
END FUNCTION

' NPV(rate, values...) — valori attuali netti (parametri variabili o array).
PUBLIC FUNCTION NPV(rate AS DOUBLE, PARAMARRAY values() AS DOUBLE) AS DOUBLE
    RETURN FINN_NPV(rate, values, LEN(values))
END FUNCTION
PUBLIC FUNCTION MNPV(rate AS DOUBLE, cashFlows() AS DOUBLE) AS DOUBLE
    RETURN FINN_MNPV(rate, cashFlows, LEN(cashFlows))
END FUNCTION

' IRR(values[, guess]) — tasso interno di rendimento.
PUBLIC FUNCTION IRR(values() AS DOUBLE, OPTIONAL guess AS DOUBLE = 0.1) AS DOUBLE
    RETURN FINN_IRR(values, LEN(values), guess)
END FUNCTION

' MIRR(values, financeRate, reinvestRate).
PUBLIC FUNCTION MIRR(values() AS DOUBLE, financeRate AS DOUBLE, reinvestRate AS DOUBLE) AS DOUBLE
    RETURN FINN_MIRR(values, LEN(values), financeRate, reinvestRate)
END FUNCTION

' XIRR(values, dates[, guess]) — IRR con date reali (DATETIME).
' Helper: DATETIME → OADate (seriale DOUBLE). La lettura di un elemento di array
' DATETIME non è convertibile con una conversione implicita su IL: si passa da uno
' scalare DATETIME (conversione supportata su tutti i backend).
PUBLIC FUNCTION FINN_OA(d AS DATETIME) AS DOUBLE
    DIM x AS DOUBLE
    x = d
    RETURN x
END FUNCTION

PUBLIC FUNCTION XIRR(values() AS DOUBLE, dates() AS DATETIME, OPTIONAL guess AS DOUBLE = 0.1) AS DOUBLE
    DIM n AS LONG
    n = LEN(dates)
    DIM oa(n - 1) AS DOUBLE
    DIM i AS LONG
    FOR i = 0 TO n - 1
        oa(i) = FINN_OA(dates(i))
    NEXT i
    RETURN FINN_XIRR(values, oa, n, guess)
END FUNCTION

' XNPV(rate, values, dates) — NPV con date reali (DATETIME).
PUBLIC FUNCTION XNPV(rate AS DOUBLE, values() AS DOUBLE, dates() AS DATETIME) AS DOUBLE
    DIM n AS LONG
    n = LEN(dates)
    DIM oa(n - 1) AS DOUBLE
    DIM i AS LONG
    FOR i = 0 TO n - 1
        oa(i) = FINN_OA(dates(i))
    NEXT i
    RETURN FINN_XNPV(rate, values, oa, n)
END FUNCTION

' DAYS360(start, end[, european]) — giorni tra date su base 30/360.
PUBLIC FUNCTION DAYS360(start AS DATETIME, end AS DATETIME, OPTIONAL european AS LONG = 0) AS DOUBLE
    DIM s AS DOUBLE, e AS DOUBLE
    s = start : e = end
    RETURN FINN_DAYS360(s, e, european)
END FUNCTION

' YEARFRAC(start, end[, basis]) — frazione d'anno tra due date.
PUBLIC FUNCTION YEARFRAC(start AS DATETIME, end AS DATETIME, OPTIONAL basis AS LONG = 0) AS DOUBLE
    DIM s AS DOUBLE, e AS DOUBLE
    s = start : e = end
    RETURN FINN_YEARFRAC(s, e, basis)
END FUNCTION
