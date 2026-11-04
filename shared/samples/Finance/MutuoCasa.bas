' FILE: Finance/MutuoCasa.bas
' SCENARIO DIDATTICO — valutare due offerte di mutuo per la stessa casa.
'
' La famiglia Rossi chiede 180.000 EUR per 25 anni. Due banche propongono tassi
' diversi. Il programma:
'   1) calcola la RATA mensile di entrambe le offerte (PMT);
'   2) scompone il primo anno in quota CAPITALE e quota INTERESSI (CUMPRINC/CUMIPMT);
'   3) verifica il tasso effettivo partendo dalla rata (RATE) — utile quando la banca
'      comunica solo la rata;
'   4) stima in quanto tempo si estingue il debito pagando la rata (NPER) e cosa
'      succede con una rata maggiorata di 100 EUR;
'   5) stima il costo totale degli interessi = rate pagate - capitale.
'
' Domini: FINANCE + MATH. Input fissi: output riproducibile.
IMPORT FINANCE

' Rata mensile: il tasso annuo viene diviso per 12 e la durata è in mesi.
FUNCTION RataMensile(capitale AS DOUBLE, tassoAnnuo AS DOUBLE, anni AS LONG) AS DOUBLE
    DIM mesi AS LONG
    mesi = anni * 12
    RETURN FINANCE.PMT(tassoAnnuo / 12.0, mesi, capitale)
END FUNCTION

SUB Main()
    DIM capitale AS DOUBLE
    capitale = 180000.0

    ' ── 1. Le due offerte ────────────────────────────────────────────────────
    PRINT "== 1. Rata mensile =="
    DIM rataA AS DOUBLE
    DIM rataB AS DOUBLE
    rataA = RataMensile(capitale, 0.032, 25)
    rataB = RataMensile(capitale, 0.027, 25)
    PRINT "banca A (3,2%): "; MATH.ROUND(ABS(rataA), 2); " EUR/mese"
    PRINT "banca B (2,7%): "; MATH.ROUND(ABS(rataB), 2); " EUR/mese"
    PRINT "differenza:     "; MATH.ROUND(ABS(rataA) - ABS(rataB), 2); " EUR/mese"

    ' ── 2. Primo anno dell'offerta B ─────────────────────────────────────────
    ' Le rate pagate sono 12: quanto va in interessi e quanto riduce il debito?
    PRINT "== 2. Primo anno (offerta B) =="
    DIM interessiPrimoAnno AS DOUBLE
    DIM capitalePrimoAnno AS DOUBLE
    interessiPrimoAnno = ABS(FINANCE.CUMIPMT(0.027 / 12.0, 300, capitale, 1, 12, 0))
    capitalePrimoAnno = ABS(FINANCE.CUMPRINC(0.027 / 12.0, 300, capitale, 1, 12, 0))
    PRINT "interessi pagati: "; MATH.ROUND(interessiPrimoAnno, 2); " EUR"
    PRINT "capitale rimborsato: "; MATH.ROUND(capitalePrimoAnno, 2); " EUR"
    PRINT "debito residuo dopo 12 rate: "; MATH.ROUND(capitale - capitalePrimoAnno, 2); " EUR"

    ' ── 3. Che tasso c'è dietro una rata? ────────────────────────────────────
    ' Se la banca comunica solo la rata, RATE ricava il tasso mensile.
    PRINT "== 3. Tasso implicito =="
    DIM tassoMensile AS DOUBLE
    tassoMensile = FINANCE.RATE(300, rataB, capitale, 0, 0)
    PRINT "tasso mensile: "; MATH.ROUND(tassoMensile * 100.0, 4); "%"
    PRINT "tasso annuo:   "; MATH.ROUND(tassoMensile * 12.0 * 100.0, 2); "%"

    ' ── 4. Rata maggiorata: quando finisce il mutuo? ─────────────────────────
    ' Pagando 100 EUR in più al mese il debito si chiude prima: NPER lo stima
    ' (valore negativo perché il debito residuo è ancora aperto).
    PRINT "== 4. Rata maggiorata di 100 EUR =="
    DIM mesi AS DOUBLE
    mesi = FINANCE.NPER(0.027 / 12.0, rataB - 100.0, capitale, 0, 0)
    PRINT "mesi necessari: "; MATH.ROUND(mesi, 1); " ("; MATH.ROUND(mesi / 12.0, 1); " anni)"
    PRINT "rate risparmiate: "; MATH.ROUND(300 - mesi, 1)

    ' ── 5. Costo totale dell'offerta B ───────────────────────────────────────
    PRINT "== 5. Costo totale =="
    DIM totalePagato AS DOUBLE
    totalePagato = ABS(rataB) * 300
    PRINT "rate pagate:     "; MATH.ROUND(totalePagato, 2); " EUR"
    PRINT "interessi totali: "; MATH.ROUND(totalePagato - capitale, 2); " EUR"
    PRINT "ogni 1000 EUR prestati costano "; MATH.ROUND((totalePagato - capitale) / capitale * 1000.0, 2); " EUR di interessi"
END SUB
