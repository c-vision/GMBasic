' FILE: Finance/InvestimentoFurgone.bas
' SCENARIO DIDATTICO — conviene comprare un furgone per le consegne?
'
' La pasticceria valuta un furgone da 45.000 EUR, che durerà 5 anni e a fine vita
' varrà 5.000 EUR. Le consegne proprie fanno risparmiare 12.000 EUR l'anno di
' corriere. Il programma risponde alle domande che si fa un imprenditore:
'   1) come si svaluta il mezzo anno per anno (SLN/SYD/DB/DDB)?
'   2) il progetto è conveniente al costo del denaro dell'8%? (NPV)
'   3) che rendimento annuo implicito ha? (IRR/MIRR) e con date reali? (XNPV/XIRR)
'   4) in quanti anni rientra l'investimento? (payback)
'
' Domini: FINANCE + DATETIME + MATH. Input fissi: output riproducibile.
IMPORT FINANCE
IMPORT DATETIME

SUB Main()
    DIM costo AS DOUBLE
    DIM valoreResiduo AS DOUBLE
    DIM anni AS LONG
    DIM risparmio AS DOUBLE
    costo = 45000.0
    valoreResiduo = 5000.0
    anni = 5
    risparmio = 12000.0

    ' ── 1. Ammortamento: quattro modi di ripartire il costo ──────────────────
    PRINT "== 1. Ammortamento (quota annua) =="
    PRINT "anno   SLN        SYD        DB         DDB"
    DIM a AS LONG
    FOR a = 1 TO anni
        PRINT a; "      "; _
              MATH.ROUND(FINANCE.SLN(costo, valoreResiduo, anni), 2); "   "; _
              MATH.ROUND(FINANCE.SYD(costo, valoreResiduo, anni, a), 2); "   "; _
              MATH.ROUND(FINANCE.DB(costo, valoreResiduo, anni, a, 12), 2); "    "; _
              MATH.ROUND(FINANCE.DDB(costo, valoreResiduo, anni, a, 2.0), 2)
    NEXT a
    PRINT "SLN ripartisce in parti uguali, SYD e DDB anticipano la svalutazione."

    ' ── 2. Flussi di cassa e valore attuale netto ────────────────────────────
    ' Anno 0: esce il costo. Anni 1..5: entra il risparmio; a fine anno 5 entra
    ' anche il valore residuo del mezzo.
    DIM flussi(6) AS DOUBLE
    flussi(0) = 0.0 - costo
    FOR a = 1 TO 5
        flussi(a) = risparmio
    NEXT a
    flussi(5) = flussi(5) + valoreResiduo

    PRINT "== 2. Valore attuale netto (costo del denaro 8%) =="
    ' NPV sconta i flussi degli anni 1..5; l'investimento iniziale si sottrae a parte.
    DIM flussiPositivi(4) AS DOUBLE
    FOR a = 0 TO 4
        flussiPositivi(a) = flussi(a + 1)
    NEXT a
    DIM attuale AS DOUBLE
    attuale = FINANCE.NPV(0.08, flussiPositivi) - costo
    PRINT "VAN: "; MATH.ROUND(attuale, 2); " EUR"
    IF attuale > 0 THEN
        PRINT "esito: conviene investire (VAN positivo)"
    ELSE
        PRINT "esito: non conviene (VAN negativo)"
    END IF

    ' ── 3. Rendimento implicito ──────────────────────────────────────────────
    PRINT "== 3. Rendimento =="
    ' IRR cerca il tasso che azzera il VAN del flusso completo (anno 0 negativo).
    DIM rendimento AS DOUBLE
    rendimento = FINANCE.IRR(flussi, 0.1)
    PRINT "IRR:  "; MATH.ROUND(rendimento * 100.0, 2); "% annuo"
    ' MIRR distingue il costo del finanziamento (6%) dal reinvestimento (9%).
    DIM mirr AS DOUBLE
    mirr = FINANCE.MIRR(flussi, 0.06, 0.09)
    PRINT "MIRR: "; MATH.ROUND(mirr * 100.0, 2); "% annuo"

    ' Con le DATE reali (XIRR): l'investimento parte oggi e ogni anno incassa.
    DIM dateFlussi(6) AS DATETIME
    dateFlussi(0) = DATETIME.CTOD("2026-01-15")
    dateFlussi(1) = DATETIME.CTOD("2027-01-15")
    dateFlussi(2) = DATETIME.CTOD("2028-01-15")
    dateFlussi(3) = DATETIME.CTOD("2029-01-15")
    dateFlussi(4) = DATETIME.CTOD("2030-01-15")
    dateFlussi(5) = DATETIME.CTOD("2031-01-15")
    DIM xr AS DOUBLE
    xr = FINANCE.XIRR(flussi, dateFlussi, 0.1)
    PRINT "XIRR (date reali): "; MATH.ROUND(xr * 100.0, 2); "% annuo"
    PRINT "XNPV @8%:          "; MATH.ROUND(FINANCE.XNPV(0.08, flussi, dateFlussi), 2); " EUR"
    PRINT "giorni 2026-01-15 -> 2031-01-15 (30/360): "; _
          DAYS360(DATETIME.CTOD("2026-01-15"), DATETIME.CTOD("2031-01-15"))

    ' ── 4. Tempo di rientro (payback) ────────────────────────────────────────
    PRINT "== 4. Payback =="
    DIM cumulato AS DOUBLE
    DIM annoRientro AS LONG
    cumulato = flussi(0)
    annoRientro = 0
    FOR a = 1 TO 5
        cumulato = cumulato + flussi(a)
        IF cumulato >= 0 AND annoRientro = 0 THEN annoRientro = a
    NEXT a
    PRINT "l'investimento rientra nell'anno "; annoRientro
    PRINT "saldo cumulato a fine periodo: "; MATH.ROUND(cumulato, 2); " EUR"
    PRINT "guadagno netto su 5 anni:      "; MATH.ROUND(cumulato, 2); " EUR"
END SUB
