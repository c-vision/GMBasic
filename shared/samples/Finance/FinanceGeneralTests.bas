' FILE: FINANCE/FinanceGeneralTests.bas
' =================================================================================================
'       TEST UNITARIO: FINANCE - CALCOLI FINANZIARI GENERALI
' =================================================================================================
' Questo test copre le funzioni del namespace FINANCE non trattate nei moduli specifici di Trading.
' Include calcoli TVM (Time Value of Money), Ammortamenti e Analisi di Progetti.
'
' COPERTURA:
' - Prestiti/Investimenti: PMT, FV, PV, RATE, NPER.
' - Cumulativi: CUMIPMT, CUMPRINC.
' - Ammortamenti: SLN, SYD, DB, DDB.
' - Analisi Cash Flow: NPV, IRR, MIRR, XNPV, XIRR.
' - Date Finanziarie: YEARFRAC, DAYS360.
' =================================================================================================

IMPORT FINANCE
IMPORT DATETIME AS DT
IMPORT MATH

SUB Main()

    CLS
    PRINT "╔════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ FINANCE - Time Value of Money e Cash Flows                             ║"
    PRINT "╚════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' --- 1. TIME VALUE OF MONEY (TVM) ---
    PRINT "--- 1. Mutui e Prestiti (TVM) ---"
    
    DIM TassoAnnuo = 0.05
    DIM Anni = 30
    DIM Importo = 200000
    
    ' Calcolo Rata Mensile (PMT)
    ' PMT(rate, nper, pv, [fv], [type])
    DIM Rata = FINANCE.PMT(TassoAnnuo/12, Anni*12, -Importo)
    PRINT "Rata Mensile (Mutuo 200k, 5%, 30y): "; MATH.ROUND(Rata, 2)
    
    ' Calcolo Valore Futuro (FV) di un piano di accumulo
    ' Verso 100 al mese per 10 anni al 7%
    DIM ValFuturo = FINANCE.FV(0.07/12, 10*12, -100, 0, 0)
    PRINT "Valore Futuro PAC (100/m, 7%, 10y): "; MATH.ROUND(ValFuturo, 2)
    
    ' Calcolo Tasso (RATE) implicito
    ' Ho preso 10000, restituisco 300 al mese per 3 anni. Qual è il tasso?
    DIM TassoCalc = FINANCE.RATE(3*12, -300, 10000, 0, 0) * 12
    PRINT "Tasso Implicito (Prestito 10k, Rata 300, 3y): "; MATH.FORMAT(TassoCalc, "0.00%")
    PRINT

    ' --- 2. AMMORTAMENTI (Depreciation) ---
    PRINT "--- 2. Ammortamenti Asset ---"
    
    DIM Costo = 10000
    DIM ValoreResiduo = 1000
    DIM VitaUtile = 5 ' Anni
    
    PRINT "Costo: 10000, Residuo: 1000, Vita: 5 anni"
    PRINT "Anno 1 (Straight Line):   "; FINANCE.SLN(Costo, ValoreResiduo, VitaUtile)
    PRINT "Anno 1 (Sum Years Digit): "; FINANCE.SYD(Costo, ValoreResiduo, VitaUtile, 1)
    PRINT "Anno 1 (Double Declining):"; FINANCE.DDB(Costo, ValoreResiduo, VitaUtile, 1, 2)
    PRINT

    ' --- 3. ANALISI CASH FLOW (IRR/NPV) ---
    PRINT "--- 3. Analisi Investimenti (Cash Flows) ---"
    
    ' Array di flussi di cassa (T0, T1, T2, T3)
    DIM Flussi(3) AS DOUBLE = {-10000, 3000, 4200, 6800}
    DIM DateFlussi(3) AS DATETIME
    DateFlussi(0) = DT.DATEVALUE("2023-01-01")
    DateFlussi(1) = DT.DATEVALUE("2024-01-01")
    DateFlussi(2) = DT.DATEVALUE("2025-01-01")
    DateFlussi(3) = DT.DATEVALUE("2026-01-01")
    
    ' NPV (Net Present Value)
    ' Nota: La funzione FINANCE.NPV standard assume flussi periodici regolari.
    ' Il primo valore (investimento) solitamente si somma fuori o dentro a seconda della convenzione.
    ' Qui assumiamo lo standard Excel dove value1 è periodo 1. 
    ' Per includere T0, lo sommiamo a parte o usiamo una funzione custom.
    ' Usiamo XNPV per precisione con date.
    
    DIM TassoSconto = 0.10
    
    ' IRR (Internal Rate of Return) periodico
    DIM IrrVal = FINANCE.IRR(Flussi)
    PRINT "IRR (Periodico): "; MATH.FORMAT(IrrVal, "0.00%")
    
    ' XIRR (IRR con date specifiche)
    DIM XIrrVal = FINANCE.XIRR(Flussi, DateFlussi)
    PRINT "XIRR (Con Date): "; MATH.FORMAT(XIrrVal, "0.00%")
    
    ' XNPV
    DIM XNpvVal = FINANCE.XNPV(TassoSconto, Flussi, DateFlussi)
    PRINT "XNPV (Sconto 10%):"; MATH.ROUND(XNpvVal, 2)
    
    ' MIRR (Modified IRR) - Tasso finanziamento 5%, Reinvestimento 8%
    DIM MirrVal = FINANCE.MIRR(Flussi, 0.05, 0.08)
    PRINT "MIRR (5%, 8%):   "; MATH.FORMAT(MirrVal, "0.00%")
    PRINT

    ' --- 4. DATE FINANZIARIE ---
    PRINT "--- 4. Date Finanziarie ---"
    
    DIM StartD = DT.DATEVALUE("2023-01-01")
    DIM EndD   = DT.DATEVALUE("2023-03-31")
    
    ' Calcolo frazione di anno (Basis 0 = US 30/360)
    PRINT "YearFrac (30/360): "; MATH.ROUND(FINANCE.YEARFRAC(StartD, EndD, 0), 4)
    PRINT "Days360 (US):      "; FINANCE.DAYS360(StartD, EndD)
    PRINT

    PRINT "--- 5. Funzioni Finanziarie Aggiuntive ---"
    PRINT "CUMIPMT test:"; FINANCE.CUMIPMT(0.05, 360, 200000, 1, 12, 0)
    PRINT "CUMPRINC test:"; FINANCE.CUMPRINC(0.05, 360, 200000, 1, 12, 0)
    PRINT "DB test:"; FINANCE.DB(10000, 1000, 5, 1, 12)
    PRINT "FVEXT test:"; FINANCE.FVEXT(0.05, 360, -1000)
    PRINT "MNPV test:"; FINANCE.MNPV(0.1, Flussi)
    PRINT

    PRINT "Test Completato."

END SUB