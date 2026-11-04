# V.27 — Matematica e statistica

## Cosa ottieni da questo capitolo

Le funzioni matematiche di base, la statistica descrittiva, l'interpolazione e le distribuzioni.
Con una regola che vale più di tutte le funzioni: un numero che esce da una formula va **verificato
contro un valore noto**, non guardato.

## 1. Il livello di base

Funzioni trigonometriche, logaritmi, potenze, arrotondamenti e combinatoria stanno nel namespace
`MATH`: `MATH.SIN`, `MATH.LOG`, `MATH.FACT`, `MATH.SQRT`, `MATH.ATN` e compagni. Il dettaglio è in
`Funzioni-Matematiche-Guida-Utente` e `Trig-Guida-Utente`.

```basic
' verifica di primalità: un caso in cui sbagliare è facile
FOR n = 2 TO 50
    IF ISPRIME(n) THEN PRINT n
NEXT
```

Eseguito (`shared/samples/Math/MathPrimeNumbers.bas`, backend C99Native):

```
 2
 3
 5
 7
 11
 13
 17
 19
 23
 29
 31
 37
 41
 43
 47
```

Nessun multiplo, nessun numero saltato: è così che si verifica una funzione numerica — su una
sequenza di cui **conosci** il risultato.

## 2. Statistica descrittiva

`STATS` offre media, mediana, varianza, deviazione standard, MAD, asimmetria, curtosi,
correlazione e quantili. Eseguito (`shared/samples/Analysis/AnalysisStatsDescriptive.bas`):

```
Dataset 1: [1,2,3,4,5,6,7,8,9,10]
Media (Mean):          5.5
Mediana (Median):      5.5
Varianza (Variance):   9.1667
Dev. Standard (StdDev):  3.0277
MAD (Median Abs Dev):  2.5
Asimmetria (Skewness): 2.214
Curtosi (Kurtosis):   -1.2
Correlazione:         -1  (Atteso: -1)
90° Percentile:        1
50° Percentile:        1  (Dovrebbe coincidere con la Mediana)
```

Media e mediana tornano (`5.5`), varianza e deviazione standard sono coerenti fra loro: questi
sono i valori su cui puoi contare. **Le ultime due righe no**, e il sample stesso lo ammette: su
un dataset uniforme da 1 a 10 l'asimmetria deve essere `0` (esce `2.214`), e il 50° percentile deve
coincidere con la mediana (esce `1`). Sul backend nativo i quantili e l'asimmetria non sono
affidabili: vanno verificati su un altro backend e trattati come difetto aperto, non usati in una
decisione.

## 3. Le librerie numeriche

| Ambito | Namespace / guida | Cosa copre |
|---|---|---|
| matrici (struttura e comandi) | `MATRIX`, `Matrix-Linalg-Manual` | creazione, accesso, copia, slicing, prodotti, predicati |
| algebra lineare (algoritmi) | `LINALG`, `Matrix-Linalg-Manual` | decomposizioni, risolutori, norme, funzioni matriciali |
| vettori | `VECTOR` | norme, distanze, prodotto scalare |
| statistica | `STATS`, `Stats-Manual` | descrittiva, test, distribuzioni |
| serie temporali | `TIMESERIES` | ARIMA, GARCH, Kalman, HMM, previsione |
| regressione e fitting | `REGRESSION`, `FITTING` | modelli lineari, curve fit |
| multivariata e simulazione | `MULTIVARIATE`, `SIMULATION` | PCA, clustering, Monte Carlo |
| interpolazione e curve | `AnalysisInterpReal.bas`, `CurveFitSciPyParity.bas` | interpolazione, fit |
| FFT | `IntegrateInterpFftSciPyParity.bas` | trasformata rapida |
| ricerca spaziale | `KdTreeAdvanced.bas`, `Spatial-Guida-Utente` | k-d tree, nearest neighbour |
| ottimizzazione | `Optimize-Manual` | minimi, derivate, ricerca |
| funzioni speciali | `Special-Manual` | gamma, beta, error function |

## 4. La parità con SciPy come metodo

Molti sample hanno `SciPyParity` nel nome: `CurveFitSciPyParity.bas`,
`DifferentialEvolutionSciPyParity.bas`, `StatsSciPyParity.bas`. Non è folklore — è il modo in cui
queste funzioni vengono verificate: lo stesso input, il risultato di riferimento di SciPy, il
confronto entro tolleranza.

Se devi validare una formula, questo è il modello da copiare: **un valore di riferimento esterno**,
non il tuo stesso codice rifatto due volte.

## 5. BITS: il livello sotto i numeri

`BITS` espone operazioni bit a bit, popcount, shift e maschere (`Bits/BitsFullCoverage.bas`).
Serve per i formati binari, per i flag compattati e per chi implementa un algoritmo che lavora
sui bit — non per l'aritmetica quotidiana.

## 6. Finanza quantitativa

`FINANCE` non è statistica: sono formule finanziarie (interessi, ammortamenti, VAN, TIR) con
varianti che cambiano da paese a paese. Le guide `Finance-Guida-Utente` e
`FinanceAPRAlgorithm.bas` documentano anche **quale** convenzione è implementata — è la parte che
si sbaglia quando si confrontano due software.

## 7. Ricerca operativa: ricerca locale, coni, rilassamento lagrangiano

Il namespace `OPTIMIZE` non contiene solo ottimizzazione numerica di funzione (`SIMPLEX`,
`BFGS`, minimi quadrati). Tre pacchetti FFI coprono la **ricerca operativa** e i **metodi di
punto interno**: `libgmblocalsearch`, `libgmbconic`, `libgmblagrangian`. Sono superfici
progettate per un ciclo di soluzione scritto in BASIC: si fornisce lo stato iniziale, una
callback che misura la soluzione, e il motore restituisce la soluzione migliore e i
moltiplicatori. Con `IMPORT OPTIMIZE` sono visibili tutte insieme.

### 7.1 Ricerca locale (`LS*`)

Il motore lavora su uno **stato vettoriale di interi** e applica mosse elementari
(ricollocamento, scambio, 2-opt): è la struttura tipica di TSP, scheduling, assegnamento.
`shape` decide quali mosse sono legali — `0` per una permutazione di `0..n-1`, `1` per
un'assegnazione di valori piccoli. `LSNEAREST` e `LSCHEAPEST` costruiscono una soluzione
iniziale da una matrice delle distanze (rispettivamente vicino più prossimo e inserimento
più economico); `LSSOLVE` e `LSSOLVECFG` la migliorano, il secondo con partenze multiple e
un criterio di accettazione selezionabile (hill climbing, late acceptance, tabu, simulated
annealing, great deluge, step-counting, record-to-record). `LSNOWMS` fornisce l'orologio di
CPU per fissare un budget di tempo.

Due dettagli contano più della firma:

- **il motore massimizza**. Un problema di minimo (un costo, una distanza) si scrive
  restituendo il valore **negato**: dimenticarlo produce giri peggiori a ogni iterazione;
- **`initial()` è in ingresso e in uscita**: al ritorno (esito `1`) contiene lo stato
  migliore trovato, non il punto di partenza. È il modo con cui si legge la soluzione.

L'ammissibilità non è un canale separato: il motore confronta solo il punteggio `soft`, quindi
un vincolo violato si rappresenta con una penalità dentro il punteggio.

### 7.2 Coni esponenziale e potenza (`EXPCONE*`)

Questi generatori sono i mattoni di un solver di punto interno per problemi conici: dato un
punto interno, producono barriera, gradiente, Hessiana, la verifica di appartenenza al cono
duale, il punto di scaling duale, le metriche di complementarità e la **scalatura NT**
(`Theta = inv(Thin)`, vettore `rt`, e la gap come `scale`). Il punto è il vettore `(t,u,v)`;
`kind` sceglie il cono (`0` esponenziale, `1` potenza, `2` potenza ruotata) e `alpha` — usato
solo dai coni di potenza — ne cambia il generatore. Un punto fuori dal cono non è un errore
silenzioso: le funzioni restituiscono `-1` e non scrivono risultati parziali.

La verifica di correttezza non richiede un solver: si confrontano gradiente e Hessiana con
differenze finite centrali della barriera e si controlla la convessità con i criteri di
Sylvester (`08-Conic.bas` fa esattamente questo, e verifica che `scale` coincida con la gap
delle metriche NT).

### 7.3 Rilassamento lagrangiano (`LAGRANGIANSUBGRADIENT`)

Per `min f(x)` soggetto a `A x >= b` e `x` in un insieme `X`, i vincoli vengono spostati nella
funzione obiettivo con moltiplicatori `lambda >= 0` e aggiornati lungo il subgradiente
`b - A x`, con passo di Polyak. Il subproblema su `X` lo risolve il chiamante: la callback
restituisce **un solo vettore** che contiene il valore ottimo del subproblema e la soluzione
`x` (il motore ha bisogno di entrambi, e l'ABI FFI ammette un solo valore di ritorno).
Il risultato è un **lower bound** valido — non una soluzione: la `x` restituita è il punto del
rilassamento che ha prodotto il bound migliore e può non rispettare i vincoli. Il divario
rispetto all'ottimo intero è il gap di dualità, e si riduce aumentando le iterazioni
(`09-Lagrangian.bas` confronta 10 e 200 iterazioni con l'ottimo calcolato per forza bruta).

### 7.4 Ricerca operativa completa (`ortools.inc`, 106 funzioni)

Se il problema è classico — imballare, assegnare, trasportare, coprire, localizzare,
sequenziare, instradare, simulare code, decidere con più criteri — la risposta è già scritta
in `ortools.inc`: 106 funzioni divise per dominio (`PACK`, `ASSIGN`, `COVER`, `CP`, `ROUTING`,
`SCHED`, `META`, `UTIL`, `DP`). Le convenzioni sono quelle del paragrafo precedente e una in
più conta: **tutte le euristiche sono deterministiche**, perché nessuna usa il tempo come
seed; due esecuzioni sullo stesso input danno lo stesso risultato su tutti e quattro i
backend. È ciò che rende confrontabili due algoritmi sullo stesso problema e che permette di
usare queste funzioni in un test automatico.

Il valore di ritorno è sempre l'oggetto del problema (costo, makespan, numero di contenitori
o di rotte, `-1`/`0` per errore), mentre la **soluzione** — l'assegnamento, il giro, la
sequenza, i moltiplicatori — si legge da un array `out()` che il chiamante dimensiona: `n`
per un vettore di decisioni, `m*n` per un piano di trasporto, `pop*d` più il fronte per
NSGA-II. Non ci sono struct da costruire: l'ABI FFI non le restituisce, e questo è il motivo
per cui anche le funzioni più articolate (`PROJECTPERT`, `VRPTIMEWINDOWS`, `METANSGA2`) restano
chiamabili da BASIC in una riga.

Tre trappole sono documentate dai test: `TSPHKBOUND` restituisce un **lower bound** e non un
giro; `CHANCECONSTRAINT` interpreta `alpha` come **coda** della distribuzione (0.05, non
0.95); `TSPDFJ` vuole un `tour()` già inizializzato perché è un MIP con tagli, non un
costruttivo.

### 7.5 LP avanzata e pianificazione aeronautica

Due pacchetti completano il quadro. **`mathprog`** espone l'analisi fine della programmazione
lineare: `LPREVISED` (simplesso revised con base in ingresso/uscita), `LPDUAL` (variabili
duali), `LPREDUCEDCOSTS` (costi ridotti rispetto ai duali), `LPALTERNATE` (ottimo alternativo),
`LPBOUNDS` (la stessa LP con bounds per variabile), `GOMORYCUT` (taglio da una riga
frazionaria del tableau) e le due matheuristiche `MATHHARDFIXING` / `MATHLOCALBRANCHING` per
il problema intero. **`aviation`** modella una giornata operativa di una compagnia: `AVFLEET`
(assegnazione flotte), `AVCREWPAIRING` (equipaggi), `AVRECOVERY` (disruption), 
`AVCREWRESCHED` (ri-assegnazione), `AVHUB` (hub location con economia `alpha`) e
`AVTAILCONTINUITY` (rotazione degli aeromobili).

I due mondi hanno la stessa forma mentale: una matrice dei costi, un vettore di decisioni che
si legge da `out()`, un valore di ritorno che è l'obiettivo. Cambia il segno dei vincoli —
`LPREVISED`/`LPDUAL`/`LPALTERNATE` lavorano su **uguaglianze** (`A x = b`) come `LPSOLVE`,
mentre `LPBOUNDS` lavora su disuguaglianze con bounds — ed è la prima cosa da controllare
quando una soluzione "non torna".

### 7.6 Esempio minimo

```basic
IMPORT OPTIMIZE

FUNCTION Score(state() AS DOUBLE) AS DOUBLE      ' massimizza: somma pesata
  s = 0
  FOR i = 0 TO UBOUND(state) : s = s + i * state(i) : NEXT i
  RETURN s
END FUNCTION

DIM init(3) AS LONG = {3, 2, 1, 0}
DIM out(2) AS DOUBLE
PRINT OPTIMIZE.LSSOLVE(Score, init, 4, 0, 200, out)   ' 1
PRINT out(1), init(0), init(1), init(2), init(3)      ' 14  0 1 2 3
```

## 8. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| percentile inatteso | convenzione o difetto del backend | verifica su un altro backend, non "aggiusta" l'input |
| asimmetria diversa da 0 su dati simmetrici | idem | confronta con SciPy (`StatsSciPyParity`) |
| varianza "sbagliata" | divisore `n` contro `n-1` | controlla la convenzione della guida |
| `NaN` in un risultato | dominio non valido (log di 0, sqrt negativo) | valida l'input prima |
| confronto fra double che fallisce | uguaglianza esatta | usa una tolleranza |

## 9. Dove andare adesso

- **V.28** — le stesse formule applicate a serie temporali e trading.
- **V.31** — TORCH, quando la matematica diventa tensori.
- **III.15** — come si scrive il test di una funzione numerica.

















<!-- BEGIN DOMAIN TABLE -->
## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **MATH.ABS(x1)** — `PRINT "ABS(-10.5): "; MATH.ABS(-10.5)` (shared/samples/Math/MathLogarithms.bas:74)
- **MATH.ACOS(x1)** — `PRINT "Arcocoseno (ACOS) di 0:     "; MATH.ACOS(0); " (rad)"` (shared/samples/Math/MathTrigonometry.bas:47)
- **MATH.ASIN(x1)** — `PRINT "Arcoseno (ASIN) di 1:       "; MATH.ASIN(1); " (rad)"` (shared/samples/Math/MathTrigonometry.bas:44)
- **MATH.ATN(x1)** — `PRINT "ATN(1)  = "; ROUND(MATH.RAD2DEG(MATH.ATN(1)), 6); " deg"` (shared/samples/Math/MathAtnVectorAngle.bas:28)
- **MATH.AVG()** — `media = MATH.AVG(risparmi)` (shared/samples/Math/PianificazioneRisparmi.bas:19)
- **MATH.COSH(x1)** — `PRINT "Coseno Iperbolico (COSH) di 1:  "; MATH.COSH(1)` (shared/samples/Math/MathTrigonometry.bas:63)
- **MATH.CEILING(x1)** — `PRINT "  CEILING(num):  "; MATH.CEILING(num_pos)` (shared/samples/Math/MathLogarithms.bas:59)
- **MATH.CLAMP(x1, x2, x3)** — `PRINT "CLAMP(150, 0, 100): "; MATH.CLAMP(val, 0, 100); " (Atteso 100)"` (shared/samples/Math/MathCombinatorics.bas:70)
- **MATH.COMB(x1, x2)** — `PRINT "COMB("; n; ", "; k; "):   "; MATH.COMB(n, k)` (shared/samples/Math/MathCombinatorics.bas:39)
- **MATH.COS(x1)** — `PRINT "Coseno (COS):     "; MATH.COS(angle_rad)` (shared/samples/Math/MathTrigonometry.bas:33)
- **MATH.DEG2RAD(x1)** — `DIM angle_rad = MATH.DEG2RAD(angle_deg)` (shared/samples/Math/MathTrigonometry.bas:27)
- **MATH.EXP(x1)** — `ys(0) = 1.0 : ys(1) = MATH.EXP(0.5) : ys(2) = MATH.EXP(1.0) : ys(3) = MATH.EXP(1.5) : ys(4) = MATH.E…` (shared/samples/include/optimize/05-Fitting.bas:9)
- **MATH.FACT(x1)** — `PRINT "FACT("; n; "):      "; MATH.FACT(n)` (shared/samples/Math/MathCombinatorics.bas:31)
- **MATH.FIX(x1)** — `PRINT "  FIX(num):      "; MATH.FIX(num_pos)` (shared/samples/Math/MathLogarithms.bas:56)
- **MATH.FLOOR(x1)** — `PRINT "  FLOOR(num):    "; MATH.FLOOR(num_pos)` (shared/samples/Math/MathLogarithms.bas:58)
- **MATH.HYPOT(x1, x2)** — `PRINT "HYPOT(3, 4):        "; MATH.HYPOT(3, 4)` (shared/samples/Math/MathCombinatorics.bas:66)
- **MATH.INT(x1)** — `PRINT "  INT(num):      "; MATH.INT(num_pos)` (shared/samples/Math/MathLogarithms.bas:57)
- **MATH.ISNUMERIC(x1)** — `IF MATH.ISNUMERIC(Input) THEN` (shared/samples/Types/TypesAny.bas:91)
- **MATH.LOG(x1)** — `PRINT "LOG(e):                     "; MATH.LOG(e); " (Atteso: 1)"` (shared/samples/Math/MathLogarithms.bas:34)
- **MATH.LOG10(x1)** — `PRINT "LOG10(1000):                "; MATH.LOG10(1000); " (Atteso: 3)"` (shared/samples/Math/MathLogarithms.bas:37)
- **MATH.MAX()** — `GmbStats = "mean=" + STR$(STATS.MEAN(n)) + " max=" + STR$(MATH.MAX(n[0], n[1], n[2])) + " min=" + ST…` (shared/samples/Ai/McpCapabilityServer.bas:51)
- **MATH.MIN()** — `PRINT "MIN(3, 1, 4, 1, 5, 9): "; MATH.MIN(3, 1, 4, 1, 5, 9) ' Atteso: 1` (shared/samples/Math/MathExtended.bas:43)
- **MATH.PERM(x1, x2)** — `PRINT "PERM("; n; ", "; k; "):   "; MATH.PERM(n, k)` (shared/samples/Math/MathCombinatorics.bas:35)
- **MATH.PI()** — `SerieStagionale(si) = (si * 0.1) + SIN(si * 2 * MATH.PI() / 12) * 5 + (MATH.RND() - 0.5)` (shared/samples/TimeSeries/TimeSeriesAdvanced.bas:63)
- **MATH.PYTHAGORAS(x1, x2)** — `820 PRINT "PYTHAGORAS(3, 4) =", PYTHAGORAS(3, 4); "(Atteso: 5)"` (shared/samples/Types/TypesConversions.bas:135)
- **MATH.RAD2DEG(x1)** — `PRINT "ATN(1)  = "; ROUND(MATH.RAD2DEG(MATH.ATN(1)), 6); " deg"` (shared/samples/Math/MathAtnVectorAngle.bas:28)
- **MATH.RANGE()** — `PRINT "MATH.RANGE — Sequenze numeriche"` (shared/samples/Math/RangeDemo.bas:12)
- **MATH.RND()** — `Prices(i) = Prices(i-1) * (1 + (MATH.RND() - 0.5) * 0.02)` (shared/samples/TimeSeries/TimeSeriesTests.bas:34)
- **MATH.ROUND(x1)** — `PRINT "scostamento: "; MATH.ROUND(scostamento, 2); " EUR"` (shared/samples/Databases/ContabilitaPasticceria.bas:69)
- **MATH.SINH(x1)** — `PRINT "Seno Iperbolico (SINH) di 1:    "; MATH.SINH(1)` (shared/samples/Math/MathTrigonometry.bas:62)
- **MATH.SGN(x1)** — `PRINT "SGN(-10.5): "; MATH.SGN(-10.5)` (shared/samples/Math/MathLogarithms.bas:75)
- **MATH.SIN(x1)** — `PRINT "Seno (SIN):       "; MATH.SIN(angle_rad)` (shared/samples/Math/MathTrigonometry.bas:32)
- **MATH.SQR(x1)** — `PRINT "SQR(25):                    "; MATH.SQR(25); " (Atteso: 5)"` (shared/samples/Math/MathLogarithms.bas:43)
- **MATH.SUM()** — `SPEED SELECT Clienti.Nome AS NomeCliente, SUM(Ordini.Importo) AS TotaleImporto INTO ris() OF Risulta…` (shared/samples/Databases/DbSpeedFullTests.bas:261)
- **MATH.TANH(x1)** — `PRINT "Tangente Iperbolica (TANH) di 1: "; MATH.TANH(1)` (shared/samples/Math/MathTrigonometry.bas:64)
- **MATH.TAN(x1)** — `PRINT "Tangente (TAN):   "; MATH.TAN(angle_rad)` (shared/samples/Math/MathTrigonometry.bas:34)
- **MATH.TRUNC(x1)** — `PRINT "  TRUNC(num, 2): "; MATH.TRUNC(num_pos, 2)` (shared/samples/Math/MathLogarithms.bas:55)
- **MATRIX.ABOVEMASK(x1, x2)** — `DIM flatMask() = MATRIX.ABOVEMASK(rowsBroadcast, 15.0)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:51)
- **MATRIX.ABOVEMATRIX(x1, x2)** — `DIM matrixMask() = MATRIX.ABOVEMATRIX(rowsBroadcast, 15.0)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:52)
- **MATRIX.ADDCOLVECTOR(x1, x2)** — `DIM columnsBroadcast = MATRIX.ADDCOLVECTOR(m, columnVector)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:42)
- **MATRIX.ADDROWVECTOR(x1, x2)** — `DIM rowsBroadcast = MATRIX.ADDROWVECTOR(m, rowVector)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:41)
- **MATRIX.ARGMAX(x1)** — `DIM amax() = MATRIX.ARGMAX(m)` (shared/samples/Arrays/MatrixArgmaxShadow.bas:20)
- **MATRIX.ARGMIN(x1)** — `DIM amin() = MATRIX.ARGMIN(m)` (shared/samples/Arrays/MatrixArgmaxShadow.bas:21)
- **MATRIX.ASCSV(x1)** — `PRINT "ASCSV="; MATRIX.ASCSV(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:57)
- **MATRIX.ASJSON(x1)** — `PRINT "ASJSON="; MATRIX.ASJSON(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:58)
- **MATRIX.BLOCKDIAG(x1, x2)** — `bd = MATRIX.BLOCKDIAG(h, h)` (shared/samples/Math/MatrixFunctionsSciPyParity.bas:27)
- **MATRIX.CIRCULANT(x1)** — `cc = MATRIX.CIRCULANT(c1)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:49)
- **MATRIX.CLONEMATRIX(x1)** — `DIM cloned = MATRIX.CLONEMATRIX(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:60)
- **MATRIX.COLMEANS(x1)** — `DIM meansByColumn() = MATRIX.COLMEANS(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:13)
- **MATRIX.COLSUMS(x1)** — `DIM sumsByColumn() = MATRIX.COLSUMS(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:11)
- **MATRIX.COLUMNCOUNT(x1)** — `PRINT "COLUMNCOUNT="; MATRIX.COLUMNCOUNT(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:64)
- **MATRIX.COLVARIANCES(x1)** — `DIM variancesByColumn() = MATRIX.COLVARIANCES(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:15)
- **MATRIX.CONCATCOLS(x1, x2)** — `DIM columnsJoined = MATRIX.CONCATCOLS(m, m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:40)
- **MATRIX.CONCATROWS(x1, x2)** — `DIM rowsJoined = MATRIX.CONCATROWS(m, m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:39)
- **MATRIX.CREATE(x1, x2)** — `h = MATRIX.CREATE(2, 2)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:7)
- **MATRIX.GETVALUE(x1, x2, x3)** — `FOR k = 0 TO 4: re(k) = MATRIX.GETVALUE(rf, k, 0): im(k) = MATRIX.GETVALUE(rf, k, 1): NEXT` (shared/samples/Analysis/AnalysisMiniTest.bas:20)
- **MATRIX.HANKEL(x1, x2)** — `hk = MATRIX.HANKEL(c1, r1)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:47)
- **MATRIX.ISLOWERTRIANGULAR(x1)** — `PRINT "LOWER="; MATRIX.ISLOWERTRIANGULAR(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:31)
- **MATRIX.ISORTHOGONAL(x1)** — `PRINT "ORTHOGONAL="; MATRIX.ISORTHOGONAL(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:29)
- **MATRIX.ISSYMMETRIC(x1)** — `PRINT "SYMMETRIC="; MATRIX.ISSYMMETRIC(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:28)
- **MATRIX.ISUPPERTRIANGULAR(x1)** — `PRINT "UPPER="; MATRIX.ISUPPERTRIANGULAR(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:30)
- **MATRIX.KRONECKER(x1, x2)** — `k = MATRIX.KRONECKER(h, h)` (shared/samples/Math/MatrixFuncSciPyParity.bas:10)
- **MATRIX.LOADCSV(x1)** — `DIM csvMatrix = MATRIX.LOADCSV("samples/Fixtures/MatrixIdentity.csv")` (shared/samples/Matrix/MatrixCommonLibSimd.bas:69)
- **MATRIX.LOADJSON(x1)** — `DIM jsonMatrix = MATRIX.LOADJSON("samples/Fixtures/MatrixIdentity.json")` (shared/samples/Matrix/MatrixCommonLibSimd.bas:70)
- **MATRIX.MAXVALUE(x1)** — `PRINT "MAXVALUE="; MATRIX.MAXVALUE(rowsBroadcast)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:53)
- **MATRIX.MINVALUE(x1)** — `PRINT "MINVALUE="; MATRIX.MINVALUE(rowsBroadcast)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:54)
- **MATRIX.MULTIPLY(x1, x2)** — `DIM mul = MATRIX.MULTIPLY(a, a)` (shared/samples/Matrix/MatrixDecompositions.bas:26)
- **MATRIX.MULTIPLYLEFTVECTOR(x1, x2)** — `DIM leftProduct() = MATRIX.MULTIPLYLEFTVECTOR(m, columnVector)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:62)
- **MATRIX.MULTIPLYVECTOR(x1, x2)** — `DIM rightProduct() = MATRIX.MULTIPLYVECTOR(m, rowVector)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:61)
- **MATRIX.RESHAPE(x1, x2, x3)** — `DIM reshaped = MATRIX.RESHAPE(m, 1, 9)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:38)
- **MATRIX.ROWCOUNT(x1)** — `PRINT "welchrows ="; MATRIX.ROWCOUNT(w)` (shared/samples/Math/SciPyP1Parity.bas:12)
- **MATRIX.ROWMEANS(x1)** — `DIM meansByRow() = MATRIX.ROWMEANS(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:12)
- **MATRIX.ROWSUMS(x1)** — `DIM sumsByRow() = MATRIX.ROWSUMS(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:10)
- **MATRIX.ROWVARIANCES(x1)** — `DIM variancesByRow() = MATRIX.ROWVARIANCES(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:14)
- **MATRIX.SETVALUE(x1, x2, x3, x4)** — `h = MATRIX.SETVALUE(h, 0, 0, 2) : h = MATRIX.SETVALUE(h, 0, 1, 1)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:8)
- **MATRIX.SLICE(x1, x2, x3, x4, x5)** — `DIM sliced = MATRIX.SLICE(m, 0, 2, 0, 2)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:37)
- **MATRIX.TOEPLITZ(x1, x2)** — `tp = MATRIX.TOEPLITZ(c1, r1)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:44)
- **MATRIX.TRACE(x1)** — `PRINT "trace="; MATH.ROUND(MATRIX.TRACE(a), 6)` (shared/samples/Matrix/MatrixDecompositions.bas:15)
- **MATRIX.TRANSPOSE(x1)** — `DIM tr = MATRIX.TRANSPOSE(a)` (shared/samples/Matrix/MatrixDecompositions.bas:24)
- **STATS.ANOVA(x1)** — `result = STATS.ANOVA(groups)` (shared/samples/Analysis/StatsSciPyParity.bas:22)
- **STATS.BERNOULLIPMF(x1, x2)** — `PRINT "BERNOULLIPMF(1,0.5): "; FORMAT(STATS.BERNOULLIPMF(1,0.5), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:154)
- **STATS.BETACDF(x1, x2, x3)** — `PRINT "BETACDF(0.5,2,2): "; FORMAT(STATS.BETACDF(0.5,2,2), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:140)
- **STATS.BETAPDF(x1, x2, x3)** — `PRINT "BETAPDF(0.5,2,2): "; FORMAT(STATS.BETAPDF(0.5,2,2), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:139)
- **STATS.BETAPPF(x1, x2, x3)** — `PRINT "betappf  ="; STATS.BETAPPF(0.5, 2, 3)` (shared/samples/Math/StatsInverseSciPyParity.bas:15)
- **STATS.BINOMIALPMF(x1, x2, x3)** — `PRINT "BINOMIALPMF(3,10,0.5): "; FORMAT(STATS.BINOMIALPMF(3,10,0.5), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:155)
- **STATS.CHI2(x1, x2)** — `DIM chi2 = STATS.CHI2(observed, expected)` (shared/samples/Analysis/StatsCategoricalTests.bas:10)
- **STATS.CHI2CON(x1)** — `r = STATS.CHI2CON(tb)` (shared/samples/Math/StatsSciPyParityP0.bas:32)
- **STATS.CHICDF(x1, x2)** — `PRINT "CHICDF(1,2):   "; FORMAT(STATS.CHICDF(1,2), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:136)
- **STATS.CHIPDF(x1, x2)** — `PRINT "CHIPDF(1,2):   "; FORMAT(STATS.CHIPDF(1,2), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:135)
- **STATS.CHIPPF(x1, x2)** — `PRINT "chippf   ="; STATS.CHIPPF(0.9, 7)` (shared/samples/Math/StatsInverseSciPyParity.bas:13)
- **STATS.CHISQUAREDCDF(x1, x2)** — `PRINT "CHISQUAREDCDF(1,2): "; FORMAT(STATS.CHISQUAREDCDF(1,2), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:138)
- **STATS.CHISQUAREDISF(x1, x2)** — `PRINT "chi2isf  ="; STATS.CHISQUAREDISF(0.05, 5)` (shared/samples/Math/StatsInverseSciPyParity.bas:20)
- **STATS.CHISQUAREDPDF(x1, x2)** — `PRINT "CHISQUAREDPDF(1,2): "; FORMAT(STATS.CHISQUAREDPDF(1,2), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:137)
- **STATS.CHISQUAREDPPF(x1, x2)** — `PRINT "chi2ppf  ="; STATS.CHISQUAREDPPF(0.95, 5)` (shared/samples/Math/StatsInverseSciPyParity.bas:12)
- **STATS.CHISQUAREDSF(x1, x2)** — `PRINT "chi="; MATH.ROUND(STATS.ChiSquaredSf(3.841459, 1), 6)` (shared/minitests/stats/sf_extra.bas:5)
- **STATS.CORRELATION(x1, x2)** — `DIM Corr = STATS.Correlation(Dataset1, Dataset2)` (shared/samples/Analysis/AnalysisStatsDescriptive.bas:59)
- **STATS.COVARIANCE(x1, x2)** — `PRINT "cov="; MATH.ROUND(STATS.Covariance(x, y), 6)` (shared/minitests/stats/covariance.bas:6)
- **STATS.DESCRIBE(x1)** — `r = STATS.DESCRIBE(v)` (shared/samples/Math/StatsSciPyParityP0.bas:60)
- **STATS.EXPONENTIALCDF(x1, x2)** — `PRINT "EXPONENTIALCDF(1,2): "; FORMAT(STATS.EXPONENTIALCDF(1,2), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:148)
- **STATS.EXPONENTIALISF(x1, x2)** — `PRINT "expisf   ="; STATS.EXPONENTIALISF(0.05, 0.5)` (shared/samples/Math/StatsInverseSciPyParity.bas:22)
- **STATS.EXPONENTIALPDF(x1, x2)** — `PRINT "EXPONENTIALPDF(1,2): "; FORMAT(STATS.EXPONENTIALPDF(1,2), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:147)
- **STATS.EXPONENTIALPPF(x1, x2)** — `PRINT "expppf   ="; STATS.EXPONENTIALPPF(0.9, 0.5)` (shared/samples/Math/StatsInverseSciPyParity.bas:16)
- **STATS.EXPONENTIALSF(x1, x2)** — `PRINT "exp="; MATH.ROUND(STATS.ExponentialSf(2, 1.0), 6)` (shared/minitests/stats/sf_extra.bas:3)
- **STATS.FCDF(x1, x2, x3)** — `PRINT "FCDF(1,5,10):  "; FORMAT(STATS.FCDF(1,5,10), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:146)
- **STATS.FISF(x1, x2, x3)** — `PRINT "fisf     ="; STATS.FISF(0.05, 3, 8)` (shared/samples/Math/StatsInverseSciPyParity.bas:21)
- **STATS.FPDF(x1, x2, x3)** — `PRINT "FPDF(1,5,10):  "; FORMAT(STATS.FPDF(1,5,10), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:145)
- **STATS.FPPF(x1, x2, x3)** — `PRINT "fppf     ="; STATS.FPPF(0.95, 3, 8)` (shared/samples/Math/StatsInverseSciPyParity.bas:14)
- **STATS.FSF(x1, x2, x3)** — `PRINT "f="; MATH.ROUND(STATS.FSf(1.0, 5, 10), 6)` (shared/minitests/stats/sf_weibull_f.bas:4)
- **STATS.FISHER(x1, x2, x3, x4)** — `DIM fisher = STATS.FISHER(8, 2, 1, 5)` (shared/samples/Analysis/StatsCategoricalTests.bas:14)
- **STATS.GAMMACDF(x1, x2, x3)** — `PRINT "gammacdf="; STATS.GAMMACDF(2, 3, 4)` (shared/samples/Math/StatsCdfPdfSciPyParity.bas:15)
- **STATS.GAMMAISF(x1, x2, x3)** — `PRINT "gammaisf ="; MATH.ROUND(STATS.GAMMAISF(2, 1, 0.05), 9)` (shared/samples/Math/StatsInverseSciPyParity.bas:23)
- **STATS.GAMMAPDF(x1, x2, x3)** — `PRINT "gammapdf="; STATS.GAMMAPDF(2, 3, 4)` (shared/samples/Math/StatsCdfPdfSciPyParity.bas:16)
- **STATS.GAMMAPPF(x1, x2, x3)** — `PRINT "gppf="; MATH.ROUND(STATS.GammaPpf(2, 1, 0.5), 6)` (shared/minitests/stats/stats_all.bas:222)
- **STATS.GAMMASF(x1, x2, x3)** — `DIM sf = STATS.GammaSf(2, 1, x)` (shared/minitests/stats/gamma_sf.bas:4)
- **STATS.GEOMETRICPMF(x1, x2)** — `PRINT "GEOMETRICPMF(3,0.5): "; FORMAT(STATS.GEOMETRICPMF(3,0.5), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:157)
- **STATS.HYPERGEOMETRICPMF(x1, x2, x3, x4)** — `PRINT "HYPERGEOMETRICPMF(2,5,10,20): "; FORMAT(STATS.HYPERGEOMETRICPMF(2,20,10,5), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:159)
- **STATS.IQR(x1)** — `PRINT "iqr      ="; MATH.ROUND(STATS.IQR(x), 6)` (shared/samples/Math/StatsSciPyParityP0.bas:20)
- **STATS.JARQUEBERA(x1)** — `r = STATS.JARQUEBERA(z)` (shared/samples/Math/StatsSciPyParityP0.bas:58)
- **STATS.KS(x1, x2)** — `result = STATS.KS(x, y)` (shared/samples/Analysis/StatsSciPyParity.bas:34)
- **STATS.KURTOSIS(x1)** — `PRINT "Curtosi (Kurtosis):   "; MATH.ROUND(STATS.Kurtosis(Dataset1), 4)` (shared/samples/Analysis/AnalysisStatsDescriptive.bas:57)
- **STATS.KURTOSISTEST(x1)** — `r = STATS.KURTOSISTEST(z)` (shared/samples/Math/StatsSciPyParityP0.bas:56)
- **STATS.LINREGRESS(x1, x2)** — `r = STATS.LINREGRESS(x, y)` (shared/samples/Math/StatsSciPyParityP0.bas:48)
- **STATS.LOGISTICPDF(x1, x2, x3)** — `PRINT "LOGISTICPDF(0,0,1): "; FORMAT(STATS.LOGISTICPDF(0,0,1), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:150)
- **STATS.LOGISTICPPF(x1, x2, x3)** — `PRINT "logippf  ="; STATS.LOGISTICPPF(0.5, 0, 1)` (shared/samples/Math/StatsInverseSciPyParity.bas:24)
- **STATS.LOGNORMALCDF(x1)** — `PRINT "LOGNORMALCDF(1): "; FORMAT(STATS.LOGNORMALCDF(1), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:134)
- **STATS.LOGNORMALPDF(x1)** — `PRINT "LOGNORMALPDF(1): "; FORMAT(STATS.LOGNORMALPDF(1), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:133)
- **STATS.LOGNORMALPPF(x1)** — `PRINT "logppf   ="; STATS.LOGNORMALPPF(0.9, 1, 0.5)` (shared/samples/Math/StatsInverseSciPyParity.bas:18)
- **STATS.MAX(x1)** — `PRINT "max="; STATS.MAX(x)` (shared/minitests/stats/max_min.bas:4)
- **STATS.MIN(x1)** — `PRINT "min="; STATS.MIN(x)` (shared/minitests/stats/max_min.bas:5)
- **STATS.MAD(x1)** — `PRINT "MAD (Median Abs Dev): "; STATS.MAD(Dataset1)` (shared/samples/Analysis/AnalysisStatsDescriptive.bas:50)
- **STATS.MANNWHITNEY(x1, x2)** — `r = STATS.MANNWHITNEY(x, y)` (shared/samples/Math/StatsSciPyParityP0.bas:50)
- **STATS.MEAN(x1)** — `PRINT "Media (Mean):         "; STATS.Mean(Dataset1)` (shared/samples/Analysis/AnalysisStatsDescriptive.bas:41)
- **STATS.MEDIAN(x1)** — `PRINT "Mediana (Median):     "; STATS.Median(Dataset1)` (shared/samples/Analysis/AnalysisStatsDescriptive.bas:42)
- **STATS.MODE(x1)** — `PRINT "mode="; STATS.Mode(a)` (shared/minitests/stats/stats_all.bas:226)
- **STATS.MOMENT(x1, x2)** — `PRINT "STATS.MOMENT — momenti del campione 1..10"` (shared/samples/Analysis/StatsMoment.bas:25)
- **STATS.NEGATIVEBINOMIALPMF(x1, x2, x3)** — `PRINT "NEGATIVEBINOMIALPMF(3,5,0.5): "; FORMAT(STATS.NEGATIVEBINOMIALPMF(3,5,0.5), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:158)
- **STATS.NORMALCDF(x1)** — `PRINT "NORMALCDF(0):  "; FORMAT(STATS.NORMALCDF(0), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:132)
- **STATS.NORMALISF(x1)** — `PRINT "normisf  ="; STATS.NORMALISF(0.025, 0, 1)` (shared/samples/Math/StatsInverseSciPyParity.bas:19)
- **STATS.NORMALPDF(x1)** — `PRINT "STATS.NORMALPDF(0):  "; FORMAT(STATS.NORMALPDF(0), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:131)
- **STATS.NORMALPPF(x1)** — `PRINT "normppf  ="; STATS.NORMALPPF(0.975, 1, 2)` (shared/samples/Math/StatsInverseSciPyParity.bas:9)
- **STATS.NORMALTEST(x1)** — `result = STATS.NORMALTEST(normal)` (shared/samples/Analysis/StatsSciPyParity.bas:30)
- **STATS.PARETOPDF(x1, x2, x3)** — `PRINT "PARETOPDF(1,2,3): "; FORMAT(STATS.PARETOPDF(1,2,3), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:149)
- **STATS.PARETOPPF(x1, x2, x3)** — `PRINT "paretoppf="; STATS.PARETOPPF(0.9, 3, 2)` (shared/samples/Math/StatsInverseSciPyParity.bas:25)
- **STATS.PEARSONR(x1, x2)** — `r = STATS.PEARSONR(x, y)` (shared/samples/Math/StatsSciPyParityP0.bas:44)
- **STATS.PERCENTILE(x1, x2)** — `DIM P90 = STATS.PERCENTILE(Dataset1, 0.90)` (shared/samples/Analysis/AnalysisStatsDescriptive.bas:67)
- **STATS.POISSONPMF(x1, x2)** — `PRINT "POISSONPMF(3,2): "; FORMAT(STATS.POISSONPMF(3,2), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:156)
- **STATS.RANGE(x1)** — `PRINT "range="; STATS.RANGE(x)` (shared/minitests/stats/range.bas:5)
- **STATS.SHAPIRO(x1)** — `result = STATS.SHAPIRO(normal)` (shared/samples/Analysis/StatsSciPyParity.bas:28)
- **STATS.SKEWTEST(x1)** — `r = STATS.SKEWTEST(z)` (shared/samples/Math/StatsSciPyParityP0.bas:54)
- **STATS.SKEWEDNORMALPDF(x1)** — `PRINT "SKEWEDNORMALPDF(0): "; FORMAT(STATS.SKEWEDNORMALPDF(0), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:153)
- **STATS.SKEWNESS(x1)** — `PRINT "Asimmetria (Skewness):"; MATH.ROUND(STATS.Skewness(DatasetSkew), 4)` (shared/samples/Analysis/AnalysisStatsDescriptive.bas:56)
- **STATS.SPEARMANR(x1, x2)** — `r = STATS.SPEARMANR(x, y)` (shared/samples/Math/StatsSciPyParityP0.bas:46)
- **STATS.STDDEV(x1)** — `PRINT "Dev. Standard (StdDev): "; MATH.ROUND(STATS.StdDev(Dataset1), 4)` (shared/samples/Analysis/AnalysisStatsDescriptive.bas:49)
- **STATS.STUDENTTCDF(x1, x2)** — `PRINT "STUDENTTCDF(0,5): "; FORMAT(STATS.STUDENTTCDF(0,5), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:142)
- **STATS.STUDENTTISF(x1, x2)** — `PRINT "tisf     ="; STATS.STUDENTTISF(0.025, 10)` (shared/samples/Math/StatsInverseSciPyParity.bas:11)
- **STATS.STUDENTTPDF(x1, x2)** — `PRINT "STUDENTTPDF(0,5): "; FORMAT(STATS.STUDENTTPDF(0,5), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:141)
- **STATS.STUDENTTPPF(x1, x2)** — `PRINT "tppf     ="; STATS.STUDENTTPPF(0.975, 10)` (shared/samples/Math/StatsInverseSciPyParity.bas:10)
- **STATS.STUDENTTSF(x1, x2)** — `PRINT "stud="; MATH.ROUND(STATS.StudentTSf(1.959964, 1000), 6)` (shared/minitests/stats/sf_extra.bas:4)
- **STATS.TTEST(x1, x2)** — `DIM result = STATS.TTEST(x, y)` (shared/samples/Analysis/StatsSciPyParity.bas:16)
- **STATS.TTESTONESAMPLE(x1, x2)** — `result = STATS.TTESTONESAMPLE(x, 5.0)` (shared/samples/Analysis/StatsSciPyParity.bas:20)
- **STATS.TTESTPAIRED(x1, x2)** — `result = STATS.TTESTPAIRED(x, paired)` (shared/samples/Analysis/StatsSciPyParity.bas:18)
- **STATS.TRIANGULARPDF(x1)** — `PRINT "TRIANGULARPDF(0.5): "; FORMAT(STATS.TRIANGULARPDF(0.5), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:151)
- **STATS.TRIANGULARPPF(x1)** — `PRINT "trianppf ="; STATS.TRIANGULARPPF(0.5, 0, 1, 0.5)` (shared/samples/Math/StatsInverseSciPyParity.bas:26)
- **STATS.UNIFORMPDF(x1)** — `PRINT "UNIFORMPDF(0.5):  "; FORMAT(STATS.UNIFORMPDF(0.5), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:152)
- **STATS.UNIFORMPPF(x1)** — `PRINT "unifppf  ="; STATS.UNIFORMPPF(0.5, 0, 1)` (shared/samples/Math/StatsInverseSciPyParity.bas:27)
- **STATS.VARIANCE(x1)** — `PRINT "Varianza (Variance):  "; MATH.ROUND(STATS.Variance(Dataset1), 4)` (shared/samples/Analysis/AnalysisStatsDescriptive.bas:48)
- **STATS.WEIBULLCDF(x1, x2, x3)** — `PRINT "WEIBULLCDF(1,2,3): "; FORMAT(STATS.WEIBULLCDF(1,2,3), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:144)
- **STATS.WEIBULLPDF(x1, x2, x3)** — `PRINT "WEIBULLPDF(1,2,3): "; FORMAT(STATS.WEIBULLPDF(1,2,3), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:143)
- **STATS.WEIBULLPPF(x1, x2, x3)** — `PRINT "weibppf  ="; STATS.WEIBULLPPF(0.8, 1.5, 2)` (shared/samples/Math/StatsInverseSciPyParity.bas:17)
- **STATS.WEIBULLSF(x1, x2, x3)** — `PRINT "weib="; MATH.ROUND(STATS.WeibullSf(1.0, 2, 1), 6)` (shared/minitests/stats/sf_weibull_f.bas:3)
- **STATS.WILCOXON(x1, x2)** — `r = STATS.WILCOXON(x, y)` (shared/samples/Math/StatsSciPyParityP0.bas:52)
- **STATS.ZTEST(x1, x2, x3)** — `result = STATS.ZTEST(x, 5.0, 2.0)` (shared/samples/Analysis/StatsSciPyParity.bas:32)
<!-- END DOMAIN TABLE -->


<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### Math-Guida-Utente

*Fonte: `Math-Guida-Utente`*


## 1. Arrotondamento e troncamento
| Funzione | Comportamento |
|---|---|
| `ROUND(x[, n])` | arrotonda a `n` decimali, **banker's** sui .5 (`ROUND(2.5)` → 2, `ROUND(3.5)` → 4, `ROUND(-3.5)` → -4) |
| `TRUNC(x)` | tronca verso zero: `TRUNC(-3.9)` → -3 |
| `INT(x)` / `FIX(x)` | troncano verso zero (coincidono nel canone: `INT(-3.9)` → -3; NON è `floor`) |
| `FLOOR(x)` / `CEILING(x)` | pavimento/soffitto: `FLOOR(-3.7)` → -4, `CEILING(-3.7)` → -3 |

PRINT ROUND(2.3456, 2)   ' 2.35
PRINT TRUNC(-3.9)        ' -3
PRINT INT(-3.9)          ' -3
PRINT FLOOR(-3.7)        ' -4
PRINT CEILING(-3.2)      ' -3


## 2. Valore assoluto e segno

PRINT ABS(-5)   ' 5
PRINT SGN(5)    ' 1
PRINT SGN(-5)   ' -1
PRINT SGN(0)    ' 0


## 3. Esponenziali, logaritmi e radici

PRINT EXP(0)    ' 1
PRINT LOG(1)    ' 0        (logaritmo naturale)
PRINT LOG10(1000) ' 3      (logaritmo decimale)
PRINT SQR(9)    ' 3        (radice quadrata)


## 4. Trigonometria

PRINT SIN(0)                 ' 0
PRINT COS(0)                 ' 1
PRINT ATN(1)                 ' 0.785398163397448  (π/4)
PRINT ROUND(DEG2RAD(180), 10) ' 3.1415926536
PRINT ROUND(RAD2DEG(ATN(1)*4), 6) ' 180
PRINT SINH(0)                ' 0
PRINT COSH(0)                ' 1


## 5. Statistiche scalari (variadic)

PRINT SUM(1, 2, 3)   ' 6
PRINT AVG(2, 4)      ' 3
PRINT AVG(1, 2, 3)   ' 2
PRINT MAX(5, 2)      ' 5
PRINT MIN(5, 2)      ' 2
PRINT CLAMP(15, 0, 10) ' 10


## 6. Combinatoria

PRINT COMB(5, 2)     ' 10
PRINT COMB(10, 3)    ' 120
PRINT PERM(5, 2)     ' 20
PRINT PERM(5, 0)     ' 1
PRINT FACT(5)        ' 120


## 7. Geometria e polari

PRINT PYTHAGORAS(3, 4)   ' 5
PRINT HYPOT(6, 8)        ' 10
PRINT DIST2D(0, 0, 3, 4) ' 5
PRINT ANGLE2D(0, 0, 1, 0) ' 0    (gradi)
PRINT ANGLE2D(0, 0, 0, 1) ' 90
PRINT POLAR_X(2, 0)      ' 2    (2·cos(0))
PRINT POLAR_Y(2, 0)      ' 0


## 8. Random (`RND` / `RANDOMIZE`)

PRINT RND()              ' numero pseudo-casuale in [0,1)
RANDOMIZE 42             ' semina il generatore
PRINT RND()              ' dopo lo stesso seed la sequenza è identica ovunque


## 9. Sequenze (`RANGE`)

DIM r = RANGE(1, 5)          ' [1, 2, 3, 4, 5]
DIM s = RANGE(0, 10, 2)      ' [0, 2, 4, 6, 8, 10]
DIM d = RANGE(10, 0, -5)     ' [10, 5, 0]   (passo negativo)
PRINT LEN(r)                 ' 5
PRINT r(0); " "; r(UBOUND(r)) ' 1 5


## 10. Verifiche sui quattro backend (2026-09)

## Vedi anche

### ABS-Guida-Utente

*Fonte: `ABS-Guida-Utente`*


## Sintassi

ABS(espressione)

| Parametro | Tipo | Descrizione |
|---|---|---|
| `espressione` | numerico · stringa numerica · `BIT`/`BOOL` · `CURRENCY` | valore di cui calcolare il modulo. **Un solo argomento** (0 o 2+ = errore) |

## Semantica (identica sui 4 backend)
| Argomento | Risultato |
|---|---|
| numero positivo / zero | invariato — `ABS(5)` = 5, `ABS(0)` = 0 |
| numero negativo (`INTEGER`/`LONG`/`DOUBLE`/`SINGLE`/`CURRENCY`) | modulo — `ABS(-5)` = 5, `ABS(-2.75)` = 2.75 |
| stringa **numerica** | coercizione e modulo — `ABS("-3.5")` = 3.5; stringa non numerica → 0 |
| `BIT`/`BOOL` | **TRUE = -1** (convenzione VMBASIC) → `ABS(TRUE)` = 1; FALSE → 0 |
| `NIL` | 0 |

## Disponibilità e parità (4/4)

## Riferimenti

### Analysis-Manual

*Fonte: `Analysis-Manual`*

| Namespace | Contenuto |
|---|---|
| `NUMERICAL` | FFT/IFFT/RFFT/IRFFT, ODE (`SolveOde`/`SolveIvp`), integrali, interpolazione/spline, derivazione |
| `TIMESERIES` | `Arima`, `HoltWinters`, `AdfTest`, `MaCoefficients`, `YuleWalker` |
| `MULTIVARIATE` | `Pca`, `KMeans` |
| `REGRESSION` | `Regress` |
| `STATS` | descrittive e test già presenti (`Mean`, `Median`, `Variance`, `Anova`, …) |
| `VECTOR` | `EuclideanDistanceSquared` |

## 1. Riferimento funzioni
| NOME | SINTASSI / ESEMPIO | DESCRIZIONE |
|---|---|---|
| `ADFTEST` | `TIMESERIES.AdfTest()` | Test Dickey-Fuller aumentato. |
| `ANOVA` | `STATS.Anova(groups)` | Analisi varianza. |
| `ARIMA` | `TIMESERIES.Arima(1, 1, 1)` | Modello ARIMA. |
| `CORRELATION` | `STATS.Correlation()` | Correlazione. |
| `CUBICSPLINE` | `NUMERICAL.CubicSpline(x, y, x_new)` | Spline cubica not-a-knot. |
| `CUMTRAPEZOID` | `NUMERICAL.CumTrapezoid(y, x)` | Integrale trapezoidale cumulativo. |
| `DIFFERENTIATE` | `NUMERICAL.Differentiate(1)` | Derivazione numerica. |
| `EUCLIDEANDISTANCESQUARED` | `VECTOR.EuclideanDistanceSquared()` | Distanza euclidea quadrata. |
| `FFT` | `NUMERICAL.Fft()` | Trasformata Fourier. |
| `FFTCOMPLEX` | `NUMERICAL.FftComplex()` | FFT complessa. |
| `FFTFREQ` | `NUMERICAL.FftFreq(n, d)` | Frequenze discrete FFT. |
| `FFTSHIFT` | `NUMERICAL.FftShift(x)` | Frequenza zero al centro. |
| `HOLTWINTERS` | `TIMESERIES.HoltWinters(1, 1, 1, 1)` | Previsione Holt-Winters. |
| `IFFT` | `NUMERICAL.Ifft()` | FFT inversa. |
| `IFFTCOMPLEX` | `NUMERICAL.IfftComplex()` | IFFT complessa. |
| `IFFTSHIFT` | `NUMERICAL.IfftShift(x)` | Inverso di FftShift. |
| `INTEGRATE` | `NUMERICAL.Integrate(1, 1)` | Integrazione numerica. |
| `INTERPOLATECLAMP` | `NUMERICAL.InterpolateClamp(1)` | Interp. clamp. |
| `INTERPOLATEGENERIC` | `NUMERICAL.InterpolateGeneric("s", 1)` | Interp. generica. |
| `INTERPOLATELINEAR` | `NUMERICAL.InterpolateLinear(1)` | Interpolazione lineare. |
| `INTERPOLATEPCHIP` | `NUMERICAL.InterpolatePchip(1)` | Interp. PCHIP. |
| `INTERPOLATEPOLYNOMIAL` | `NUMERICAL.InterpolatePolynomial(1)` | Interp. polinomiale. |
| `INTERPOLATESPLINE` | `NUMERICAL.InterpolateSpline(1)` | Interpolazione spline. |
| `INTERPOLATESTEP` | `NUMERICAL.InterpolateStep(1)` | Interp. a gradino. |
| `IRFFT` | `NUMERICAL.Irfft(re, im)` | FFT reale inversa. |
| `RFFT` | `NUMERICAL.Rfft(x)` | FFT reale (componenti non negative). |
| `SPLEV` | `NUMERICAL.Splev(spline, xq)` | Valuta spline cubica. |
| `SPLREP` | `NUMERICAL.Splrep(x, y)` | Spline cubica interpolante (not-a-knot). |
| `KMEANS` | `MULTIVARIATE.KMeans(1)` | Clustering K-means. |
| `KURTOSIS` | `STATS.Kurtosis()` | Curtosi. |
| `MACOEFFICIENTS` | `TIMESERIES.MaCoefficients(1)` | Coefficienti MA. |
| `MAD` | `STATS.Mad()` | Deviazione media assoluta. |
| `MEAN` | `STATS.Mean()` | Media aritmetica. |
| `MEDIAN` | `STATS.Median()` | Mediana. |
| `PCA` | `MULTIVARIATE.Pca()` | Analisi componenti principali. |
| `REGRESS` | `REGRESSION.Regress()` | Regressione lineare. |
| `SIMPSON` | `NUMERICAL.Simpson(fn, a, b, n)` | Simpson composita. |
| `SOLVEIVP` | `NUMERICAL.SolveIvp(fn, y0, t0, t1)` | ODE adattiva RK45 (scipy solve_ivp). |
| `SOLVEODE` | `NUMERICAL.SolveOde(fn, y0, t0, t1, s)` | Solutore ODE 1o ordine (RK4). |
| `SKEWNESS` | `STATS.Skewness()` | Asimmetria. |
| `STDDEV` | `STATS.StdDev()` | Deviazione standard. |
| `TRAPEZOID` | `NUMERICAL.Trapezoid(y, x)` | Integrale trapezoidale. |
| `VARIANCE` | `STATS.Variance()` | Varianza. |
| `YULEWALKER` | `TIMESERIES.YuleWalker(1)` | Stima Yule-Walker. |

## 2. Esempio completo

' scipy.integrate / scipy.interpolate / scipy.fft (ANALYSIS)
' Oracoli: quad/simpson x^2 = 1/3, trapezoid(y,x)=9.5,
'          cumulative_trapezoid=[0.5,3,9.5], solve_ivp e^-t,
'          CubicSpline not-a-knot @1.5 = 1.3125, fftfreq(6)
IMPORT ANALYSIS

SUB Main()
  DIM yy(3) AS DOUBLE = {0, 1, 4, 9}
  DIM xx(3) AS DOUBLE = {0, 1, 2, 3}
  DIM yy2(3) AS DOUBLE = {0, 0.5, 2, 1.5}

  PRINT "integrate   ="; NUMERICAL.Integrate(FN(x) => x * x, 0, 1)
  PRINT "simpson     ="; NUMERICAL.Simpson(FN(x) => x * x, 0, 1, 256)
  PRINT "trapezoid   ="; NUMERICAL.Trapezoid(yy, xx)
  DIM ct() = NUMERICAL.CumTrapezoid(yy, xx)
  PRINT "cumtrap     ="; ct(0); ct(1); ct(2)


## 3. Note e limitazioni

## 4. Verifiche sui backend (2026-09-07, MiniTests)

4.8               MEAN
5                 MEDIAN
3.3466401061363   STDDEV
11.2              VARIANCE
0.088041393098    SKEWNESS (ROUND 12)
-1.97544642857143 KURTOSIS
3                 MAD
-0.058600903867   CORRELATION (ROUND 12)


9.5               TRAPEZOID(y,x)      (integrale x² dx [0,3])
2.25              CUBICSPLINE(x,y,1.5)  (x² → 2.25)
6.5               INTERPOLATELINEAR(x,y,2.5)
4                 INTERPOLATESTEP(x,y,2.5)
6.25              INTERPOLATEPOLYNOMIAL(x,y,2.5)
6.21875           INTERPOLATEPCHIP(x,y,2.5)
9                 INTERPOLATECLAMP(x,y,5)  (clamp al bordo y(3))
6.5               INTERPOLATEGENERIC("linear",x,y,2.5)
40                EUCLIDEANDISTANCESQUARED(x,y)


### Tranche 2 — array-return (2026-09-07, commit `e3f63fd0`)

0.5 3 9.5                CUMTRAPEZOID(y,x)        (cumulative_trapezoid scipy)
0 0.1666... 0.3333...    FFTFREQ(6,1) (0..5)      (frequenze, ultime 3 negative)
-0.5 -0.3333... -0.1666...
2 3 0 1                  FFTSHIFT(x)
2 3 0 1                  IFFTSHIFT(x)
4 1                      YULEWALKER(y, 2)
0.25 -0.3                MACOEFFICIENTS(x, 2)


### Tranche 3 — integrazione/ODE con λ (2026-09-07, commit dell'altra AI `30eb8ef16` + commit ANALYSIS)

0.333333333333333   INTEGRATE(FN(x)=>x*x, 0, 1)     (scipy quad x²=1/3)
0.333333333333333   SIMPSON(FN(x)=>x*x, 0, 1, 256)  (Simpson composita)
0.367879774412498   SOLVEODE({|t,y|RETURN -y},1,0,1,10)[10]  (e^-t, y(1))


### Tranche 4 — result-UDT: REGRESS (2026-09-07)

1.96              r.Slope      (REGRESS(x,y) — pendenza)
0.2               r.Intercept
0.99968777644033  r.R2


DIM r AS REGRESSION.RegressionResult
r = REGRESSION.Regress(x, y)
PRINT r.Slope; r.Intercept; r.R2


### BITS-Guida-Utente

*Fonte: `BITS-Guida-Utente`*


## 1. Convenzioni (valgono per tutte le funzioni)

### Convenzione numerica BIT/BOOL (TRUE = -1, FALSE = 0)
| Regola | Valore |
|---|---|
| Indici dei bit | **0-based**, `0` = bit meno significativo (LSB) |
| `n` fuori da `[0, 63]` | `BITGET`/`BITTEST` → `0`; `BITSET`/`BITCLEAR`/`BITTOGGLE` → `x` invariato |
| `width` (BITREVERSE/rotazioni) | `<= 0` → `0`; `> 64` → clampato a `64` |
| `lo`/`hi` invertiti (BITFIELD) | vengono **scambiati**; clampati a `[0,63]` |
| Predicati (`BITTEST`, `ISPOW2`) | convenzione **Bit**: `-1` = vero, `0` = falso |
| Interi | tutti i valori sono a 64 bit con segno (i numerici BASIC sono `double`, convertiti) |

## 2. Tier 1 — operazioni elementari
| Funzione | Sintassi | Ritorna | Esempio |
|---|---|---|---|
| `BITCOUNT` | `BITS.BITCOUNT(x)` | numero di bit a 1 (0..64) | `BITS.BITCOUNT(240)` → `4` |
| `BITGET` | `BITS.BITGET(x, n)` | `0`/`1` | `BITS.BITGET(240, 4)` → `1` |
| `BITSET` | `BITS.BITSET(x, n)` | `x` con il bit `n` posto a 1 | `BITS.BITSET(0, 3)` → `8` |
| `BITCLEAR` | `BITS.BITCLEAR(x, n)` | `x` con il bit `n` azzerato | `BITS.BITCLEAR(255, 0)` → `254` |
| `BITTOGGLE` | `BITS.BITTOGGLE(x, n)` | `x` con il bit `n` invertito | `BITS.BITTOGGLE(255, 0)` → `254` |
| `BITTEST` | `BITS.BITTEST(x, n)` | Bit `-1`/`0` | `BITS.BITTEST(8, 3)` → `-1` |

## 3. Tier 2 — campi, rotazioni, codifiche
| Funzione | Sintassi | Ritorna | Esempio |
|---|---|---|---|
| `BITFIELD` | `BITS.BITFIELD(x, lo, hi)` | bit `lo..hi` **inclusi**, allineati a destra | `BITS.BITFIELD(45, 1, 3)` → `6` |
| `BITREVERSE` | `BITS.BITREVERSE(x, width)` | ordine dei `width` bit bassi invertito | `BITS.BITREVERSE(1, 4)` → `8` |
| `BITROTATEL` | `BITS.BITROTATEL(x, n, width)` | rotazione sinistra su `width` bit | `BITS.BITROTATEL(1, 1, 4)` → `2` |
| `BITROTATER` | `BITS.BITROTATER(x, n, width)` | rotazione destra su `width` bit | `BITS.BITROTATER(1, 1, 4)` → `8` |
| `GRAYCODE` | `BITS.GRAYCODE(x)` | `x XOR (x >> 1)` | `BITS.GRAYCODE(5)` → `7` |
| `FROMGRAY` | `BITS.FROMGRAY(x)` | inversa del codice Gray | `BITS.FROMGRAY(7)` → `5` |
| `PARITY` | `BITS.PARITY(x)` | `0` = numero pari di bit a 1, `1` = dispari | `BITS.PARITY(7)` → `1` |
| `BITSCANFWD` | `BITS.BITSCANFWD(x)` | indice del bit a 1 **più basso**, `-1` se `x = 0` | `BITS.BITSCANFWD(8)` → `3` |
| `BITSCANREV` | `BITS.BITSCANREV(x)` | indice del bit a 1 **più alto**, `-1` se `x = 0` | `BITS.BITSCANREV(8)` → `3` |
| `BYTESWAP` | `BITS.BYTESWAP(x)` | ordine dei 8 byte invertito (endianness) | `BITS.BYTESWAP(256)` → `281474976710656` |

## 4. Tier 3 — algoritmi
| Funzione | Sintassi | Ritorna | Esempio |
|---|---|---|---|
| `HAMMINGDIST` | `BITS.HAMMINGDIST(a, b)` | numero di bit diversi | `BITS.HAMMINGDIST(0, 7)` → `3` |
| `BITINTERLEAVE` | `BITS.BITINTERLEAVE(a, b)` | codice di **Morton**: bit di `a` in posizioni pari, `b` in dispari | `BITS.BITINTERLEAVE(3, 0)` → `5` |
| `BITDEINTERLEAVE` | `BITS.BITDEINTERLEAVE(m, which)` | inversa: `which = 0` bit pari, `1` bit dispari | `BITS.BITDEINTERLEAVE(5, 0)` → `3` |
| `NEXTPOW2` | `BITS.NEXTPOW2(x)` | più piccola potenza di 2 `>= x` (`x <= 1` → `1`) | `BITS.NEXTPOW2(1000)` → `1024` |
| `ISPOW2` | `BITS.ISPOW2(x)` | Bit `-1` se potenza di 2 (e `> 0`) | `BITS.ISPOW2(8)` → `-1` |

## 5. Esempio completo e complesso (verificato 5/5)
| # | Scenario | Funzioni | Proprietà verificata |
|---|---|---|---|
| 1 | bitmask di permessi | `BITSET` `BITCLEAR` `BITTOGGLE` `BITTEST` `BITGET` `BITCOUNT` | conteggio/lettura coerenti dopo concessioni e revoche |
| 2 | header impacchettato `[15..8]ver [7..4]tipo [3..0]flag` | `BITFIELD` | round-trip dei campi su una tabella di 4 header + campo con estremi invertiti |
| 3 | integrità dei pacchetti | `PARITY` `HAMMINGDIST` | somma delle parità e somma delle distanze di Hamming |
| 4 | contatore Gray su 0..15 | `GRAYCODE` `FROMGRAY` `HAMMINGDIST` | round-trip **e** invariante "codici adiacenti differiscono di 1 bit" (0 violazioni) |
| 5 | griglia 4×4 → indice lineare | `BITINTERLEAVE` `BITDEINTERLEAVE` | round-trip su tutte le 16 celle (0 errori) |
| 6 | cifrario a rotazione + endianness | `BITROTATEL` `BITROTATER` `BITREVERSE` `BYTESWAP` | cifratura/decifratura round-trip e `BYTESWAP` applicato due volte = identità |
| 7 | capacità e allineamento | `NEXTPOW2` `ISPOW2` `BITSCANFWD` `BITSCANREV` | `ISPOW2(NEXTPOW2(n))` vero, indici LSB/MSB, `BITSCAN*` su 0 → `-1` |

DIM viol AS LONG
viol = 0
FOR k = 0 TO 14
  IF BITS.FROMGRAY(BITS.GRAYCODE(k)) <> k THEN viol = viol + 1
  IF BITS.HAMMINGDIST(BITS.GRAYCODE(k), BITS.GRAYCODE(k + 1)) <> 1 THEN viol = viol + 1
NEXT
PRINT "gray_viol="; viol          ' 0


## 6. Verifica

## 7. Note e limiti

### Finance-Guida-Utente

*Fonte: `Finance-Guida-Utente`*


## 1. Panoramica

IMPORT FINANCE
SUB Main()
    PRINT FINANCE.PMT(0.01, 12, 1000)
END SUB

| Famiglia | Funzioni |
|---|---|
| Rendite e prestiti | `PMT`, `PMTEXT`, `FV`, `FVEXT`, `PV`, `PVEXT`, `NPER`, `NPEREXT`, `RATE` |
| Ammortamenti | `SLN`, `SYD`, `DB`, `DDB`, `CUMIPMT`, `CUMPRINC` |
| Flussi di cassa | `NPV`, `IRR`, `MIRR`, `MNPV`, `XNPV`, `XIRR` |
| Convenzioni di data | `DAYS360`, `YEARFRAC` |

## 2. Rendite e prestiti

### 2.1 PMT — rata periodica

rata = FINANCE.PMT(rate, nper, pv [, fv] [, type])


IMPORT FINANCE
SUB Main()
    PRINT MATH.ROUND(FINANCE.PMT(0.01, 12, 1000), 2)          ' rateo mensile
    PRINT MATH.ROUND(FINANCE.PMT(0.01, 12, 1000, 0, 1), 2)    ' inizio periodo
END SUB


 -88.85
 -87.97


### 2.2 FV / PV — valore futuro e attuale

fv = FINANCE.FV(rate, nper, pmt [, pv] [, type])    ' capitale finale
pv = FINANCE.PV(rate, nper, pmt [, fv] [, type])    ' valore oggi


SUB Main()
    PRINT MATH.ROUND(FINANCE.FV(0.01, 12, -100, 0), 2)      ' 1268.25
    PRINT MATH.ROUND(FINANCE.PV(0.01, 12, -100, 0), 2)      ' 1125.51
END SUB


### 2.3 NPER / NPEREXT — numero di periodi

n = FINANCE.NPER(rate, pmt, pv)                  ' periodi per estinguere
n = FINANCE.NPEREXT(rate, pmt, pv [, fv] [, type])


SUB Main()
    PRINT MATH.ROUND(FINANCE.NPER(0.01, -100, 1000), 6)      ' 10.588644
END SUB


### 2.4 RATE — tasso periodico

tasso = FINANCE.RATE(nper, pmt, pv [, fv] [, type] [, guess])


SUB Main()
    PRINT MATH.ROUND(FINANCE.RATE(12, -100, 1000), 6)        ' 0.029229
END SUB


### 2.5 Varianti EST

a = FINANCE.PMTEXT(rate, nper, pv, fv, type)
b = FINANCE.FVEXT (rate, nper, pmt, pv, type)
c = FINANCE.PVEXT (rate, nper, pmt, fv, type)


## 3. Ammortamenti

### 3.1 SLN — lineare

quota = FINANCE.SLN(cost, salvage, life)


SUB Main()
    PRINT FINANCE.SLN(1000, 100, 3)      ' 300
END SUB


### 3.2 SYD — somma degli anni

quota = FINANCE.SYD(cost, salvage, life, period)


SUB Main()
    PRINT FINANCE.SYD(1000, 100, 3, 1)   ' 450
    PRINT FINANCE.SYD(1000, 100, 3, 2)   ' 300
    PRINT FINANCE.SYD(1000, 100, 3, 3)   ' 150
END SUB


### 3.3 DB / DDB — declinanti

q = FINANCE.DB (cost, salvage, life, period [, month])   ' DB (month default 12)
q = FINANCE.DDB(cost, salvage, life, period [, factor])  ' DDB (factor default 2)


SUB Main()
    PRINT FINANCE.DB(10000, 1000, 5, 1, 12)      ' 3690
    PRINT FINANCE.DDB(10000, 1000, 5, 1, 2)      ' 4000
END SUB


### 3.4 CUMIPMT / CUMPRINC — cumulati tra due periodi

interessi = FINANCE.CUMIPMT(rate, nper, pv, startPeriod, endPeriod, type)
capitale  = FINANCE.CUMPRINC(rate, nper, pv, startPeriod, endPeriod, type)


## 4. Flussi di cassa

### 4.1 NPV — valore attuale netto

v = FINANCE.NPV(rate, flusso1, flusso2, ...)   ' valori sparsi
v = FINANCE.NPV(rate, Flussi)                  ' oppure array


IMPORT FINANCE
SUB Main()
    PRINT MATH.ROUND(FINANCE.NPV(0.1, 3000, 4200, 6800), 2)          ' 11307.29
    DIM F(3) AS DOUBLE = {-10000, 3000, 4200, 6800}
    PRINT MATH.ROUND(FINANCE.NPV(0.1, F), 2)                          ' 1188.44
END SUB


### 4.2 IRR / MIRR — tassi interni

t = FINANCE.IRR(Flussi [, guess])
t = FINANCE.MIRR(Flussi, financeRate, reinvestRate)


SUB Main()
    DIM F(3) AS DOUBLE = {-10000, 3000, 4200, 6800}
    PRINT MATH.ROUND(FINANCE.IRR(F), 6)              ' 0.163406
    PRINT MATH.ROUND(FINANCE.MIRR(F, 0.05, 0.08), 6) ' 0.140507
END SUB


### 4.3 MNPV — NPV modificato

v = FINANCE.MNPV(rate, Flussi)


### 4.4 XNPV / XIRR — con date non periodiche

v = FINANCE.XNPV(rate, Flussi, DateFlussi)
t = FINANCE.XIRR(Flussi, DateFlussi [, guess])


IMPORT FINANCE
IMPORT DATETIME AS DT
SUB Main()
    DIM F(3) AS DOUBLE = {-10000, 3000, 4200, 6800}
    DIM D(3) AS DATETIME
    D(0) = DT.DATEVALUE("2023-01-01")
    D(1) = DT.DATEVALUE("2024-01-01")
    D(2) = DT.DATEVALUE("2025-01-01")
    D(3) = DT.DATEVALUE("2026-01-01")
    PRINT MATH.ROUND(FINANCE.XNPV(0.1, F, D), 2)     ' 1305.05
    PRINT MATH.ROUND(FINANCE.XIRR(F, D), 6)          ' 0.11165
END SUB


## 5. Convenzioni di data

### 5.1 DAYS360 — giorni su base 30/360

giorni = FINANCE.DAYS360(start, end [, european])


SUB Main()
    PRINT FINANCE.DAYS360(DATETIME.DATEVALUE("2024-01-01"), DATETIME.DATEVALUE("2024-01-31"))   ' 30
END SUB


### 5.2 YEARFRAC — frazione di anno

frac = FINANCE.YEARFRAC(start, end [, basis])


SUB Main()
    s = DATETIME.DATEVALUE("2024-01-01")
    e = DATETIME.DATEVALUE("2024-04-01")
    PRINT MATH.ROUND(FINANCE.YEARFRAC(s, e, 0), 4)    ' 0.25
    PRINT MATH.ROUND(FINANCE.YEARFRAC(s, e, 1), 6)    ' 0.248634
END SUB


## 6. Note e limiti

### Funzioni-Matematiche-Guida-Utente

*Fonte: `Funzioni-Matematiche-Guida-Utente`*


## Sintassi

ABS(espressione)
SQR(espressione)      ' radice quadrata
EXP(espressione)      ' e^x
LOG(espressione)      ' logaritmo naturale
LOG10(espressione)    ' logaritmo base 10
SIN(espressione)      ' seno (radianti)
COS(espressione)      ' coseno (radianti)
TAN(espressione)      ' tangente (radianti)
ATN(espressione)      ' arcotangente

| Parametro | Tipo | Descrizione |
|---|---|---|
| `espressione` | numerico · stringa numerica · `BIT`/`BOOL` · `CURRENCY` | argomento della funzione. **Un solo argomento** (0 o 2+ = errore) |

## Semantica (identica sui 4 backend)
| Argomento | Comportamento |
|---|---|
| numerico (`INTEGER`/`LONG`/`DOUBLE`/`SINGLE`/`CURRENCY`) | funzione applicata; risultato `DOUBLE` |
| stringa **numerica** | coercizione esplicita poi funzione — `SQR("4")` = 2, `LOG("1")` = 0, `EXP("0")` = 1 |
| stringa non numerica | 0 (prima della funzione) |
| `BIT`/`BOOL` | **TRUE = -1**, FALSE = 0 (convenzione VMBASIC) — `ABS(TRUE)` = 1 |
| `NIL` | 0 |

## Famiglia intera (INT / FIX / FLOOR / TRUNC / CEILING)

INT(x)       ' intero <= x  (floor)
FLOOR(x)     ' intero <= x  (sinonimo di INT)
FIX(x)       ' parte intera, troncata verso zero
TRUNC(x)     ' parte intera, troncata verso zero (sinonimo di FIX)
CEILING(x)   ' intero >= x


## Disponibilità e parità (4/4)

## Riferimenti

### KdTree-Manual

*Fonte: `KdTree-Manual`*


## 1. Riferimento funzioni
| NOME | SINTASSI / ESEMPIO | DESCRIZIONE |
|---|---|---|
| `CREATE` | `KDTREE.CREATE(points, m)` | Costruisce kd-tree (handle). |
| `QUERY` | `KDTREE.QUERY(h, x, k)` | Indici dei k vicini. |
| `QUERYBALL` | `KDTREE.QUERYBALL(h, x, r)` | Indici entro raggio r. |
| `QUERYDIST` | `KDTREE.QUERYDIST(h, x, k)` | Distanze dei k vicini. |

## 2. Esempio completo

' scipy.spatial.KDTree: costruzione (punti FLAT + m), query k-NN, query_ball_point
' Oracoli: query([4,4],k=3) -> d=[1,sqrt2,sqrt5], i=[1,7,0]
IMPORT KDTREE

SUB Main()
  DIM flat(19) AS DOUBLE = {2,3, 5,4, 9,6, 4,7, 8,1, 7,2, 1,8, 3,3, 6,5, 0,6}
  h = KDTREE.CREATE(flat, 2)

  DIM x(1) AS DOUBLE = {4, 4}
  DIM q() = KDTREE.QUERY(h, x, 3)
  PRINT "query   ="; q(0); q(1); q(2)
  DIM qd() = KDTREE.QUERYDIST(h, x, 3)
  PRINT "qdist   ="; qd(0); qd(1); qd(2)
  DIM b() = KDTREE.QUERYBALL(h, x, 2)
  PRINT "ball    ="; b(0); b(1)
END SUB


## 3. Note e limitazioni

## 4. MiniTest e Sample

### Matrix-Linalg-Manual

*Fonte: `Matrix-Linalg-Manual`*

| Namespace | Responsabilità | Esempi |
|---|---|---|
| `MATRIX` | struttura, accesso e comandi sulle matrici: creazione, lettura/scrittura, copia, slicing, concatenazione, prodotti, predicati, I/O | `CREATE`, `GETVALUE`, `SETVALUE`, `MULTIPLY`, `TRANSPOSE`, `ROWCOUNT` |
| `LINALG` | algoritmi di algebra lineare: decomposizioni, risolutori, norme, funzioni matriciali | `SVD`, `EIG`, `LU`, `QR`, `SOLVE`, `INVERSE`, `NORM`, `EXPM` |

## 1. Riferimento funzioni
| NOME | SINTASSI / ESEMPIO | DESCRIZIONE |
|---|---|---|
| `ABOVEMASK` | `MATRIX.ABOVEMASK(1)` | ABOVEMASK di MATRIX. |
| `ABOVEMATRIX` | `MATRIX.ABOVEMATRIX(1)` | ABOVEMATRIX di MATRIX. |
| `ADDCOLVECTOR` | `MATRIX.ADDCOLVECTOR(42)` | ADDCOLVECTOR di MATRIX. |
| `ADDROWVECTOR` | `MATRIX.ADDROWVECTOR(42)` | ADDROWVECTOR di MATRIX. |
| `ARGMAX` | `MATRIX.ARGMAX()` | ARGMAX di MATRIX. |
| `ARGMIN` | `MATRIX.ARGMIN()` | ARGMIN di MATRIX. |
| `ASCSV` | `MATRIX.ASCSV()` | ASCSV di MATRIX. |
| `ASJSON` | `MATRIX.ASJSON()` | ASJSON di MATRIX. |
| `CHOLESKY` | `LINALG.CHOLESKY(1)` | CHOLESKY di LINALG. |
| `CLONEMATRIX` | `MATRIX.CLONEMATRIX()` | CLONEMATRIX di MATRIX. |
| `COLMEANS` | `MATRIX.COLMEANS()` | COLMEANS di MATRIX. |
| `COLSUMS` | `MATRIX.COLSUMS()` | COLSUMS di MATRIX. |
| `COLUMNCOUNT` | `MATRIX.COLUMNCOUNT()` | COLUMNCOUNT di MATRIX. |
| `COLVARIANCES` | `MATRIX.COLVARIANCES()` | COLVARIANCES di MATRIX. |
| `CONCATCOLS` | `MATRIX.CONCATCOLS(1)` | CONCATCOLS di MATRIX. |
| `CONCATROWS` | `MATRIX.CONCATROWS(1)` | CONCATROWS di MATRIX. |
| `CREATE` | `MATRIX.CREATE(1, 1)` | CREATE di MATRIX. |
| `DETERMINANT` | `LINALG.DETERMINANT(1)` | DETERMINANT di LINALG. |
| `EVD` | `LINALG.EVD(1)` | EVD di LINALG. |
| `FACTOR` | `LINALG.FACTOR(decompositionobject, "s")` | FACTOR di LINALG. |
| `FROBENIUSNORM` | `LINALG.FROBENIUSNORM()` | FROBENIUSNORM di LINALG. |
| `INFINITYNORM` | `LINALG.INFINITYNORM()` | INFINITYNORM di LINALG. |
| `INVERSE` | `LINALG.INVERSE(1)` | INVERSE di LINALG. |
| `ISLOWERTRIANGULAR` | `MATRIX.ISLOWERTRIANGULAR()` | ISLOWERTRIANGULAR di MATRIX. |
| `ISORTHOGONAL` | `MATRIX.ISORTHOGONAL()` | ISORTHOGONAL di MATRIX. |
| `ISSYMMETRIC` | `MATRIX.ISSYMMETRIC()` | ISSYMMETRIC di MATRIX. |
| `ISUPPERTRIANGULAR` | `MATRIX.ISUPPERTRIANGULAR()` | ISUPPERTRIANGULAR di MATRIX. |
| `LOADCSV` | `MATRIX.LOADCSV("file.txt")` | LOADCSV di MATRIX. |
| `LOADJSON` | `MATRIX.LOADJSON("file.txt")` | LOADJSON di MATRIX. |
| `LU` | `LINALG.LU(1)` | LU di LINALG. |
| `MATRIX` | `MATRIX MATRIX` | MATRIX di MATRIX. |
| `MAXVALUE` | `MATRIX.MAXVALUE()` | MAXVALUE di MATRIX. |
| `MINVALUE` | `MATRIX.MINVALUE()` | MINVALUE di MATRIX. |
| `MULTIPLY` | `MATRIX.MULTIPLY(1, 1)` | MULTIPLY di MATRIX. |
| `MULTIPLYLEFTVECTOR` | `MATRIX.MULTIPLYLEFTVECTOR(42)` | MULTIPLYLEFTVECTOR di MATRIX. |
| `MULTIPLYVECTOR` | `MATRIX.MULTIPLYVECTOR(42)` | MULTIPLYVECTOR di MATRIX. |
| `NORM` | `LINALG.NORM(1)` | NORM di LINALG. |
| `ONENORM` | `LINALG.ONENORM()` | ONENORM di LINALG. |
| `PINVERSE` | `LINALG.PINVERSE(1)` | PINVERSE di LINALG. |
| `QR` | `LINALG.QR(1)` | QR di LINALG. |
| `RANK` | `LINALG.RANK(1)` | RANK di LINALG. |
| `RESHAPE` | `MATRIX.RESHAPE(1, 1)` | RESHAPE di MATRIX. |
| `ROWCOUNT` | `MATRIX.ROWCOUNT()` | ROWCOUNT di MATRIX. |
| `ROWMEANS` | `MATRIX.ROWMEANS()` | ROWMEANS di MATRIX. |
| `ROWSUMS` | `MATRIX.ROWSUMS()` | ROWSUMS di MATRIX. |
| `ROWVARIANCES` | `MATRIX.ROWVARIANCES()` | ROWVARIANCES di MATRIX. |
| `SLICE` | `MATRIX.SLICE(1, 1, 1, 1)` | SLICE di MATRIX. |
| `SOLVE` | `LINALG.SOLVE(1, 1)` | SOLVE di LINALG. |
| `SVD` | `LINALG.SVD(1)` | SVD di LINALG. |
| `TRACE` | `MATRIX.TRACE(1)` | TRACE di MATRIX. |
| `TRANSPOSE` | `MATRIX.TRANSPOSE(1)` | TRANSPOSE di MATRIX. |
| `EXPM` | `LINALG.EXPM(h)` | Esponenziale di matrice (handle). |
| `KRONECKER` | `MATRIX.KRONECKER(h1, h2)` | Prodotto di Kronecker A⊗B. |
| `SQRTM` | `LINALG.SQRTM(h)` | Radice quadrata di matrice, scipy.linalg.sqrtm. |
| `LOGM` | `LINALG.LOGM(h)` | Logaritmo naturale di matrice, scipy.linalg.logm. |
| `HESSENBERG` | `LINALG.HESSENBERG(h)` | Forma di Hessenberg superiore (Householder). |
| `BLOCKDIAG` | `MATRIX.BLOCKDIAG(h1, h2)` | Matrice diagonale a blocchi, scipy.linalg.block_diag. |
| `NULLSPACE` | `LINALG.NULLSPACE(h)` | Base del kernel, scipy.linalg.null_space. |
| `SETVALUE` | `MATRIX.SETVALUE(h, r, c, v)` | Scrive v alla riga r, colonna c (0-based). |
| `GETVALUE` | `MATRIX.GETVALUE(h, r, c)` | Legge l'elemento riga r, colonna c (0-based). |
| `COSM` | `LINALG.COSM(h)` | Coseno di matrice, scipy.linalg.cosm. |
| `SINM` | `LINALG.SINM(h)` | Seno di matrice, scipy.linalg.sinm. |
| `TANM` | `LINALG.TANM(h)` | Tangente di matrice, scipy.linalg.tanm. |
| `SIGNM` | `LINALG.SIGNM(h)` | Segno di matrice, scipy.linalg.signm. |
| `FRACTIONALPOWER` | `LINALG.FRACTIONALPOWER(h, p)` | Potenza frazionaria, scipy.linalg.fractional_matrix_power. |
| `ORTH` | `LINALG.ORTH(h)` | Base ortonormale del range, scipy.linalg.orth. |
| `SUBSPACEANGLES` | `LINALG.SUBSPACEANGLES(h1, h2)` | Angoli tra sottospazi, scipy.linalg.subspace_angles. |
| `POLAR` | `LINALG.POLAR(h)` | Decomp. polare A=U·P (fattori U/P), scipy.linalg.polar. |
| `COND` | `LINALG.COND(h)` | Numero di condizionamento (norma 2). |
| `NUCLEARNORM` | `LINALG.NUCLEARNORM(h)` | Norma nucleare (somma valori singolari). |
| `TOEPLITZ` | `MATRIX.TOEPLITZ(c[], r[])` | Matrice di Toeplitz, scipy.linalg.toeplitz. |
| `HANKEL` | `MATRIX.HANKEL(c[], r[])` | Matrice di Hankel, scipy.linalg.hankel. |
| `CIRCULANT` | `MATRIX.CIRCULANT(c[])` | Matrice circolante, scipy.linalg.circulant. |
| `SOLVETRIANGULAR` | `LINALG.SOLVETRIANGULAR(hA, b[], lower)` | Risolve sistema triangolare. |
| `EIG` | `LINALG.EIG(h)` | Autovalori/autovettori generali (fattori DREAL/DIMAG/VRREAL/VRIMAG). |
| `SCHUR` | `LINALG.SCHUR(h)` | Real Schur form (fattori T/Z). |

## 2. Esempio completo

' scipy.linalg: sqrtm, logm, hessenberg, block_diag (oracolo scipy 1.18)

SUB Main()
  ' A = [[2,1],[1,2]]
  DIM h = MATRIX.CREATE(2, 2)
  h = MATRIX.SETVALUE(h, 0, 0, 2) : h = MATRIX.SETVALUE(h, 0, 1, 1)
  h = MATRIX.SETVALUE(h, 1, 0, 1) : h = MATRIX.SETVALUE(h, 1, 1, 2)

  DIM s = LINALG.SQRTM(h)                    ' sqrtm diag 1.3660
  PRINT "sqrtm00  ="; MATRIX.GETVALUE(s, 0, 0)
  DIM l = LINALG.LOGM(h)                     ' logm diag 0.5493
  PRINT "logm11   ="; MATRIX.GETVALUE(l, 1, 1)

  ' Hessenberg di una 3x3: Hh[2,0] deve essere ~0
  DIM h3 = MATRIX.CREATE(3, 3)
  h3 = MATRIX.SETVALUE(h3, 0, 0, 1) : h3 = MATRIX.SETVALUE(h3, 0, 1, 2) : h3 = MATRIX.SETVALUE(h3, 0, 2, 3)


## 3. Note e limitazioni

## 4. Parità backend (2026-09-12)

## 5. Sessione 2026-09-16 — blocco VALIDATO DEFINITIVO

### Optimize-Manual

*Fonte: `Optimize-Manual`*


## 1. Riferimento funzioni
| NOME | SINTASSI / ESEMPIO | DESCRIZIONE |
|---|---|---|
| `NEWTON` | `OPTIMIZE.NEWTON(fn, guess)` | Radice Newton -> DOUBLE. |
| `BRENT` | `OPTIMIZE.BRENT(fn, lo, hi)` | Radice Brent -> DOUBLE. |
| `SIMPLEX` | `OPTIMIZE.SIMPLEX(c, A, b)` | Prog. lineare (max c·x, A·x ≤ b) -> ARRAY `[x..., obj]`. |
| `BROYDEN` | `OPTIMIZE.BROYDEN(fn, init)` | Radice Broyden -> ARRAY. |
| `NELDERMEAD` | `OPTIMIZE.NELDERMEAD(fn, init)` | Minim. Nelder-Mead -> ARRAY. |
| `BISECTION` | `OPTIMIZE.BISECTION(fn, a, b)` | Radice bisezione -> DOUBLE. |
| `SECANT` | `OPTIMIZE.SECANT(fn, x0, x1)` | Radice secante -> DOUBLE. |
| `LEASTSQUARES` | `OPTIMIZE.LEASTSQUARES(A, b)` | Minimi quadrati -> ARRAY. |
| `CHOLESKYSOLVE` | `OPTIMIZE.CHOLESKYSOLVE(A, b)` | Soluzione Cholesky -> ARRAY. |
| `GOLDENSECTION` | `OPTIMIZE.GOLDENSECTION(fn, a, b)` | Min. sezione aurea -> DOUBLE. |
| `BFGS` | `OPTIMIZE.BFGS(fn, init)` | Minim. BFGS -> ARRAY. |
| `LBFGS` | `OPTIMIZE.LBFGS(fn, init)` | Minim. L-BFGS -> ARRAY. |
| `CONJUGATEGRADIENT` | `OPTIMIZE.CONJUGATEGRADIENT(fn, x0)` | Min. gradiente coniugato -> ARRAY. |
| `CURVEFIT` | `OPTIMIZE.CURVEFIT(fn, x, y, p0)` | Fit non lineare (curve_fit). |
| `DIFFERENTIALEVOLUTION` | `OPTIMIZE.DIFFERENTIALEVOLUTION(fn, b)` | Ottimiz. globale (DE). |
| `LEVENBERGMARQUARDT` | `OPTIMIZE.LEVENBERGMARQUARDT(fn, p0, x, y)` | Fit L-M (residui scalari) -> ARRAY. |
| `LEVENBERGMARQUARDTVEC` | `OPTIMIZE.LEVENBERGMARQUARDTVEC(fn, p0, x, y)` | Fit L-M vettoriale -> ARRAY. |
| `BRENTQ` | `OPTIMIZE.BRENTQ(fn, a, b)` | Radice Brent (bracketed) -> DOUBLE. |
| `BRENTH` | `OPTIMIZE.BRENTH(fn, a, b)` | Radice Brent/secante iperbolica -> DOUBLE. |
| `RIDDER` | `OPTIMIZE.RIDDER(fn, a, b)` | Radice di Ridder -> DOUBLE. |
| `TOMS748` | `OPTIMIZE.TOMS748(fn, a, b)` | Radice Alefeld-Potra-Shi -> DOUBLE. |
| `BRENTMIN` | `OPTIMIZE.BRENTMIN(fn, a, b)` | Minimizzatore scalare (brent) -> DOUBLE. |
| `MINIMIZESCALAR` | `OPTIMIZE.MINIMIZESCALAR(fn, a, b, "m")` | minimize_scalar (brent/golden/bounded). |
| `POWELL` | `OPTIMIZE.POWELL(fn, x0)` | Min. direzioni coniugate (powell). |
| `TNC` | `OPTIMIZE.TNC(fn, x0, bounds)` | Min. Newton troncato con bounds (tnc). |
| `COBYLA` | `OPTIMIZE.COBYLA(fn, x0, bounds)` | Min. approssimazione lineare (cobyla). |
| `SLSQP` | `OPTIMIZE.SLSQP(fn, x0, bounds)` | Min. SLSQP (SQP proiettato). |
| `MINIMIZE` | `OPTIMIZE.MINIMIZE(fn, x0, "m"[, bounds])` | Dispatcher minimize (nelder-mead/powell/cg/bfgs/lbfgs/...). |
| `FSOLVE` | `OPTIMIZE.FSOLVE(fn, x0)` | Radice sistema non lineare (fsolve). |
| `LEASTSQUARESFIT` | `OPTIMIZE.LEASTSQUARESFIT(fn, x0[, bounds])` | least_squares nonlineare con bounds. |
| `LINPROG` | `OPTIMIZE.LINPROG(c, A_ub, b_ub[, A_eq, b_eq, bounds])` | Programmazione lineare (two-phase). |
| `BRUTE` | `OPTIMIZE.BRUTE(fn, ranges, N)` | Griglia esaustiva (brute). |
| `BASINHOPPING` | `OPTIMIZE.BASINHOPPING(fn, x0, niter)` | Minimo globale (basinhopping). |
| `DUALANNEALING` | `OPTIMIZE.DUALANNEALING(fn, bounds, maxiter)` | Ricottura simulata (dual_annealing). |

## 2. Esempio completo

' scipy.optimize: radici, min scalar, minimize, fsolve, lstsq, linprog, globali
IMPORT OPTIMIZE

SUB Main()
  ' --- radici scalari (return DOUBLE) ---
  PRINT "brentq   ="; OPTIMIZE.BRENTQ(FN(x AS DOUBLE) AS DOUBLE => x * x - 2, 0, 2)
  PRINT "brenth   ="; OPTIMIZE.BRENTH(FN(x AS DOUBLE) AS DOUBLE => x * x * x - 27, 2, 4)
  PRINT "ridder   ="; OPTIMIZE.RIDDER(FN(x AS DOUBLE) AS DOUBLE => x * x * x - 27, 2, 4)
  PRINT "toms748  ="; OPTIMIZE.TOMS748(FN(x AS DOUBLE) AS DOUBLE => SIN(x) - 0.5, 0, 1)

  ' --- minimizzazione scalare ---
  PRINT "brentmin ="; OPTIMIZE.BRENTMIN(FN(x AS DOUBLE) AS DOUBLE => (x - 3) * (x - 3), 0, 5)
  PRINT "minsc    ="; OPTIMIZE.MINIMIZESCALAR(FN(x AS DOUBLE) AS DOUBLE => (x + 1) * (x + 1) + 2, -5, 5, "brent")

  ' --- minimize multivariata (Rosenbrock -> [1,1]) ---
  DIM x0(1) AS DOUBLE = {-1.2, 1}


## 3. Note e limitazioni

### Optimize-OR-Guida-Utente

*Fonte: `Optimize-OR-Guida-Utente`*


IMPORT OPTIMIZE

| Pacchetto | `.inc` | dylib | Funzioni |
|---|---|---|---|
| localsearch | `shared/include/localsearch.inc` | `libgmblocalsearch` | 5 |
| conic | `shared/include/conic.inc` | `libgmbconic` | 7 |
| lagrangian | `shared/include/lagrangian.inc` | `libgmblagrangian` | 1 |

## 1. Convenzioni comuni

## 2. `localsearch` — motore di ricerca locale (`LS*`)

### 2.1 `LSSOLVE` — discesa semplice, una partenza

DECLARE FUNCTION LSSOLVE LIB "gmblocalsearch" ALIAS "ofx_ls_solve" (
    eval AS FUNCTION(state() AS DOUBLE) AS DOUBLE,
    initial() AS LONG, n AS LONG, shape AS LONG, maxSteps AS LONG,
    out() AS DOUBLE) AS LONG

| Parametro | Tipo | Significato |
|---|---|---|
| `eval` | callback | `FUNCTION(state() AS DOUBLE) AS DOUBLE`: punteggio **soft** dello stato (più alto = meglio). Lo stato arriva 0-based (`state(0..n-1)`). |
| `initial()` | `LONG(n)` **IN/OUT** | In ingresso il punto di partenza; in uscita (esito `1`) lo **stato migliore trovato**. |
| `n` | `LONG` | Dimensione dello stato (> 0). |
| `shape` | `LONG` | `0` = permutazione di `0..n-1` (mosse relocate/swap/2-opt); `1` = assegnazione (valori `0..1`). |
| `maxSteps` | `LONG` | Budget di passi (> 0). Non c'è limite di tempo: il time-box si fa col chiamante. |
| `out()` | `DOUBLE(3)` | `out(0)` = hard (sempre `0`), `out(1)` = soft migliore, `out(2)` = passi eseguiti. |

FUNCTION Score(state() AS DOUBLE) AS DOUBLE
  s = 0
  FOR i = 0 TO UBOUND(state) : s = s + i * state(i) : NEXT i
  RETURN s                      ' massimizzo la somma pesata
END FUNCTION

DIM init(3) AS LONG = {3, 2, 1, 0}
DIM out(2) AS DOUBLE
r = OPTIMIZE.LSSOLVE(Score, init, 4, 0, 200, out)
' r = 1 · out(1) = 14 · init ora contiene {0,1,2,3}


### 2.2 `LSSOLVECFG` — partenze multiple e acceptor

DECLARE FUNCTION LSSOLVECFG LIB "gmblocalsearch" ALIAS "ofx_ls_solve_cfg" (
    eval AS FUNCTION(state() AS DOUBLE) AS DOUBLE,
    initial() AS LONG, n AS LONG, shape AS LONG, maxSteps AS LONG,
    starts AS LONG, acceptor AS LONG, out() AS DOUBLE) AS LONG

| Parametro | Tipo | Significato |
|---|---|---|
| `starts` | `LONG` | Numero di partenze indipendenti (`<= 0` → 1). Ogni partenza usa un seed derivato (`seed + s*0x9E3779B97F4A7C15`) → **deterministico**. |
| `acceptor` | `LONG` | Criterio di accettazione della mossa peggiorante: `0` hill climbing, `1` late acceptance, `2` entity tabu, `3` simulated annealing, `4` great deluge, `5` step-counting HC, `6` record-to-record. |

### 2.3 `LSNEAREST` — costruttiva nearest neighbour

DECLARE FUNCTION LSNEAREST LIB "gmblocalsearch" ALIAS "ofx_ls_nearest" (
    dist() AS DOUBLE, n AS LONG, start AS LONG, perm() AS LONG) AS LONG

| Parametro | Tipo | Significato |
|---|---|---|
| `dist()` | `DOUBLE(n*n)` | Matrice delle distanze row-major (`dist(i*n+j)`). |
| `n` | `LONG` | Numero di nodi. |
| `start` | `LONG` | Nodo di partenza, in `[0, n)`. |
| `perm()` | `LONG(n)` | **Uscita**: ordine di visita (permutazione di `0..n-1`), `perm(0) = start`. |

### 2.4 `LSCHEAPEST` — costruttiva cheapest insertion

DECLARE FUNCTION LSCHEAPEST LIB "gmblocalsearch" ALIAS "ofx_ls_cheapest" (
    dist() AS DOUBLE, n AS LONG, perm() AS LONG) AS LONG


### 2.5 `LSNOWMS` — orologio CPU

DECLARE FUNCTION LSNOWMS LIB "gmblocalsearch" ALIAS "ofx_ls_now_ms" () AS DOUBLE


### 2.6 Esempio completo

cheapest= 1 :92: 1 |nearest= 1 :148: 1
|hc= 1 :86:-1 : 1
|la= 1 :88: 1
|sa= 1 :86: 1
|migliora=-1 :-1
|clock=-1 -1
|tour=
 0  1  2  3  6  7  4  5 |cost=86


## 3. `conic` — coni esponenziale e potenza (`EXPCONE*`)
| `kind` | Cono | Dominio (punto interno) |
|---|---|---|
| `0` | `PEXP` (esponenziale primale) | `t >= u*EXP(v/u)`, `u > 0` |
| `1` | `PPOW` (potenza primale) | `t^(2α) * u^(2(1-α)) - v² > 0`, `t,u > 0` |
| `2` | `RPOW` (potenza ruotata) | `2 * t^(2α) * u^(2(1-α)) - v² > 0`, `t,u > 0` |

### 3.1 `EXPCONEBARRIER` — valore della barriera

DECLARE FUNCTION EXPCONEBARRIER LIB "gmbconic" ALIAS "ofx_expcone_barrier" (
    kind AS LONG, alpha AS DOUBLE, x() AS DOUBLE, f() AS DOUBLE) AS LONG

| Parametro | Tipo | Significato |
|---|---|---|
| `kind` | `LONG` | `0` PEXP, `1` PPOW, `2` RPOW. |
| `alpha` | `DOUBLE` | Esponente del generatore di potenza, in `(0,1)`; ignorato per `PEXP`. |
| `x()` | `DOUBLE(3)` | Punto `(t,u,v)`. |
| `f()` | `DOUBLE(1)` | **Uscita**: valore della barriera `f(x)` (negativo all'interno). |

### 3.2 `EXPCONEGRAD` — gradiente

DECLARE FUNCTION EXPCONEGRAD LIB "gmbconic" ALIAS "ofx_expcone_grad" (
    kind AS LONG, alpha AS DOUBLE, x() AS DOUBLE, g() AS DOUBLE) AS LONG


### 3.3 `EXPCONEHESS` — Hessiana

DECLARE FUNCTION EXPCONEHESS LIB "gmbconic" ALIAS "ofx_expcone_hess" (
    kind AS LONG, alpha AS DOUBLE, x() AS DOUBLE, H() AS DOUBLE) AS LONG


### 3.4 `EXPCONEDUALIN` — membership nel cono duale

DECLARE FUNCTION EXPCONEDUALIN LIB "gmbconic" ALIAS "ofx_expcone_dual_in" (
    kind AS LONG, alpha AS DOUBLE, s() AS DOUBLE) AS LONG


### 3.5 `EXPCONEDUALPOINT` — punto di scaling duale

DECLARE FUNCTION EXPCONEDUALPOINT LIB "gmbconic" ALIAS "ofx_expcone_dual_point" (
    kind AS LONG, alpha AS DOUBLE, s() AS DOUBLE, W() AS DOUBLE) AS LONG


### 3.6 `EXPCONENTMETRICS` — metriche NT

DECLARE FUNCTION EXPCONENTMETRICS LIB "gmbconic" ALIAS "ofx_expcone_nt_metrics" (
    kind AS LONG, alpha AS DOUBLE, z() AS DOUBLE, s() AS DOUBLE, m() AS DOUBLE) AS LONG

| Indice | Metrica | Significato |
|---|---|---|
| `m(0)` | `GAP` | `z's` (gap di dualità) |
| `m(1)` | `CONDZ` | condizionamento lato primale |
| `m(2)` | `CONDS` | condizionamento lato duale |
| `m(3)` | `REL` | misura relativa di complementarità |
| `m(4)` | `MZ` | norma/peso primale |
| `m(5)` | `MS` | norma/peso duale |
| `m(6)` | `WN` | norma del punto di scaling (deve essere > 0) |

### 3.7 `EXPCONESCALING` — scalatura NT completa

DECLARE FUNCTION EXPCONESCALING LIB "gmbconic" ALIAS "ofx_expcone_scaling" (
    kind AS LONG, alpha AS DOUBLE, z() AS DOUBLE, s() AS DOUBLE,
    smu AS DOUBLE, out() AS DOUBLE) AS LONG

| Parametro | Tipo | Significato |
|---|---|---|
| `z()` | `DOUBLE(3)` | Punto primale interno. |
| `s()` | `DOUBLE(3)` | Punto duale interno. |
| `smu` | `DOUBLE` | Parametro di centralità del passo. |
| `out()` | `DOUBLE(28)` | `Theta(9)` \| `Thin(9)` \| `rt(9)` \| `scale`, tutti row-major. |

### 3.8 Esempio completo

## 4. `lagrangian` — rilassamento lagrangiano con subgradiente

min f(x)   s.t.   A x >= b,   x in X


DECLARE FUNCTION LAGRANGIANSUBGRADIENT LIB "gmblagrangian" ALIAS "ofx_lagr_subgradient" (
    sub AS FUNCTION(lam() AS DOUBLE) AS ARRAY,
    A() AS DOUBLE, b() AS DOUBLE, m AS LONG, nx AS LONG, maxit AS LONG,
    out() AS DOUBLE) AS DOUBLE

| Parametro | Tipo | Significato |
|---|---|---|
| `sub` | callback | `FUNCTION(lam() AS DOUBLE) AS ARRAY OF DOUBLE`: riceve i moltiplicatori `lam(0..m-1)` e risolve il subproblema su `X`. |
| `A()` | `DOUBLE(m*nx)` | Matrice dei vincoli rilassati, row-major. |
| `b()` | `DOUBLE(m)` | Termini noti dei vincoli. |
| `m` | `LONG` | Numero di vincoli rilassati (> 0). |
| `nx` | `LONG` | Dimensione di `x` (> 0). |
| `maxit` | `LONG` | Iterazioni massime (`<= 0` → 100). |
| `out()` | `DOUBLE(1+m+nx)` | `out(0)` = miglior lower bound; `out(1..m)` = `lambda`; `out(1+m..m+nx)` = `x` del subproblema migliore. |

### 4.1 Contratto della callback (importante)

r(0)      = valore ottimo del subproblema (min su X di f(x) + lambda'(b - A x))
r(1..nx)  = x(0..nx-1) — la soluzione del subproblema


### 4.2 Esempio completo

## 5. Protocollo §4.1 — esito 4 backend
| Superficie | MiniTest | Funzioni coperte | IL | PCode | VM/C | C99Native |
|---|---|---|---|---|---|---|
| `LS*` | `minitests/localsearchffi/ls_core.bas` | `LSSOLVE`, `LSSOLVECFG` (7 acceptor), `LSNEAREST`, `LSCHEAPEST`, `LSNOWMS` | ✓ | ✓ | ✓ | ✓ |
| `EXPCONE*` | `minitests/conicffi/conic_core.bas` | barriera/gradiente/Hessiana sui 3 coni, `DUALIN`, `DUALPOINT`, `METRICS`, `SCALING`, domini e `alpha` fuori range | ✓ | ✓ | ✓ | ✓ |
| `LAGRANGIANSUBGRADIENT` | `minitests/lagrangianffi/lagr_core.bas` | 2 istanze (`m=2/nx=2`, `m=1/nx=3`), `maxit<=0` (default 100), layout di `out()` | ✓ | ✓ | ✓ | ✓ |
| Sample | Funzioni | Output | Parità |
|---|---|---|---|
| `shared/samples/include/optimize/07-LocalSearch.bas` | 5/5 | 9 righe | `parity4` PASS (4 backend) |
| `shared/samples/include/optimize/08-Conic.bas` | 7/7 | 14 righe | `parity4` PASS (4 backend) |
| `shared/samples/include/optimize/09-Lagrangian.bas` | 1/1 | 7 righe | `parity4` PASS (4 backend) |

## 6. Limiti e note

## 7. Riferimenti

## 8. `mathprog` e `aviation` — la 2ª fascia (14 funzioni)

### 8.1 `mathprog`: analisi della LP e matheuristica (8)
| Funzione | Firma | Parametri |
|---|---|---|
| `LPREVISED` | `FUNCTION LPREVISED(A() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE, basis() AS LONG, x() AS DOUBLE, m AS LONG, n AS LONG)` AS LONG | Revised simplex da base ammissibile basis() = m; x() = n. |
| `LPBOUNDS` | `FUNCTION LPBOUNDS(A() AS DOUBLE, bounds() AS DOUBLE, c() AS DOUBLE, x() AS DOUBLE, m AS LONG, n AS LONG)` AS LONG | LP con bound: bounds() = s[m] / b[m] / l[n] / u[n] (i bound possono essere ±1e30). |
| `LPDUAL` | `FUNCTION LPDUAL(A() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE, basis() AS LONG, x() AS DOUBLE, m AS LONG, n AS LONG)` AS LONG | Dual simplex da base dual-ammissibile basis() = m; x() = n. |
| `LPREDUCEDCOSTS` | `FUNCTION LPREDUCEDCOSTS(A() AS DOUBLE, c() AS DOUBLE, y() AS DOUBLE, red() AS DOUBLE, m AS LONG, n AS LONG)` AS LONG | Costi ridotti red() = n dati i duali y() = m. |
| `LPALTERNATE` | `FUNCTION LPALTERNATE(A() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE, x2() AS DOUBLE, m AS LONG, n AS LONG, z2() AS DOUBLE)` AS LONG | Ottimo alternativo: x2() = n, z2(0) = valore; 1 se esiste, 0 se unico. |
| `MATHHARDFIXING` | `FUNCTION MATHHARDFIXING(A() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE, intIdx() AS LONG, x() AS DOUBLE, info() AS DOUBLE, m AS LONG, n AS LONG)` AS LONG | Matheuristic hard fixing: info(0)=fixFrac (in), info(1)=rounds (in), info(2)=obj (out). |
| `MATHLOCALBRANCHING` | `FUNCTION MATHLOCALBRANCHING(A() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE, intIdx() AS LONG, x() AS DOUBLE, info() AS DOUBLE, m AS LONG, n AS LONG)` AS LONG | Matheuristic local branching: info(0)=kStart (in), info(1)=rounds (in), info(2)=obj (out). |
| `GOMORYCUT` | `FUNCTION GOMORYCUT(tabrow() AS DOUBLE, ncols AS LONG, rhsCol AS LONG, cut() AS DOUBLE)` AS LONG | Taglio di Gomory da una riga del tableau: tabrow() = ncols; cut() = ncols+1. |

### 8.2 `aviation`: pianificazione operativa (6)
| Funzione | Firma | Parametri |
|---|---|---|
| `AVFLEET` | `FUNCTION AVFLEET(profit() AS DOUBLE, capacity() AS DOUBLE, demand() AS DOUBLE, avail() AS LONG, nf AS LONG, nt AS LONG, assign() AS LONG)` AS DOUBLE | Fleet assignment: profit() = nf*nt; avail() = nt; assign() = nf. |
| `AVCREWPAIRING` | `FUNCTION AVCREWPAIRING(voli() AS LONG, cost() AS DOUBLE, nf AS LONG, np AS LONG, chosen() AS LONG, nChosen() AS LONG)` AS DOUBLE | Crew pairing: voli() = nf*np; chosen() = np; nChosen(0) = quanti. |
| `AVRECOVERY` | `FUNCTION AVRECOVERY(delayCost() AS DOUBLE, nf AS LONG, na AS LONG, assign() AS LONG)` AS DOUBLE | Recovery: delayCost() = nf*na; assign() = nf. |
| `AVCREWRESCHED` | `FUNCTION AVCREWRESCHED(cost() AS DOUBLE, available() AS LONG, ncrew AS LONG, nf AS LONG, assign() AS LONG)` AS DOUBLE | Crew rescheduling: cost() = ncrew*nf; available() = ncrew; assign() = ncrew. |
| `AVHUB` | `FUNCTION AVHUB(dist() AS DOUBLE, flow() AS DOUBLE, n AS LONG, p AS LONG, alpha AS DOUBLE, hubs() AS LONG)` AS DOUBLE | Hub location (p-hub median): dist()/flow() = n*n; hubs() = p. |
| `AVTAILCONTINUITY` | `FUNCTION AVTAILCONTINUITY(cost() AS DOUBLE, na AS LONG, nf AS LONG, assign() AS LONG)` AS DOUBLE | Tail continuity: cost() = na*nf; assign() = na. |

### 8.3 Esito §4.1
| Superficie | MiniTest | Funzioni | IL | PCode | VM/C | C99Native |
|---|---|---|---|---|---|---|
| mathprog (2ª fascia) | `mathprogffi/mathprog2.bas` | 8 | ✓ | ✓ | ✓ | ✓ |
| aviation (2ª fascia) | `aviationffi/aviation2.bas` | 6 | ✓ | ✓ | ✓ | ✓ |

### Optimize-ORTools-Guida-Utente

*Fonte: `Optimize-ORTools-Guida-Utente`*


IMPORT OPTIMIZE


## 1. Convenzioni (valgono per tutto il pacchetto)

## 2. Le funzioni, per dominio

### PACK (9)
| Funzione | Firma | Parametri |
|---|---|---|
| `BINPACKING` | `FUNCTION BINPACKING(items() AS DOUBLE, n AS LONG, cap AS DOUBLE, algo AS LONG, binOf() AS LONG) AS LONG` | items, n, cap, algo, binOf — algo: 0=NF, 1=FF, 2=BF, 3=WF, 4=FFD, 5=BFD; binOf() = n. |
| `BINPACKINGLB` | `FUNCTION BINPACKINGLB(items() AS DOUBLE, n AS LONG, cap AS DOUBLE) AS LONG` | items, n, cap —  |
| `KNAPSACKFRACTIONAL` | `FUNCTION KNAPSACKFRACTIONAL(w() AS DOUBLE, v() AS DOUBLE, n AS LONG, cap AS DOUBLE, x() AS DOUBLE) AS DOUBLE` | w, v, n, cap, x — Knapsack frazionario: x() = n (0..1). |
| `KNAPSACK01` | `FUNCTION KNAPSACK01(w() AS DOUBLE, v() AS DOUBLE, n AS LONG, cap AS LONG, take() AS LONG) AS DOUBLE` | w, v, n, cap, take — 0/1 knapsack (capacità intera): take() = n. |
| `KNAPSACK01MITM` | `FUNCTION KNAPSACK01MITM(w() AS DOUBLE, v() AS DOUBLE, n AS LONG, cap AS DOUBLE, take() AS LONG) AS DOUBLE` | w, v, n, cap, take —  |
| `KNAPSACKUNBOUNDED` | `FUNCTION KNAPSACKUNBOUNDED(w() AS DOUBLE, v() AS DOUBLE, n AS LONG, cap AS LONG, counts() AS LONG) AS DOUBLE` | w, v, n, cap, counts —  |
| `SUBSETSUM` | `FUNCTION SUBSETSUM(items() AS DOUBLE, n AS LONG, target AS DOUBLE, take() AS LONG) AS LONG` | items, n, target, take —  |
| `CUTTINGSTOCK` | `FUNCTION CUTTINGSTOCK(pieces() AS DOUBLE, n AS LONG, stock AS DOUBLE, pattern() AS LONG) AS LONG` | pieces, n, stock, pattern —  |
| `BINPACKING3D` | `FUNCTION BINPACKING3D(items() AS DOUBLE, n AS LONG, W AS DOUBLE, H AS DOUBLE, D AS DOUBLE, binOf() AS LONG) AS LONG` | items, n, W, H, D, binOf —  |

### ASSIGN (9)
| Funzione | Firma | Parametri |
|---|---|---|
| `ASSIGNHUNGARIAN` | `FUNCTION ASSIGNHUNGARIAN(cost() AS DOUBLE, n AS LONG, m AS LONG, assign() AS LONG) AS DOUBLE` | cost, n, m, assign —  |
| `ASSIGNAUCTION` | `FUNCTION ASSIGNAUCTION(cost() AS DOUBLE, n AS LONG, m AS LONG, assign() AS LONG, eps AS DOUBLE) AS DOUBLE` | cost, n, m, assign, eps —  |
| `ASSIGNBOTTLENECK` | `FUNCTION ASSIGNBOTTLENECK(cost() AS DOUBLE, n AS LONG, assign() AS LONG) AS DOUBLE` | cost, n, assign —  |
| `STABLEMARRIAGE` | `FUNCTION STABLEMARRIAGE(prefMen() AS LONG, prefWomen() AS LONG, n AS LONG, matchMen() AS LONG) AS LONG` | prefMen, prefWomen, n, matchMen — Gale-Shapley: prefMen()/prefWomen() = n*n; matchMen() = n; 1 se ok. |
| `TRANSPORTVAM` | `FUNCTION TRANSPORTVAM(supply() AS DOUBLE, demand() AS DOUBLE, cost() AS DOUBLE, m AS LONG, n AS LONG, x() AS DOUBLE) AS DOUBLE` | supply, demand, cost, m, n, x — Trasporto: supply(m), demand(n), cost(m*n), x(m*n) = piano. |
| `TRANSPORTNORTHWEST` | `FUNCTION TRANSPORTNORTHWEST(supply() AS DOUBLE, demand() AS DOUBLE, cost() AS DOUBLE, m AS LONG, n AS LONG, x() AS DOUBLE) AS DOUBLE` | supply, demand, cost, m, n, x —  |
| `TRANSPORTLEASTCOST` | `FUNCTION TRANSPORTLEASTCOST(supply() AS DOUBLE, demand() AS DOUBLE, cost() AS DOUBLE, m AS LONG, n AS LONG, x() AS DOUBLE) AS DOUBLE` | supply, demand, cost, m, n, x —  |
| `TRANSPORTMODI` | `FUNCTION TRANSPORTMODI(supply() AS DOUBLE, demand() AS DOUBLE, cost() AS DOUBLE, m AS LONG, n AS LONG, x() AS DOUBLE, maxit AS LONG) AS DOUBLE` | supply, demand, cost, m, n, x, maxit —  |
| `ASSIGNQAPTABU` | `FUNCTION ASSIGNQAPTABU(A() AS DOUBLE, B() AS DOUBLE, n AS LONG, p() AS LONG, maxit AS LONG, seed AS LONG) AS DOUBLE` | A, B, n, p, maxit, seed — QAP tabu: A/B n*n, p() = n (permutazione), seed LONG. |

### COVER (14)
| Funzione | Firma | Parametri |
|---|---|---|
| `SETCOVER` | `FUNCTION SETCOVER(elem() AS LONG, cost() AS DOUBLE, m AS LONG, n AS LONG, chosen() AS LONG, nChosen() AS LONG) AS DOUBLE` | elem, cost, m, n, chosen, nChosen — elem() = m*n (elemento i in insieme j); chosen() = n; nChosen(0) = quanti. |
| `SETCOVERSOFT` | `FUNCTION SETCOVERSOFT(elem() AS LONG, cost() AS DOUBLE, m AS LONG, n AS LONG, chosen() AS LONG, pen AS DOUBLE, uncovered() AS LONG) AS DOUBLE` | elem, cost, m, n, chosen, pen, uncovered — chosen() booleano in ingresso; uncovered(0) in uscita. |
| `SETCOVERPRICING` | `FUNCTION SETCOVERPRICING(y() AS DOUBLE, m AS LONG, baseCost AS DOUBLE, costPerElem AS DOUBLE, chosenSet() AS LONG) AS DOUBLE` | y, m, baseCost, costPerElem, chosenSet —  |
| `SETCOVERSA` | `FUNCTION SETCOVERSA(elem() AS LONG, cost() AS DOUBLE, m AS LONG, n AS LONG, pen AS DOUBLE, maxit AS LONG, seed AS LONG, chosen() AS LONG) AS DOUBLE` | elem, cost, m, n, pen, maxit, seed, chosen — Simulated annealing (t0=1.0, alpha=0.995 fissati nello shim). |
| `SETCOVERLOCAL` | `FUNCTION SETCOVERLOCAL(elem() AS LONG, cost() AS DOUBLE, m AS LONG, n AS LONG, chosen() AS LONG, nChosen() AS LONG, maxit AS LONG) AS DOUBLE` | elem, cost, m, n, chosen, nChosen, maxit —  |
| `SETCOVERGRASP` | `FUNCTION SETCOVERGRASP(elem() AS LONG, cost() AS DOUBLE, m AS LONG, n AS LONG, alpha AS DOUBLE, maxit AS LONG, chosen() AS LONG, nChosen() AS LONG) AS DOUBLE` | elem, cost, m, n, alpha, maxit, chosen, nChosen — GRASP (seed fisso 12345 nello shim). |
| `SETCOVERRGRASP` | `FUNCTION SETCOVERRGRASP(elem() AS LONG, cost() AS DOUBLE, m AS LONG, n AS LONG, alphas() AS DOUBLE, maxit AS LONG, chosen() AS LONG, nChosen() AS LONG) AS DOUBLE` | elem, cost, m, n, alphas, maxit, chosen, nChosen — Reactive GRASP: alphas() = [nalpha, a0, a1, ...] (nalpha compatto in testa; seed fisso). |
| `MAXCOVERAGE` | `FUNCTION MAXCOVERAGE(elem() AS LONG, m AS LONG, n AS LONG, k AS LONG, chosen() AS LONG, nChosen() AS LONG) AS LONG` | elem, m, n, k, chosen, nChosen —  |
| `PMEDIAN` | `FUNCTION PMEDIAN(dist() AS DOUBLE, n AS LONG, p AS LONG, medians() AS LONG, assign() AS LONG, maxit AS LONG) AS DOUBLE` | dist, n, p, medians, assign, maxit — p-median: dist(n*n); medians(p), assign(n). |
| `PCENTER` | `FUNCTION PCENTER(dist() AS DOUBLE, n AS LONG, p AS LONG, medians() AS LONG) AS DOUBLE` | dist, n, p, medians —  |
| `VERTEXCOVER` | `FUNCTION VERTEXCOVER(edges() AS LONG, n AS LONG, ne AS LONG, cover() AS LONG) AS LONG` | edges, n, ne, cover — Vertex cover: edges() = 2*ne; cover() = n. |
| `HITTINGSET` | `FUNCTION HITTINGSET(elem() AS LONG, m AS LONG, n AS LONG, chosen() AS LONG, nChosen() AS LONG) AS LONG` | elem, m, n, chosen, nChosen —  |
| `FACILITYLOCATION` | `FUNCTION FACILITYLOCATION(openCost() AS DOUBLE, connCost() AS DOUBLE, m AS LONG, n AS LONG, open() AS LONG) AS DOUBLE` | openCost, connCost, m, n, open —  |
| `KMEANS` | `FUNCTION KMEANS(points() AS DOUBLE, n AS LONG, dim AS LONG, k AS LONG, centroids() AS DOUBLE, labels() AS LONG, maxit AS LONG) AS DOUBLE` | points, n, dim, k, centroids, labels, maxit — k-means: points(n*dim), centroids(k*dim), labels(n); seed fisso nello shim. |

### CP (3)
| Funzione | Firma | Parametri |
|---|---|---|
| `SATDPLL` | `FUNCTION SATDPLL(clauses() AS LONG, clauseStart() AS LONG, clauseLen() AS LONG, nvars AS LONG, nclauses AS LONG, assignment() AS LONG) AS LONG` | clauses, clauseStart, clauseLen, nvars, nclauses, assignment — clauses() flat; clauseStart()/clauseLen() = nclauses; assignment() = nvars; 1 = SATDPLL. |
| `CPALLDIFF` | `FUNCTION CPALLDIFF(dom() AS LONG, n AS LONG, D AS LONG, sol() AS LONG) AS LONG` | dom, n, D, sol — dom() = n*D; sol() = n. |
| `CPBINARY` | `FUNCTION CPBINARY(dom() AS LONG, allowed() AS LONG, n AS LONG, D AS LONG, sol() AS LONG) AS LONG` | dom, allowed, n, D, sol —  |

### ROUTING (24)
| Funzione | Firma | Parametri |
|---|---|---|
| `TSPNEAREST` | `FUNCTION TSPNEAREST(dist() AS DOUBLE, n AS LONG, tour() AS LONG) AS DOUBLE` | dist, n, tour — supererebbe gli 8 argomenti FFI. |
| `TSP2OPT` | `FUNCTION TSP2OPT(dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxit AS LONG) AS DOUBLE` | dist, n, tour, maxit —  |
| `TSPTOURCOST` | `FUNCTION TSPTOURCOST(dist() AS DOUBLE, n AS LONG, tour() AS LONG) AS DOUBLE` | dist, n, tour —  |
| `TSPEXTRAMILEAGE` | `FUNCTION TSPEXTRAMILEAGE(dist() AS DOUBLE, n AS LONG, tour() AS LONG) AS DOUBLE` | dist, n, tour —  |
| `TSPGRASP` | `FUNCTION TSPGRASP(dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxit AS LONG) AS DOUBLE` | dist, n, tour, maxit — GRASP: alpha=0.3 e seed fissi nello shim. |
| `TSPVNS` | `FUNCTION TSPVNS(dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxit AS LONG) AS DOUBLE` | dist, n, tour, maxit —  |
| `TSP3OPT` | `FUNCTION TSP3OPT(dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxit AS LONG) AS DOUBLE` | dist, n, tour, maxit —  |
| `TSPSA` | `FUNCTION TSPSA(dist() AS DOUBLE, n AS LONG, tour() AS LONG, T0 AS DOUBLE, cooling AS DOUBLE, iters AS LONG) AS DOUBLE` | dist, n, tour, T0, cooling, iters —  |
| `TSPTABU` | `FUNCTION TSPTABU(dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxit AS LONG, tenureKind AS LONG) AS DOUBLE` | dist, n, tour, maxit, tenureKind — tenure_kind: 0=costante, 1=triangolare, 2=quadrata, 3=sawtooth. |
| `TSPONETREE` | `FUNCTION TSPONETREE(dist() AS DOUBLE, n AS LONG, root AS LONG, edges() AS LONG) AS DOUBLE` | dist, n, root, edges —  |
| `TSPHKBOUND` | `FUNCTION TSPHKBOUND(dist() AS DOUBLE, n AS LONG, maxit AS LONG, pi() AS DOUBLE) AS DOUBLE` | dist, n, maxit, pi —  |
| `TSPSUBTOURS` | `FUNCTION TSPSUBTOURS(succ() AS LONG, n AS LONG, comp() AS LONG, sizes() AS LONG) AS LONG` | succ, n, comp, sizes — succ()/comp()/sizes() = n; ritorna il numero di sottotour. |
| `TSPDFJ` | `FUNCTION TSPDFJ(dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxcuts AS LONG) AS DOUBLE` | dist, n, tour, maxcuts —  |
| `TSPERX` | `FUNCTION TSPERX(p1() AS LONG, p2() AS LONG, n AS LONG, child() AS LONG) AS LONG` | p1, p2, n, child — ERX: p1()/p2()/child() = n; ritorna 1. |
| `TSPGA` | `FUNCTION TSPGA(dist() AS DOUBLE, n AS LONG, tour() AS LONG, pop AS LONG, gens AS LONG) AS DOUBLE` | dist, n, tour, pop, gens —  |
| `TSPOROPT` | `FUNCTION TSPOROPT(dist() AS DOUBLE, n AS LONG, tour() AS LONG, maxit AS LONG) AS DOUBLE` | dist, n, tour, maxit —  |
| `TSPDOUBLETREE` | `FUNCTION TSPDOUBLETREE(dist() AS DOUBLE, n AS LONG, tour() AS LONG) AS DOUBLE` | dist, n, tour —  |
| `TSPHELDKARP` | `FUNCTION TSPHELDKARP(dist() AS DOUBLE, n AS LONG, tour() AS LONG) AS DOUBLE` | dist, n, tour —  |
| `VRPCLARKEWRIGHT` | `FUNCTION VRPCLARKEWRIGHT(dist() AS DOUBLE, demand() AS DOUBLE, n AS LONG, cap AS DOUBLE, depot AS LONG, routeOf() AS LONG, total() AS DOUBLE) AS LONG` | dist, demand, n, cap, depot, routeOf, total — CVRP: dist (n+1)x(n+1) col deposito; demand() = n; routeOf() = n; total(0). |
| `VRPTIMEWINDOWS` | `FUNCTION VRPTIMEWINDOWS(dist() AS DOUBLE, demand() AS DOUBLE, n AS LONG, cap AS DOUBLE, depot AS LONG, tw() AS DOUBLE, routeOf() AS LONG, total() AS DOUBLE) AS LONG` | dist, demand, n, cap, depot, tw, routeOf, total — VRPTIMEWINDOWS: tw() = ready[n] / due[n] / service[n] (11 parametri C compattati). |
| `VRPMULTIDEPOT` | `FUNCTION VRPMULTIDEPOT(dist() AS DOUBLE, demand() AS DOUBLE, n AS LONG, ndepots AS LONG, cap AS DOUBLE, routeOf() AS LONG, total() AS DOUBLE) AS LONG` | dist, demand, n, ndepots, cap, routeOf, total — VRPMULTIDEPOT: depositi agli indici 0..ndepots-1. |
| `VRPPICKUPDELIVERY` | `FUNCTION VRPPICKUPDELIVERY(dist() AS DOUBLE, demand() AS DOUBLE, nreq AS LONG, cap AS DOUBLE, pickup() AS LONG, delivery() AS LONG, route() AS LONG, total() AS DOUBLE) AS LONG` | dist, demand, nreq, cap, pickup, delivery, route, total — VRPPICKUPDELIVERY: pickup()/delivery() = nreq; route() = 2*nreq; total(0). |
| `ARCROUTING` | `FUNCTION ARCROUTING(w() AS DOUBLE, n AS LONG, euler() AS LONG, elen() AS LONG) AS DOUBLE` | w, n, euler, elen — Arc routing (Chinese postman): euler() = n*n+2; elen(0) = lunghezza. |
| `METAACOTSP` | `FUNCTION METAACOTSP(dist() AS DOUBLE, n AS LONG, tour() AS LONG, ants AS LONG, maxit AS LONG) AS DOUBLE` | dist, n, tour, ants, maxit —  |

### SCHED (16)
| Funzione | Firma | Parametri |
|---|---|---|
| `SCHEDSPT` | `FUNCTION SCHEDSPT(p() AS DOUBLE, n AS LONG, order() AS LONG) AS DOUBLE` | p, n, order —  |
| `SCHEDWSPT` | `FUNCTION SCHEDWSPT(p() AS DOUBLE, w() AS DOUBLE, n AS LONG, order() AS LONG) AS DOUBLE` | p, w, n, order —  |
| `SCHEDEDD` | `FUNCTION SCHEDEDD(p() AS DOUBLE, due() AS DOUBLE, n AS LONG, order() AS LONG) AS DOUBLE` | p, due, n, order —  |
| `SCHEDMOORE` | `FUNCTION SCHEDMOORE(p() AS DOUBLE, due() AS DOUBLE, n AS LONG, order() AS LONG) AS LONG` | p, due, n, order — Min #tardivi (Moore-Hodgson): ritorna il numero di job tardivi; order() = n. |
| `SCHEDLAWLER` | `FUNCTION SCHEDLAWLER(p() AS DOUBLE, due() AS DOUBLE, n AS LONG, order() AS LONG) AS DOUBLE` | p, due, n, order —  |
| `FLOWSHOPJOHNSON` | `FUNCTION FLOWSHOPJOHNSON(p1() AS DOUBLE, p2() AS DOUBLE, n AS LONG, seq() AS LONG) AS DOUBLE` | p1, p2, n, seq — Flow shop: seq() = n. |
| `FLOWSHOPNEH` | `FUNCTION FLOWSHOPNEH(p() AS DOUBLE, n AS LONG, m AS LONG, seq() AS LONG) AS DOUBLE` | p, n, m, seq — FLOWSHOPNEH: p() = n*m (job i, macchina k = p(i*m+k)). |
| `FLOWSHOPJOHNSON3` | `FUNCTION FLOWSHOPJOHNSON3(p1() AS DOUBLE, p2() AS DOUBLE, p3() AS DOUBLE, n AS LONG, seq() AS LONG) AS DOUBLE` | p1, p2, p3, n, seq —  |
| `FLOWSHOPCDS` | `FUNCTION FLOWSHOPCDS(p() AS DOUBLE, n AS LONG, m AS LONG, seq() AS LONG) AS DOUBLE` | p, n, m, seq —  |
| `PARALLELLPT` | `FUNCTION PARALLELLPT(p() AS DOUBLE, n AS LONG, m AS LONG, assign() AS LONG) AS DOUBLE` | p, n, m, assign —  |
| `PROJECTCPM` | `FUNCTION PROJECTCPM(dur() AS DOUBLE, preds() AS LONG, n AS LONG, npred AS LONG, est() AS DOUBLE, lst() AS DOUBLE, slack() AS DOUBLE) AS DOUBLE` | dur, preds, n, npred, est, lst, slack — PROJECTCPM: preds() = n*npred (predecessori, -1 padding); est()/lst()/slack() = n. |
| `PROJECTPERT` | `FUNCTION PROJECTPERT(to() AS DOUBLE, tm() AS DOUBLE, tp() AS DOUBLE, preds() AS LONG, n AS LONG, npred AS LONG, est() AS DOUBLE, variance() AS DOUBLE) AS DOUBLE` | to, tm, tp, preds, n, npred, est, variance — PROJECTPERT: to()/tm()/tp() = n; est()/variance() = n. |
| `PROJECTRCPSP` | `FUNCTION PROJECTRCPSP(dur() AS DOUBLE, preds() AS LONG, n AS LONG, npred AS LONG, demand() AS DOUBLE, capacity() AS DOUBLE, nr AS LONG, start() AS LONG) AS DOUBLE` | dur, preds, n, npred, demand, capacity, nr, start — PROJECTRCPSP: demand() = n*nr; capacity() = nr; start() = n. |
| `JOBSHOP` | `FUNCTION JOBSHOP(route() AS LONG, dur() AS DOUBLE, njobs AS LONG, nmach AS LONG, start() AS LONG) AS DOUBLE` | route, dur, njobs, nmach, start — Job shop: route()/dur()/start() = njobs*nmach. |
| `ROSTERING` | `FUNCTION ROSTERING(demand() AS LONG, nnurses AS LONG, ndays AS LONG, maxWork AS LONG, schedule() AS LONG) AS LONG` | demand, nnurses, ndays, maxWork, schedule — Rostering: demand() = ndays; schedule() = nnurses*ndays; ritorna i turni non coperti. |
| `LITTLESLAW` | `FUNCTION LITTLESLAW(lambda AS DOUBLE, W AS DOUBLE) AS DOUBLE` | lambda, W —  |

### META (11)
| Funzione | Firma | Parametri |
|---|---|---|
| `METAHILLCLIMB` | `FUNCTION METAHILLCLIMB(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, maxit AS LONG) AS DOUBLE` | f FUNCTION(x ), x, d, lo, hi, maxit — Seed fissi dove la firma C supera gli 8 argomenti FFI. |
| `METASA` | `FUNCTION METASA(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, maxit AS LONG, T0 AS DOUBLE, alpha AS DOUBLE) AS DOUBLE` | f FUNCTION(x ), x, d, lo, hi, maxit, T0, alpha —  |
| `METATABU` | `FUNCTION METATABU(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, maxit AS LONG, tenure AS LONG) AS DOUBLE` | f FUNCTION(x ), x, d, lo, hi, maxit, tenure — tenure: 0=costante, 1=triangolare, 2=quadrata, 3=sawtooth. |
| `METAGA` | `FUNCTION METAGA(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, pop AS LONG, maxgen AS LONG) AS DOUBLE` | f FUNCTION(x ), x, d, lo, hi, pop, maxgen —  |
| `METAPSO` | `FUNCTION METAPSO(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, swarm AS LONG, maxit AS LONG) AS DOUBLE` | f FUNCTION(x ), x, d, lo, hi, swarm, maxit —  |
| `METADE` | `FUNCTION METADE(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, pop AS LONG, maxgen AS LONG) AS DOUBLE` | f FUNCTION(x ), x, d, lo, hi, pop, maxgen —  |
| `METACMAES` | `FUNCTION METACMAES(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, maxit AS LONG) AS DOUBLE` | f FUNCTION(x ), x, d, lo, hi, maxit —  |
| `METAES` | `FUNCTION METAES(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, sigma AS DOUBLE, alpha AS DOUBLE, pop AS LONG, maxit AS LONG) AS DOUBLE` | f FUNCTION(x ), x, d, sigma, alpha, pop, maxit — ES/ARS: nessun box (perturbazioni gaussiane attorno a x). |
| `METAARS` | `FUNCTION METAARS(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, x() AS DOUBLE, d AS LONG, sigma AS DOUBLE, alpha AS DOUBLE, topB AS LONG, pop AS LONG, maxit AS LONG) AS DOUBLE` | f FUNCTION(x ), x, d, sigma, alpha, topB, pop, maxit —  |
| `TABUTENURE` | `FUNCTION TABUTENURE(kind AS LONG, it AS LONG, n AS LONG) AS LONG` | kind, it, n —  |
| `METANSGA2` | `FUNCTION METANSGA2(f1 AS FUNCTION(x() AS DOUBLE) AS DOUBLE, f2 AS FUNCTION(x() AS DOUBLE) AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, pop AS LONG, maxgen AS LONG, out() AS DOUBLE) AS LONG` | f1 FUNCTION(x ), f2 FUNCTION(x ), d, lo, hi, pop, maxgen, out — NSGA-II (2 obiettivi): out() = pareto[pop*d] / fv1[pop] / fv2[pop]. |

### UTIL (17)
| Funzione | Firma | Parametri |
|---|---|---|
| `INTERVALMAX` | `FUNCTION INTERVALMAX(start() AS DOUBLE, finish() AS DOUBLE, n AS LONG, selected() AS LONG) AS LONG` | start, finish, n, selected — Intervalli: selected()/start()/end() = n. |
| `INTERVALPARTITION` | `FUNCTION INTERVALPARTITION(start() AS DOUBLE, finish() AS DOUBLE, n AS LONG) AS LONG` | start, finish, n —  |
| `TIMETABLING` | `FUNCTION TIMETABLING(conflicts() AS LONG, n AS LONG, slotOf() AS LONG) AS LONG` | conflicts, n, slotOf — Timetabling: conflicts() = n*n; slotOf() = n. |
| `QUEUEMM1` | `FUNCTION QUEUEMM1(lambda AS DOUBLE, mu AS DOUBLE, out() AS DOUBLE) AS DOUBLE` | lambda, mu, out — out() = L, Lq, W, Wq; ritorna rho. |
| `QUEUEMMC` | `FUNCTION QUEUEMMC(lambda AS DOUBLE, mu AS DOUBLE, c AS LONG, out() AS DOUBLE) AS DOUBLE` | lambda, mu, c, out —  |
| `EOQ` | `FUNCTION EOQ(demand AS DOUBLE, setup AS DOUBLE, holding AS DOUBLE) AS DOUBLE` | demand, setup, holding —  |
| `EPQ` | `FUNCTION EPQ(demand AS DOUBLE, setup AS DOUBLE, holding AS DOUBLE, rate AS DOUBLE) AS DOUBLE` | demand, setup, holding, rate —  |
| `NEWSVENDOR` | `FUNCTION NEWSVENDOR(cu AS DOUBLE, co AS DOUBLE, mean AS DOUBLE, std AS DOUBLE) AS DOUBLE` | cu, co, mean, std —  |
| `SAFETYSTOCK` | `FUNCTION SAFETYSTOCK(z AS DOUBLE, sigma AS DOUBLE) AS DOUBLE` | z, sigma —  |
| `AHP` | `FUNCTION AHP(pairwise() AS DOUBLE, n AS LONG, weights() AS DOUBLE, consistency() AS DOUBLE) AS LONG` | pairwise, n, weights, consistency — AHP: weights() = n; consistency(0) = rapporto di consistenza. |
| `TOPSIS` | `FUNCTION TOPSIS(X() AS DOUBLE, m AS LONG, n AS LONG, w() AS DOUBLE, benefit() AS LONG, scores() AS DOUBLE) AS LONG` | X, m, n, w, benefit, scores — TOPSIS: X() = m*n; benefit() = n; scores() = m. |
| `MDPVALUE` | `FUNCTION MDPVALUE(P() AS DOUBLE, R() AS DOUBLE, ns AS LONG, na AS LONG, gamma AS DOUBLE, maxit AS LONG, out() AS DOUBLE) AS LONG` | P, R, ns, na, gamma, maxit, out — MDP: out() = V[ns] / policy[ns] (policy come double); ritorna le iterazioni. |
| `MDPPOLICY` | `FUNCTION MDPPOLICY(P() AS DOUBLE, R() AS DOUBLE, ns AS LONG, na AS LONG, gamma AS DOUBLE, maxit AS LONG, out() AS DOUBLE) AS LONG` | P, R, ns, na, gamma, maxit, out —  |
| `MONTECARLO` | `FUNCTION MONTECARLO(f AS FUNCTION(x() AS DOUBLE) AS DOUBLE, d AS LONG, lo() AS DOUBLE, hi() AS DOUBLE, n AS LONG, out() AS DOUBLE) AS DOUBLE` | f FUNCTION(x ), d, lo, hi, n, out — Monte Carlo: out(0) = deviazione standard. |
| `CVAR` | `FUNCTION CVAR(returns() AS DOUBLE, n AS LONG, alpha AS DOUBLE, out() AS DOUBLE) AS DOUBLE` | returns, n, alpha, out — CVaR: out(0) = VaR. |
| `DESMM1` | `FUNCTION DESMM1(lambda AS DOUBLE, mu AS DOUBLE, ncust AS LONG, out() AS DOUBLE) AS DOUBLE` | lambda, mu, ncust, out — DES M/M/1: out(0) = utilizzazione. |
| `CHANCECONSTRAINT` | `FUNCTION CHANCECONSTRAINT(sigma() AS DOUBLE, x() AS DOUBLE, n AS LONG, alpha AS DOUBLE, out() AS DOUBLE) AS DOUBLE` | sigma, x, n, alpha, out — Chance constraint: out(0) = z. |

### DP (3)
| Funzione | Firma | Parametri |
|---|---|---|
| `DPOPTIMALBST` | `FUNCTION DPOPTIMALBST(p() AS DOUBLE, n AS LONG, root() AS LONG) AS DOUBLE` | p, n, root — Optimal BST: p() = n; root() = n*n (-1 se vuoto). |
| `DPEQUIPMENT` | `FUNCTION DPEQUIPMENT(years AS LONG, buyCost AS DOUBLE, maxAge AS LONG, opCost() AS DOUBLE, resale() AS DOUBLE, schedule() AS LONG) AS DOUBLE` | years, buyCost, maxAge, opCost, resale, schedule — Equipment replacement: opCost()/resale() = maxAge; schedule() = years. |
| `DPSPORTSSERIES` | `FUNCTION DPSPORTSSERIES(nGames AS LONG, needed AS LONG, pHome AS DOUBLE, pAway AS DOUBLE, isHome() AS LONG) AS DOUBLE` | nGames, needed, pHome, pAway, isHome — Sports series: isHome() = nGames. |

## 3. Esempio completo

## 4. Protocollo §4.1 — esito
| Superficie | MiniTest | Funzioni | IL | PCode | VM/C | C99Native |
|---|---|---|---|---|---|---|
| pack + assign | `minitests/orffi/pack_assign2.bas` | 13 | ✓ | ✓ | ✓ | ✓ |
| cover + cp | `minitests/orffi/cover2.bas` | 12 | ✓ | ✓ | ✓ | ✓ |
| routing | `minitests/orffi/routing2.bas` | 13/14 | ✓ | ✓ | ✓ | ✓ |
| sched | `minitests/orffi/sched2.bas` | 8 | ✓ | ✓ | ✓ | ✓ |
| meta + util + dp | `minitests/orffi/meta_util_dp2.bas` | 17 | ✓ | ✓ | ✓ | ✓ |

## 5. Difetti trovati dal protocollo
| Difetto | Esito |
|---|---|
| `CPBINARY` leggeva `allowed[]` oltre l'allocazione (lo shim convertiva `n*n` valori invece di `n*n*D*D`) | **corretto** nello shim: il vincolo binario funziona su 4 backend |
| `DPEQUIPMENT` dichiarata `AS LONG` ma restituisce un costo `DOUBLE` (valore troncato, es. `-1342177280`) | **corretto** in `ortools.inc` (`AS DOUBLE`); controllo automatico su tutte e 106 le dichiarazioni: unica discrepanza |
| `TSPERX` restituisce un figlio corretto ma **corrompe l'heap** (SIGBUS nella backend nativa dopo la chiamata) | **aperto**, escluso dal MiniTest: `Bugs/(open) - OR-TSPERX-Corruzione-Heap.md` |
| `SUBSETSUM` restituisce l'esito giusto ma `take()` non ricostruisce una combinazione che somma al target | **aperto**: `Bugs/(open) - OR-SubsetSum-Ricostruzione.md` |

## 6. Note pratiche

## 7. Riferimenti

### Spatial-Guida-Utente

*Fonte: `Spatial-Guida-Utente`*


## 1. Panoramica

IMPORT SPATIAL
SUB Main()
    DIM P(1) AS ARRAY OF DOUBLE
    P(0) = {0, 0}
    P(1) = {3, 4}
    DIM d() = SPATIAL.PDIST(P, "euclidean")
    PRINT d(0)               ' 5
END SUB


## 2. `PDIST(X, metric)` → `DOUBLE[]`
| `metric` | Descrizione |
|---|---|
| `"euclidean"` | distanza euclidea (default) |
| `"cityblock"`/`"manhattan"` | somma dei valori assoluti |
| `"chebyshev"` | massimo scarto per coordinata |
| `"cosine"` | distanza coseno |

DIM d() = SPATIAL.PDIST(P, "euclidean")
PRINT d(0)          ' distanza P(0)→P(1)


## 3. `CONVEXHULL(X)` → `DOUBLE[]`

DIM P(4) AS ARRAY OF DOUBLE
P(0) = {0, 0} : P(1) = {4, 0} : P(2) = {4, 4} : P(3) = {0, 4} : P(4) = {2, 2}
DIM h() = SPATIAL.CONVEXHULL(P)
PRINT h(0); h(1); h(2); h(3)     ' 0  1  2  3  (il punto interno non compare)


## 4. `CDIST(X, Y, metric)` → handle matrice

IMPORT SPATIAL
IMPORT MATRIX
SUB Main()
    DIM X(1) AS ARRAY OF DOUBLE
    X(0) = {0, 0} : X(1) = {3, 4}
    DIM Y(1) AS ARRAY OF DOUBLE
    Y(0) = {1, 0} : Y(1) = {0, 1}
    m = SPATIAL.CDIST(X, Y, "euclidean")
    PRINT MATRIX.GETVALUE(m, 0, 0)      ' distanza X(0)→Y(0)
END SUB


## 5. `DISTANCEMATRIX(X, Y)` → handle matrice

m = SPATIAL.DISTANCEMATRIX(X, Y)
PRINT MATRIX.GETVALUE(m, 1, 1)


## 6. Esempio completo

## 7. Stato di copertura

### Special-Manual

*Fonte: `Special-Manual`*


## 1. Riferimento funzioni
| NOME | SINTASSI / ESEMPIO | DESCRIZIONE |
|---|---|---|
| `GAMMA` | `SPECIAL.GAMMA(5)` | Funzione Gamma completa Γ(x). |
| `LGAMMA` | `SPECIAL.LGAMMA(5)` | Logaritmo di Γ(x). |
| `BETA` | `SPECIAL.BETA(2, 3)` | Funzione Beta B(a,b). |
| `ERF` | `SPECIAL.ERF(0.5)` | Funzione errore erf(x). |
| `ERFC` | `SPECIAL.ERFC(0.5)` | Complementare 1-erf(x). |
| `GAMMAINC` | `SPECIAL.GAMMAINC(2, 3)` | Incompleta regolarizzata inferiore P(a,x). |
| `GAMMAINCC` | `SPECIAL.GAMMAINCC(2, 3)` | Incompleta regolarizzata superiore Q(a,x). |
| `BETAINC` | `SPECIAL.BETAINC(2, 3, 0.5)` | Beta incompleta regolarizzata I_x(a,b). |
| `DIGAMMA` | `SPECIAL.DIGAMMA(3)` | Funzione digamma ψ(x). |
| `BESSELJ` | `SPECIAL.BESSELJ(1, 2)` | Bessel di I specie J_v(x). |
| `BESSELY` | `SPECIAL.BESSELY(1, 2)` | Bessel di II specie Y_v(x). |
| `BESSELI` | `SPECIAL.BESSELI(1, 2)` | Bessel modificata I_v(x). |
| `BESSELK` | `SPECIAL.BESSELK(1, 2)` | Bessel modificata K_v(x). |
| `BETALN` | `SPECIAL.BETALN(2, 3)` | Logaritmo della funzione Beta. |
| `ERFINV` | `SPECIAL.ERFINV(0.5)` | Inversa di erf(x). |
| `ERFCINV` | `SPECIAL.ERFCINV(0.5)` | Inversa di erfc(x). |
| `GAMMAINCINV` | `SPECIAL.GAMMAINCINV(2, 0.8)` | Inversa di gammainc. |
| `GAMMAINCCINV` | `SPECIAL.GAMMAINCCINV(2, 0.2)` | Inversa di gammaincc. |
| `BETAINCINV` | `SPECIAL.BETAINCINV(2, 3, 0.5)` | Inversa di betainc. |

## 2. Esempio completo

' SPECIAL — nucleo scipy.special (oracolo scipy 1.18, tolleranza 1e-9)
IMPORT SPECIAL

SUB Main()
  PRINT "gamma(5)    ="; SPECIAL.GAMMA(5)      ' 24
  PRINT "gamma(0.5)  ="; SPECIAL.GAMMA(0.5)    ' sqrt(pi)
  PRINT "lgamma(5)   ="; SPECIAL.LGAMMA(5)
  PRINT "beta(2,3)   ="; SPECIAL.BETA(2, 3)
  PRINT "erf(0.5)    ="; SPECIAL.ERF(0.5)
  PRINT "erfc(0.5)   ="; SPECIAL.ERFC(0.5)
  PRINT "gammainc    ="; SPECIAL.GAMMAINC(2, 3)
  PRINT "gammaincc   ="; SPECIAL.GAMMAINCC(2, 3)
  PRINT "betainc     ="; SPECIAL.BETAINC(2, 3, 0.5)
  PRINT "digamma(3)  ="; SPECIAL.DIGAMMA(3)
  PRINT "jv(1,2)     ="; SPECIAL.BESSELJ(1, 2)
  PRINT "yv(1,2)     ="; SPECIAL.BESSELY(1, 2)


## 3. Note e limitazioni

### Stats-Manual

*Fonte: `Stats-Manual`*


## 1. Riferimento funzioni
| NOME | SINTASSI / ESEMPIO | DESCRIZIONE |
|---|---|---|
| `ANOVA` | `STATS.Anova(groups)` | Analisi varianza. |
| `BERNOULLIPMF` | `STATS.BernoulliPmf(1)` | BernoulliPmf di STATS. |
| `BETACDF` | `STATS.BetaCdf(1, 1)` | BetaCdf di STATS. |
| `BETAPDF` | `STATS.BetaPdf(1, 1)` | BetaPdf di STATS. |
| `BETAPPF` | `STATS.BetaPpf(1, 1)` | Quantile beta (inverso CDF). |
| `BINOMIALPMF` | `STATS.BinomialPmf(1, 1)` | BinomialPmf di STATS. |
| `CHI2` | `STATS.Chi2()` | Chi2 di STATS. |
| `CHI2CON` | `STATS.Chi2Con(table)` | Test chi2 di indipendenza (contingenza). |
| `DESCRIBE` | `STATS.Describe(data)` | Statistiche descrittive complete. |
| `GAMMACDF` | `STATS.GammaCdf(a, s, x)` | Gamma CDF. |
| `GAMMAISF` | `STATS.GammaIsf(a, s, p)` | Gamma ISF. |
| `GAMMAPDF` | `STATS.GammaPdf(a, s, x)` | Gamma PDF. |
| `GAMMAPPF` | `STATS.GammaPpf(a, s, p)` | Gamma PPF. |
| `IQR` | `STATS.Iqr(data)` | Intervallo interquartile (Q3−Q1). |
| `JARQUEBERA` | `STATS.JarqueBera(data)` | Test di normalità Jarque-Bera. |
| `KURTOSISTEST` | `STATS.KurtosisTest(data)` | Test di normalità sulla curtosi. |
| `LINREGRESS` | `STATS.LinRegress(x, y)` | Regressione lineare OLS (slope, int, r, p, se). |
| `MANNWHITNEY` | `STATS.MannWhitney(x, y)` | Test U di Mann-Whitney. |
| `MODE` | `STATS.Mode(data)` | Valore più frequente. |
| `MOMENT` | `STATS.Moment(data, order)` | Momento centrale di ordine k. |
| `PEARSONR` | `STATS.PearsonR(x, y)` | Correlazione di Pearson (r, p). |
| `SKEWTEST` | `STATS.SkewTest(data)` | Test di normalità sull'asimmetria. |
| `SPEARMANR` | `STATS.SpearmanR(x, y)` | Correlazione per ranghi (rho, p). |
| `WILCOXON` | `STATS.Wilcoxon(x, y)` | Test dei ranghi con segno. |
| `CHICDF` | `STATS.ChiCdf(x, k)` | Chi CDF = P(k/2, x²/2). |
| `CHIPDF` | `STATS.ChiPdf(x, k)` | Chi PDF (distinta dalla chi-quadro). |
| `CHIPPF` | `STATS.ChiPpf(p, k)` | Quantile chi (inverso CDF). |
| `CHISQUAREDCDF` | `STATS.ChiSquaredCdf(x, k)` | Chi-quadro CDF = P(k/2, x/2). |
| `CHISQUAREDISF` | `STATS.ChiSquaredIsf(p, k)` | ISF chi-quadrato. |
| `CHISQUAREDPPF` | `STATS.ChiSquaredPpf(p, k)` | Quantile chi-quadrato. |
| `CHISQUAREDPDF` | `STATS.ChiSquaredPdf(x, k)` | Chi-quadro PDF. |
| `CORRELATION` | `STATS.Correlation()` | Correlazione. |
| `EXPONENTIALCDF` | `STATS.ExponentialCdf(x, rate)` | ExponentialCdf di STATS. |
| `EXPONENTIALISF` | `STATS.ExponentialIsf(p, rate)` | ISF esponenziale. |
| `EXPONENTIALPPF` | `STATS.ExponentialPpf(p, rate)` | Quantile esponenziale. |
| `EXPONENTIALPDF` | `STATS.ExponentialPdf(x, rate)` | ExponentialPdf di STATS. |
| `FCDF` | `STATS.FCdf(1, 1)` | FCdf di STATS. |
| `FISHER` | `STATS.Fisher(1, 1, 1, 1)` | Fisher di STATS. |
| `FPDF` | `STATS.FPdf(1, 1)` | FPdf di STATS. |
| `FISF` | `STATS.FIsf(1, 1)` | ISF di Fisher. |
| `FPPF` | `STATS.FPpf(1, 1)` | Quantile F (Fisher). |
| `GEOMETRICPMF` | `STATS.GeometricPmf(1)` | GeometricPmf di STATS. |
| `HYPERGEOMETRICPMF` | `STATS.HypergeometricPmf(1, 1, 1)` | HypergeometricPmf di STATS. |
| `KS` | `STATS.Ks()` | Ks di STATS. |
| `KURTOSIS` | `STATS.Kurtosis()` | Curtosi. |
| `LOGISTICPDF` | `STATS.LogisticPdf(x, location, scale)` | LogisticPdf di STATS. |
| `LOGISTICPPF` | `STATS.LogisticPpf(p, location, scale)` | Quantile logistico. |
| `LOGNORMALCDF` | `STATS.LognormalCdf(x, mu, sigma)` | LognormalCdf di STATS. |
| `LOGNORMALPDF` | `STATS.LognormalPdf(x, mu, sigma)` | LognormalPdf di STATS. |
| `LOGNORMALPPF` | `STATS.LognormalPpf(p, mu, sigma)` | Quantile lognormale. |
| `MAD` | `STATS.Mad()` | Deviazione media assoluta. |
| `MEAN` | `STATS.Mean()` | Media aritmetica. |
| `MEDIAN` | `STATS.Median()` | Mediana. |
| `NEGATIVEBINOMIALPMF` | `STATS.NegativeBinomialPmf(x, r, p)` | NegativeBinomialPmf di STATS. |
| `NORMALCDF` | `STATS.NormalCdf(x, mu, sigma)` | NormalCdf di STATS. |
| `NORMALISF` | `STATS.NormalIsf(1, 1)` | ISF normale. |
| `NORMALPPF` | `STATS.NormalPpf(1, 1)` | Quantile normale. |
| `NORMALPDF` | `STATS.NormalPdf(x, mu, sigma)` | NormalPdf di STATS. |
| `NORMALTEST` | `STATS.NormalTest(data)` | Test di normalità D'Agostino-Pearson (n>=20). |
| `PARETOPDF` | `STATS.ParetoPdf(1, 1)` | ParetoPdf di STATS. |
| `PARETOPPF` | `STATS.ParetoPpf(1, 1)` | Quantile Pareto. |
| `PERCENTILE` | `STATS.Percentile(1)` | Percentile di STATS. |
| `POISSONPMF` | `STATS.PoissonPmf(1)` | PoissonPmf di STATS. |
| `SHAPIRO` | `STATS.Shapiro()` | Shapiro di STATS. |
| `SKEWEDNORMALPDF` | `STATS.SkewedNormalPdf(x, location, scale, alpha)` | SkewedNormalPdf di STATS. |
| `SKEWNESS` | `STATS.Skewness()` | Asimmetria. |
| `STDDEV` | `STATS.StdDev()` | Deviazione standard. |
| `STUDENTTCDF` | `STATS.StudentTCdf(x, df)` | StudentTCdf di STATS. |
| `STUDENTTISF` | `STATS.StudentTIsf(1)` | ISF Student-t. |
| `STUDENTTPPF` | `STATS.StudentTPpf(1)` | Quantile Student-t. |
| `STUDENTTPDF` | `STATS.StudentTPdf(x, df)` | StudentTPdf di STATS. |
| `TRIANGULARPDF` | `STATS.TriangularPdf(1, 1, 1)` | TriangularPdf di STATS. |
| `TRIANGULARPPF` | `STATS.TriangularPpf(1, 1, 1)` | Quantile triangolare. |
| `TTEST` | `STATS.TTest()` | TTest di STATS. |
| `TTESTONESAMPLE` | `STATS.TTestOneSample(1)` | TTestOneSample di STATS. |
| `TTESTPAIRED` | `STATS.TTestPaired()` | TTestPaired di STATS. |
| `UNIFORMPDF` | `STATS.UniformPdf(x, a, b)` | UniformPdf di STATS. |
| `UNIFORMPPF` | `STATS.UniformPpf(1, 1)` | Quantile uniforme. |
| `VARIANCE` | `STATS.Variance()` | Varianza. |
| `WEIBULLCDF` | `STATS.WeibullCdf(1, 1)` | WeibullCdf di STATS. |
| `WEIBULLPPF` | `STATS.WeibullPpf(1, 1)` | Quantile Weibull. |
| `WEIBULLPDF` | `STATS.WeibullPdf(1, 1)` | WeibullPdf di STATS. |
| `ZTEST` | `STATS.ZTest(1, 1)` | ZTest di STATS. |

## 2. Esempio completo

' scipy.stats: t-test, anova, chi2, fisher, shapiro, normaltest, ztest, ks
IMPORT STATS
IMPORT MATH

SUB Main()
    DIM x() = {2.0, 4.0, 5.0, 7.0, 9.0}
    DIM y() = {1.0, 3.0, 6.0, 8.0, 10.0}
    DIM paired() = {1.5, 3.5, 5.5, 6.5, 8.5}
    DIM observed() = {16.0, 18.0, 16.0, 14.0, 12.0, 12.0}
    DIM expected() = {16.0, 16.0, 16.0, 16.0, 12.0, 12.0}
    DIM normal() = {0.12, -0.31, 0.45, 1.02, -0.76, 0.33, 0.18, -0.22, 0.57, -0.41}
    ' NORMALTEST richiede n>=20 (contratto ValidateSample): campione dedicato.
    DIM norm20() = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 10.0, 11.0, 12.0, 13.0, 14.0, 15.0, 16.0, 17.0, 18.0, 19.0, 20.0}
    DIM groups(1) AS ARRAY OF DOUBLE
    groups(0) = {2.0, 3.0, 4.0, 5.0}
    groups(1) = {4.0, 5.0, 6.0, 7.0}


## 3. Note e limitazioni

## 4. Parità backend (2026-09-13)

## 5. Sessione 2026-09-16 — blocco VALIDATO DEFINITIVO

### Trig-Guida-Utente

*Fonte: `Trig-Guida-Utente`*


## 1. Funzioni trigonometriche

PRINT SIN(0)   ' 0
PRINT COS(0)   ' 1
PRINT TAN(0)   ' 0


## 2. Arcotangente

### `ATN(x)` — arcotangente (unico nome, `ATN`)

PRINT ATN(0)   ' 0
PRINT ATN(1)   ' 0.785398163397448   (π/4)


## 3. Logaritmo in base 10

### `LOG10(x)` — logaritmo decimale

PRINT LOG10(100)    ' 2
PRINT LOG10(1000)   ' 3


## 4. Verifiche sui quattro backend (2026-09)

## Vedi anche

### Vector-Manual

*Fonte: `Vector-Manual`*


## 1. Riferimento funzioni
| NOME | SINTASSI / ESEMPIO | DESCRIZIONE |
|---|---|---|
| `VECTOR` | `VECTOR.CREATE(n)` | Comando vettore (legacy). |
| `ADD` | `VECTOR.ADD(v, x)` | Somma vettori. |
| `SUB` | `VECTOR.SUB(v, x)` | Sottrae vettori. |
| `MUL` | `VECTOR.MUL(v, x)` | Moltiplica vettori. |
| `DIV` | `VECTOR.DIV(v, x)` | Divide vettori. |
| `SET` | `VECTOR.SET(v, i, x)` | Imposta componenti. |
| `WRAP` | `VECTOR.WRAP(v)` | Avvolge i valori. |
| `ADDWRAP` | `VECTOR.ADDWRAP(v, x)` | Somma con avvolgimento. |
| `FILLRANDOM` | `VECTOR.FILLRANDOM(v)` | Riempie casualmente. |
| `BOUNCE2D` | `VECTOR.BOUNCE2D(v)` | Rimbalzo 2D. |
| `CREATE` | `VECTOR.CREATE(x, y, z, w)` | Crea vettore -> LONG. |
| `DOTPRODUCT` | `VECTOR.DOTPRODUCT(h1, h2)` | Prodotto scalare -> DOUBLE. |
| `NORM` | `VECTOR.NORM(handle)` | Norma vettore -> DOUBLE. |
| `MANHATTAN` | `VECTOR.MANHATTAN(h1, h2)` | Distanza L1 tra vettori (handle). **4/4 backend** (2026-09-12). |
| `CHEBYSHEV` | `VECTOR.CHEBYSHEV(h1, h2)` | Distanza L∞ tra vettori (handle). **4/4 backend** (2026-09-12). |
| `COSINE` | `VECTOR.COSINE(h1, h2)` | Distanza coseno 1-similitudine. **4/4 backend** (2026-09-12). |
| `MINKOWSKI` | `VECTOR.MINKOWSKI(h1, h2, p)` | Distanza Minkowski di ordine p. **4/4 backend** (2026-09-12). |
| `PRODUCT` | `VECTOR.PRODUCT(h1, h2)` | Prodotto scalare. **4/4 backend** (2026-09-12). |
| `EUCLIDEA` | `VECTOR.EUCLIDEA(v)` | Norma euclidea dell'array (sinonimo di `NORM`, forma su array). **4/4 backend** (2026-09-15). |

## 2. Esempio completo

' VECTOR — metriche di distanza (scipy.spatial.distance)
' Input: a=[1,2,3], b=[4,6,8] (handle da VECTOR.CREATE)
' Oracolo: manhattan=12, chebyshev=5, cosine=0.007416666, minkowski(p=3)=6.0
IMPORT VECTOR

SUB Main()
  h1 = VECTOR.CREATE(1, 2, 3)
  h2 = VECTOR.CREATE(4, 6, 8)
  PRINT "manhattan  ="; VECTOR.MANHATTAN(h1, h2)
  PRINT "chebyshev  ="; VECTOR.CHEBYSHEV(h1, h2)
  PRINT "cosine     ="; VECTOR.COSINE(h1, h2)
  PRINT "minkowski  ="; VECTOR.MINKOWSKI(h1, h2, 3)
END SUB


## 3. Note e limitazioni

### Spatial-Manual

*Fonte: `Spatial-Manual`*


## 1. Riferimento funzioni
| NOME | SINTASSI / ESEMPIO | DESCRIZIONE |
|---|---|---|
| `CDIST` | `SPATIAL.CDIST(X, Y, "m")` | Distanze a coppie X×Y (matrice). |
| `CONVEXHULL` | `SPATIAL.CONVEXHULL(X)` | Inviluppo convesso 2D (indici vertici). |
| `DISTANCEMATRIX` | `SPATIAL.DISTANCEMATRIX(X, Y)` | Matrice distanze euclidee. |
| `PDIST` | `SPATIAL.PDIST(X, "m")` | Distanze condensate (triangolo superiore). |

## 2. Esempio completo

' scipy.spatial: pdist/cdist/distance_matrix/ConvexHull
IMPORT SPATIAL
IMPORT MATRIX

SUB Main()
  DIM X(2) AS ARRAY OF DOUBLE
  X(0) = {0, 0} : X(1) = {3, 4} : X(2) = {6, 8}
  DIM Y(0) AS ARRAY OF DOUBLE
  Y(0) = {0, 0}

  DIM pd() = SPATIAL.PDIST(X, "euclidean")   ' distanze a coppie
  PRINT "pdist0    ="; pd(0)
  cdm = SPATIAL.CDIST(X, Y, "euclidean")      ' matrice MxN (handle MATRIX)
  PRINT "cdist10   ="; MATRIX.GETVALUE(cdm, 1, 0)
  dm = SPATIAL.DISTANCEMATRIX(X, Y)
  PRINT "distmat10 ="; MATRIX.GETVALUE(dm, 1, 0)


## 3. Note e limitazioni

### Stats-Guida-Utente

*Fonte: `Stats-Guida-Utente`*


## 1. Panoramica

IMPORT STATS AS S
SUB Main()
    DIM d(4) AS DOUBLE = {5, 2, 9, 1, 7}
    PRINT S.MEAN(d)
END SUB


## 2. Statistiche descrittive
| Funzione | Risultato su `d` |
|---|---|
| `STATS.MEAN(d)` | 4.8 |
| `STATS.MEDIAN(d)` | 5 |
| `STATS.STDDEV(d)` | 3.3466401061363 |
| `STATS.VARIANCE(d)` | 11.2 |
| `STATS.KURTOSIS(d)` | -1.97544642857143 |
| `STATS.MAD(d)` | 3 |
| `STATS.SKEWNESS(d)` | 0.088041393098 |
| `STATS.CORRELATION(d, e)` | -0.058600903867 |
| `STATS.PERCENTILE(d, 50)` | 5 |
| `STATS.PERCENTILE(d, 100)` | 9 |

IMPORT STATS AS S
SUB Main()
    DIM d(4) AS DOUBLE = {5, 2, 9, 1, 7}
    DIM e(4) AS DOUBLE = {9, 4, 3, 6, 8}
    PRINT S.MEAN(d)
    PRINT S.MEDIAN(d)
    PRINT S.STDDEV(d)
    PRINT S.VARIANCE(d)
    PRINT S.KURTOSIS(d)
    PRINT S.MAD(d)
    PRINT ROUND(S.SKEWNESS(d), 12)
    PRINT ROUND(S.CORRELATION(d, e), 12)
    PRINT S.PERCENTILE(d, 50)
    PRINT S.PERCENTILE(d, 100)
END SUB


 4.8
 5
 3.3466401061363
 11.2
 -1.97544642857143
 3
 0.088041393098
 -0.058600903867
 5
 9


## 3. Statistiche su array (ARRAYS)

IMPORT ARRAYS AS AR
SUB Main()
    DIM d(4) AS DOUBLE = {5, 2, 9, 1, 7}
    PRINT AR.AMAX(d)   ' 9
    PRINT AR.AMIN(d)   ' 1
    PRINT AR.ASUM(d)   ' 24
END SUB


## 4. Vedi anche

<!-- END SURFACE -->
