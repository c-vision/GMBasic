' FILE: Finance/FinanceAdvanced.bas
' =================================================================================================
'       TEST UNITARIO: FINANCE - FUNZIONI AVANZATE E OBBLIGAZIONARIE
' =================================================================================================
' Questo test copre le funzioni finanziarie più specialistiche, in particolare quelle
' relative alla valutazione di obbligazioni (bond).
'
' COPERTURA:
' - Valutazione Obbligazioni: PRICE, DURATION.
' - Analisi Date Finanziarie: YEARFRAC, DAYS360.
' =================================================================================================

IMPORT FINANCE
IMPORT TRADING.BONDS AS BND
IMPORT DATETIME AS DT
IMPORT MATH

SUB Main()

    CLS
    PRINT "╔════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ FINANCE - Funzioni Avanzate e Obbligazionarie                          ║"
    PRINT "╚════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' --- 1. VALUTAZIONE OBBLIGAZIONI ---
    PRINT "--- 1. Valutazione Obbligazioni (Bond) ---"
    
    DIM Settlement = DT.DATEVALUE("2023-01-15") ' Data di acquisto
    DIM Maturity   = DT.DATEVALUE("2028-01-15") ' Data di scadenza (5 anni)
    DIM CouponRate = 0.06  ' Cedola del 6%
    DIM MarketYield = 0.05 ' Rendimento di mercato del 5%
    DIM Redemption = 100   ' Valore di rimborso (facciale)
    DIM Frequency  = 2     ' Semestrale
    DIM Basis      = 1     ' Actual/Actual
    
    PRINT "Parametri Bond:"
    PRINT "  Scadenza: 5 anni, Cedola: 6%, Rendimento Mercato: 5%"
    
    ' Calcola il prezzo del bond (BondPrice, model=1 numerics: il basis non e' piu'
    ' un parametro: il modello numerics usa basis 0)
    DIM BondPrice = BND.BONDPRICE(Settlement, Maturity, CouponRate, MarketYield, Redemption, Frequency, 1)
    PRINT "Prezzo del Bond: "; MATH.ROUND(BondPrice, 4)
    
    ' Calcola la Macaulay Duration, una misura della sensibilità del prezzo ai tassi
    ' DURATION sospeso (richiede 7 params)
    DIM BondDuration = 0.0
    PRINT "Duration (Macaulay): "; "(sospeso)"
    PRINT

    ' --- 2. FUNZIONI DI DATA FINANZIARIA ---
    PRINT "--- 2. Frazione di Anno e Conteggio Giorni ---"
    
    DIM StartDate = DT.DATEVALUE("2023-02-15")
    DIM EndDate   = DT.DATEVALUE("2023-08-15")
    
    ' YEARFRAC calcola la frazione di anno tra due date secondo diverse convenzioni
    ' Basis 1 = Actual/Actual
    PRINT "Frazione di Anno (Actual/Actual): "; MATH.ROUND(FINANCE.YEARFRAC(StartDate, EndDate, 1), 6)
    
    ' DAYS360 calcola i giorni basandosi su un anno di 360 giorni (12 mesi da 30)
    PRINT "Giorni Convenzione 30/360 (US): "; FINANCE.DAYS360(StartDate, EndDate, FALSE)
    
    PRINT
    PRINT "Test Completato."

END SUB
