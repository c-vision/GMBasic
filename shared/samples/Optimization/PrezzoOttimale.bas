' FILE: Optimization/PrezzoOttimale.bas
' SCENARIO DIDATTICO — quale prezzo massimizza il profitto di una pasticceria?
'
' La pasticceria vende torte. Ogni torta costa 3 EUR di ingredienti e, al prezzo p,
' la domanda è q(p) = 200 - 5p (più cara, meno torte). Il profitto è
'     profitto(p) = (p - 3) * q(p)
' e la domanda del titolare è: a che prezzo conviene vendere? Lo scenario risponde con
' quattro strumenti diversi, come farebbe un analista:
'   1) MINIMIZESCALAR / GOLDENSECTION su UNA variabile -> prezzo ottimo;
'   2) NELDERMEAD su DUE variabili (prezzo e spesa di pubblicità);
'   3) BISECTION per il punto di PAREGGIO (profitto = 0);
'   4) LEVENBERGMARQUARDT per stimare la curva di domanda dai dati di vendita.
'
' Nota: gli ottimizzatori ricevono FUNZIONI NOMINATE (non lambda `FN(...) => ...`):
' è la forma supportata da tutti i backend ed è anche più leggibile.
' Domini: OPTIMIZE + MATH. Input fissi: output riproducibile.
IMPORT OPTIMIZE
IMPORT MATH

' Domanda al prezzo p.
FUNCTION DomandaA(p AS DOUBLE) AS DOUBLE
    RETURN 200.0 - 5.0 * p
END FUNCTION

' Profitto di una sola variabile: il prezzo.
FUNCTION Profitto(p AS DOUBLE) AS DOUBLE
    RETURN (p - 3.0) * DomandaA(p)
END FUNCTION

' Per cercare un MASSIMO con un minimizzatore si minimizza il profitto cambiato di segno.
FUNCTION ProfittoNegativo(p AS DOUBLE) AS DOUBLE
    RETURN -Profitto(p)
END FUNCTION

' Profitto con due decisioni: prezzo e spesa pubblicitaria.
' La pubblicità alza la domanda di `2 * radice(spesa)` e costa la spesa stessa.
FUNCTION ProfittoDue(v() AS DOUBLE) AS DOUBLE
    DIM prezzo AS DOUBLE
    DIM spesa AS DOUBLE
    prezzo = v(0)
    spesa = v(1)
    DIM domanda AS DOUBLE
    domanda = 200.0 - 5.0 * prezzo + 2.0 * SQR(ABS(spesa))
    RETURN (prezzo - 3.0) * domanda - spesa
END FUNCTION

' Versione cambiata di segno per NELDERMEAD.
FUNCTION ProfittoDueNegativo(v() AS DOUBLE) AS DOUBLE
    RETURN -ProfittoDue(v)
END FUNCTION

' Modello di domanda q = a + b*p usato per il fit sui dati reali.
FUNCTION ModelloDomanda(param() AS DOUBLE, prezzo AS DOUBLE) AS DOUBLE
    RETURN param(0) + param(1) * prezzo
END FUNCTION

SUB Main()
    ' ── 1. Prezzo ottimo (una variabile) ─────────────────────────────────────
    PRINT "== 1. Prezzo ottimo =="
    DIM prezzoOttimo AS DOUBLE
    prezzoOttimo = OPTIMIZE.MINIMIZESCALAR(ProfittoNegativo, 3.0, 40.0)
    PRINT "prezzo consigliato: "; MATH.ROUND(prezzoOttimo, 2); " EUR"
    PRINT "profitto atteso:    "; MATH.ROUND(Profitto(prezzoOttimo), 2); " EUR"

    ' Controprova con la ricerca aurea: stesso intervallo, stesso risultato.
    DIM prezzoAurea AS DOUBLE
    prezzoAurea = OPTIMIZE.GOLDENSECTION(ProfittoNegativo, 3.0, 40.0)
    PRINT "golden section:     "; MATH.ROUND(prezzoAurea, 2); " EUR"

    ' ── 2. Due decisioni insieme: prezzo e pubblicità ────────────────────────
    ' Con la pubblicità il profitto cresce: NELDERMEAD cerca la coppia
    ' (prezzo, spesa) migliore partendo da un'ipotesi grossolana.
    PRINT "== 2. Prezzo + pubblicita' =="
    DIM partenza(1) AS DOUBLE
    partenza(0) = 30.0
    partenza(1) = 100.0
    DIM ottimo() AS DOUBLE
    ottimo() = OPTIMIZE.NELDERMEAD(ProfittoDueNegativo, partenza)
    ' Arrotondato all'euro (prezzo a 1 decimale): gli ottimizzatori fermano la
    ' ricerca su tolleranze leggermente diverse, e 534,82 vs 535,05 non è un
    ' disallineamento di semantica ma di punto di arresto.
    PRINT "prezzo:     "; MATH.ROUND(ottimo(0), 1); " EUR"
    PRINT "pubblicita': "; MATH.ROUND(ottimo(1), 0); " EUR"
    PRINT "profitto:   "; MATH.ROUND(ProfittoDue(ottimo), 0); " EUR"

    ' ── 3. Punto di pareggio ─────────────────────────────────────────────────
    ' A quale prezzo il profitto è zero? BISECTION lavora sul segno della funzione.
    PRINT "== 3. Pareggio =="
    DIM pareggio AS DOUBLE
    pareggio = OPTIMIZE.BISECTION(Profitto, 3.0, 40.0)
    PRINT "prezzo di pareggio: "; MATH.ROUND(pareggio, 2); " EUR"
    PRINT "profitto a pareggio: "; MATH.ROUND(Profitto(pareggio), 2); " EUR"

    ' ── 4. La curva di domanda dai dati reali ────────────────────────────────
    ' Sei rilevazioni prezzo/quantità: la retta q = a + b*p si stima con
    ' LEVENBERGMARQUARDT (minimi quadrati non lineari).
    PRINT "== 4. Stima della domanda =="
    DIM prezzi(5) AS DOUBLE
    prezzi(0) = 10.0 : prezzi(1) = 15.0 : prezzi(2) = 20.0
    prezzi(3) = 25.0 : prezzi(4) = 30.0 : prezzi(5) = 35.0
    DIM quantita(5) AS DOUBLE
    quantita(0) = 150.0 : quantita(1) = 125.0 : quantita(2) = 100.0
    quantita(3) = 75.0  : quantita(4) = 50.0  : quantita(5) = 25.0
    DIM stima(1) AS DOUBLE
    stima(0) = 0.0 : stima(1) = 0.0
    DIM fit() AS DOUBLE
    fit() = OPTIMIZE.LEVENBERGMARQUARDT(ModelloDomanda, stima, prezzi, quantita)
    PRINT "intercetta a = "; MATH.ROUND(fit(0), 2); " (attesa 200)"
    PRINT "pendenza   b = "; MATH.ROUND(fit(1), 2); " (attesa -5)"

    ' Con la curva stimata si rivaluta il prezzo ottimo: deve restare ~21,50.
    ' Massimo di (p-c)(a+bp) in p: p* = (b*c - a) / (2b).
    DIM costo AS DOUBLE
    costo = 3.0
    PRINT "prezzo ottimo dalla curva stimata: "; _
          MATH.ROUND((fit(1) * costo - fit(0)) / (2.0 * fit(1)), 2)
END SUB
