REM FILE: TradingOptionsAndBonds.bas

' FILE: Trading/TradingOptionsAndBonds.bas
' =================================================================================================
' TEST UNITARIO: TRADING.OPTIONS e TRADING.BONDS
' =================================================================================================
' Questo test verifica le funzioni per la valutazione di derivati e titoli a reddito fisso.
'
' COPERTURA OPZIONI (Modello Black-Scholes-Merton):
' - Prezzo: BsCall, BsPut.
' - Greche: Delta, Gamma, Theta, Vega, Rho.
' - Greche Avanzate: Vanna, Volga.
'
' COPERTURA OBBLIGAZIONI:
' - BondPrice: Prezzo "dirty" (incluso rateo).
' - BondYield: Rendimento alla scadenza (YTM).
' - Duration: Macaulay Duration.
' - Convexity: Convessità (se disponibile).
'
' COPERTURA GENERALE TRADING:
' - CAPM: Capital Asset Pricing Model.
' =================================================================================================
IMPORT TRADING.OPTIONS AS OPT
IMPORT TRADING.BONDS AS BND
' CORREZIONE: Cambiato l'alias da GEN a TRD per evitare collisione con la keyword GENERATOR.
IMPORT TRADING AS TRD
IMPORT DATETIME AS DT

SUB Main()
    CLS
    PRINT "╔════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ TRADING - Opzioni, Obbligazioni e Valutazione                          ║"
    PRINT "╚════════════════════════════════════════════════════════════════════════╝"
    PRINT
    
    ' --- 1. OPZIONI (Black-Scholes) ---
    PRINT "--- 1. Valutazione Opzioni (Black-Scholes) ---"
    
    ' Parametri: S=100, K=100, T=1 anno, r=5%, vol=20%, div=0%
    DIM S = 100.0   ' Prezzo Sottostante
    DIM K = 100.0   ' Strike Price
    DIM T = 1.0     ' Tempo a scadenza (anni)
    DIM r = 0.05    ' Tasso risk-free
    DIM v = 0.20    ' Volatilità
    DIM q = 0.0     ' Dividend yield (per Merton model)
    
    DIM CallPrice = OPT.BSCALL(S, K, T, r, v)  ' Standard Black-Scholes (5 args)
    DIM PutPrice  = OPT.BSPUT(S, K, T, r, v)   ' Standard Black-Scholes (5 args)
    DIM MertonCall = OPT.MERTONCALL(S, K, T, r, v, q)  ' Con dividend yield
    
    PRINT "Parametri: S="; S; " K="; K; " T="; T; " r="; r; " v="; v
    PRINT "Prezzo Call: "; MATH.ROUND(CallPrice, 4)
    PRINT "Prezzo Put:  "; MATH.ROUND(PutPrice, 4)
    PRINT
    
    PRINT "--- 2. Greche delle Opzioni ---"
    
    DIM d = OPT.DELTA(S, K, T, r, v, FALSE)   ' FALSE = Call option
    DIM g = OPT.GAMMA(S, K, T, r, v)
    DIM th = OPT.THETA(S, K, T, r, v, FALSE)  ' FALSE = Call option
    DIM ve = OPT.VEGA(S, K, T, r, v)
    DIM rh = OPT.RHO(S, K, T, r, v, FALSE)    ' FALSE = Call option
    
    PRINT "Delta (Call): "; MATH.ROUND(d, 4)
    PRINT "Gamma:        "; MATH.ROUND(g, 4)
    PRINT "Theta (Call): "; MATH.ROUND(th, 4)
    PRINT "Vega:         "; MATH.ROUND(ve, 4)
    PRINT "Rho (Call):   "; MATH.ROUND(rh, 4)
    
    ' Greche Avanzate (senza dividend yield, standard Black-Scholes)
    PRINT "Vanna:        "; MATH.ROUND(OPT.VANNA(S, K, T, r, v), 4)
    PRINT "Volga:        "; MATH.ROUND(OPT.VOLGA(S, K, T, r, v), 4)
    PRINT "Charm (Call): "; MATH.ROUND(OPT.CHARM(S, K, T, r, v, FALSE), 4)
    PRINT "Zomma:        "; MATH.ROUND(OPT.ZOMMA(S, K, T, r, v), 4)
    PRINT "DeltaSpeed:   "; MATH.ROUND(OPT.DELTASPEED(S, K, T, r, v), 4)
    PRINT "GammaColor:   "; MATH.ROUND(OPT.GAMMACOLOR(S, K, T, r, v), 4)
    PRINT

    ' --- Merton e Binomial ---"
    PRINT "MertonPut:    "; MATH.ROUND(OPT.MERTONPUT(S, K, T, r, v, 0), 4)
    PRINT "BinomialOption: "; MATH.ROUND(OPT.BINOMIALOPTION(S, K, T, r, v, 100, "CALL"), 4)
    PRINT
    
    ' --- 3. OBBLIGAZIONI ---
    PRINT "--- 3. Valutazione Obbligazioni ---"
    
    DIM Settlement = DT.DATEVALUE("2023-01-01")
    DIM Maturity   = DT.DATEVALUE("2028-01-01") ' 5 Anni
    DIM Rate       = 0.05  ' Cedola 5%
    DIM Yld        = 0.06  ' Rendimento mercato 6%
    DIM Redemption = 100   ' Valore rimborso
    DIM Frequency  = 1     ' Annuale
    DIM Price = BND.BONDPRICE(Settlement, Maturity, Rate, Yld, Redemption, Frequency)
    PRINT "Prezzo Bond (Yield 6%): "; MATH.ROUND(Price, 4)
    
    ' Calcolo inverso: dato il prezzo, trova lo Yield
    DIM CalcYield = BND.BONDYIELD(Settlement, Maturity, Rate, Price, Redemption, Frequency)
    PRINT "Yield Calcolato:        "; MATH.ROUND(CalcYield, 4); " (Atteso: 0.06)"
    
    ' Duration
    DIM Dur = BND.DURATION(Settlement, Maturity, Rate, Yld, Frequency, 0, 1)
    PRINT "Duration (Macaulay):    "; MATH.ROUND(Dur, 4)
    PRINT "BondEquivalentYield:  "; MATH.ROUND(BND.BONDEQUIVALENTYIELD(100, 95, 180), 4)
    PRINT "CurrentYield:          "; MATH.ROUND(BND.CURRENTYIELD(5, 95), 4)
    PRINT "YieldToMaturity:       "; MATH.ROUND(BND.YIELDTOMATURITY(5, 100, 95, 5), 4)
    PRINT "ZeroCouponBondValue:   "; MATH.ROUND(BND.ZEROCOUPONBONDVALUE(100, 0.05, 5), 4)
    PRINT "ZeroCouponBondYield:   "; MATH.ROUND(BND.ZEROCOUPONBONDYIELD(100, 78, 5), 4)
    PRINT
    
    ' --- 4. MODELLI GENERALI ---
    PRINT "--- 4. Modelli di Valutazione Generali ---"
    
    ' CAPM: RiskFree + Beta * (MarketReturn - RiskFree)
    DIM Rf = 0.03
    DIM Rm = 0.10
    DIM Beta = 1.2
    
    DIM ExpRet = TRD.CAPM(Rf, Beta, Rm)
    PRINT "Rendimento atteso (CAPM): "; MATH.ROUND(ExpRet, 4); " (Beta 1.2)"
    
    ' Gordon Growth Model (Dividend Discount Model)
    DIM D0 = 2.00    ' Dividendo corrente
    DIM g_rate = 0.05 ' Tasso di crescita
    DIM k_req = 0.10  ' Tasso richiesto
    
    ' P0 = D1 / (k - g) = D0 * (1+g) / (k - g)
    DIM StockVal = TRD.STOCKVALUECONSTGROWTH(D0 * (1 + g_rate), k_req, g_rate)
    PRINT "Valore Azione (Gordon):   "; MATH.ROUND(StockVal, 2)
    
    PRINT
    PRINT "Test Completato."

END SUB