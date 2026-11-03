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

`ANALYSIS` offre media, mediana, varianza, deviazione standard, MAD, asimmetria, curtosi,
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
| matrici e algebra | `MATRIX`, `Matrix-Linalg-Manual` | prodotto, inversa, autovalori, decomposizioni |
| vettori | `VECTOR` | norme, distanze, prodotto scalare |
| statistica | `ANALYSIS`, `Stats-Manual` | descrittiva, test, distribuzioni |
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

## 7. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| percentile inatteso | convenzione o difetto del backend | verifica su un altro backend, non "aggiusta" l'input |
| asimmetria diversa da 0 su dati simmetrici | idem | confronta con SciPy (`StatsSciPyParity`) |
| varianza "sbagliata" | divisore `n` contro `n-1` | controlla la convenzione della guida |
| `NaN` in un risultato | dominio non valido (log di 0, sqrt negativo) | valida l'input prima |
| confronto fra double che fallisce | uguaglianza esatta | usa una tolleranza |

## 8. Dove andare adesso

- **V.28** — le stesse formule applicate a serie temporali e trading.
- **V.31** — TORCH, quando la matematica diventa tensori.
- **III.15** — come si scrive il test di una funzione numerica.













<!-- BEGIN DOMAIN TABLE -->
## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **ANALYSIS.ADFTEST(x1)** — `adf = ANALYSIS.AdfTest(d)` (shared/samples/Analysis/AnalysisMiniTest.bas:12)
- **ANALYSIS.ANOVA(x1)** — `aov = ANALYSIS.ANOVA(g)` (shared/samples/Analysis/AnalysisUncoveredCoverage.bas:59)
- **ANALYSIS.ARIMA(x1, x2, x3, x4)** — `DIM ArimaRes = ANALYSIS.ARIMA(SerieTemporale, 1, 1, 1)` (shared/samples/Analysis/AnalysisTests.bas:66)
- **ANALYSIS.CORRELATION(x1, x2)** — `DIM Corr = ANALYSIS.CORRELATION(Dataset1, Dataset2)` (shared/samples/Analysis/AnalysisStatsDescriptive.bas:61)
- **ANALYSIS.CUBICSPLINE(x1, x2, x3)** — `PRINT xn; "  "; ANALYSIS.INTERPOLATELINEAR(x, y, xn); "  "; ANALYSIS.CUBICSPLINE(x, y, xn); "  "; AN…` (shared/samples/Analysis/AnalysisInterpReal.bas:14)
- **ANALYSIS.CUMTRAPEZOID(x1, x2)** — `DIM ct() = ANALYSIS.CUMTRAPEZOID(yy, xx)` (shared/samples/Math/IntegrateInterpFftSciPyParity.bas:15)
- **ANALYSIS.DIFFERENTIATE(x1, x2)** — `DIM Derivata = ANALYSIS.DIFFERENTIATE(FN(x) => x*x, 2)` (shared/samples/Analysis/AnalysisTests.bas:134)
- **ANALYSIS.EUCLIDEANDISTANCESQUARED(x1, x2)** — `PRINT "euclid: "; ANALYSIS.EUCLIDEANDISTANCESQUARED(x, y)` (shared/samples/Analysis/AnalysisInterpReal.bas:17)
- **ANALYSIS.FFT(x1)** — `DIM FftRes() = ANALYSIS.FFT(Signal)` (shared/samples/Analysis/AnalysisTests.bas:86)
- **ANALYSIS.FFTCOMPLEX(x1, x2)** — `DIM spettro() = ANALYSIS.FFTCOMPLEX(real, imag)` (shared/samples/Analysis/AnalysisUncoveredCoverage.bas:34)
- **ANALYSIS.FFTFREQ(x1, x2)** — `DIM f6() = ANALYSIS.FFTFREQ(6, 1)` (shared/samples/Math/IntegrateInterpFftSciPyParity.bas:23)
- **ANALYSIS.FFTSHIFT(x1)** — `DIM fs() = ANALYSIS.FFTSHIFT(a6)` (shared/samples/Math/IntegrateInterpFftSciPyParity.bas:26)
- **ANALYSIS.HOLTWINTERS(x1, x2, x3, x4, x5)** — `DIM HwRes = ANALYSIS.HOLTWINTERS(SerieTemporale, 0.2, 0.1, 0.1, 10)` (shared/samples/Analysis/AnalysisTests.bas:61)
- **ANALYSIS.IFFT(x1)** — `DIM IfftRes() = ANALYSIS.IFFT(FftRes)` (shared/samples/Analysis/AnalysisTests.bas:94)
- **ANALYSIS.IFFTCOMPLEX(x1, x2)** — `DIM ii() = ANALYSIS.IFFTCOMPLEX(real, imag)` (shared/samples/Analysis/AnalysisUncoveredCoverage.bas:37)
- **ANALYSIS.IFFTSHIFT(x1)** — `DIM ifs() = ANALYSIS.IFFTSHIFT(a6)` (shared/samples/Math/IntegrateInterpFftSciPyParity.bas:28)
- **ANALYSIS.INTEGRATE(x1, x2, x3)** — `DIM Integrale = ANALYSIS.INTEGRATE(FN(x) => x*x, 0, 1)` (shared/samples/Analysis/AnalysisTests.bas:130)
- **ANALYSIS.INTERPOLATECLAMP(x1, x2, x3)** — `DIM y_clamp = ANALYSIS.INTERPOLATECLAMP(DatiX, DatiY, x_new)` (shared/samples/Analysis/AnalysisTests.bas:111)
- **ANALYSIS.INTERPOLATEGENERIC(x1, x2, x3, x4)** — `DIM y_gen = ANALYSIS.INTERPOLATEGENERIC("linear", DatiX, DatiY, x_new)` (shared/samples/Analysis/AnalysisTests.bas:112)
- **ANALYSIS.INTERPOLATELINEAR(x1, x2, x3)** — `PRINT xn; "  "; ANALYSIS.INTERPOLATELINEAR(x, y, xn); "  "; ANALYSIS.CUBICSPLINE(x, y, xn); "  "; AN…` (shared/samples/Analysis/AnalysisInterpReal.bas:14)
- **ANALYSIS.INTERPOLATEPCHIP(x1, x2, x3)** — `PRINT xn; "  "; ANALYSIS.INTERPOLATELINEAR(x, y, xn); "  "; ANALYSIS.CUBICSPLINE(x, y, xn); "  "; AN…` (shared/samples/Analysis/AnalysisInterpReal.bas:14)
- **ANALYSIS.INTERPOLATEPOLYNOMIAL(x1, x2, x3)** — `PRINT xn; "  "; ANALYSIS.INTERPOLATELINEAR(x, y, xn); "  "; ANALYSIS.CUBICSPLINE(x, y, xn); "  "; AN…` (shared/samples/Analysis/AnalysisInterpReal.bas:14)
- **ANALYSIS.INTERPOLATESPLINE(x1, x2, x3)** — `PRINT "spline@2.5: "; ANALYSIS.INTERPOLATESPLINE(x, y, 2.5)` (shared/samples/Analysis/AnalysisInterpReal.bas:16)
- **ANALYSIS.INTERPOLATESTEP(x1, x2, x3)** — `PRINT xn; "  "; ANALYSIS.INTERPOLATELINEAR(x, y, xn); "  "; ANALYSIS.CUBICSPLINE(x, y, xn); "  "; AN…` (shared/samples/Analysis/AnalysisInterpReal.bas:14)
- **ANALYSIS.IRFFT(x1, x2)** — `DIM ir() = ANALYSIS.Irfft(re, im)` (shared/samples/Analysis/AnalysisParityP0.bas:21)
- **ANALYSIS.KMEANS(x1, x2)** — `DIM KRes = ANALYSIS.KMEANS(ClusterData, 2) ' Cerchiamo 2 cluster` (shared/samples/Analysis/AnalysisTests.bas:78)
- **ANALYSIS.KURTOSIS(x1)** — `PRINT "Curtosi (Kurtosis):   "; MATH.ROUND(ANALYSIS.KURTOSIS(Dataset1), 4)` (shared/samples/Analysis/AnalysisStatsDescriptive.bas:59)
- **ANALYSIS.MACOEFFICIENTS(x1, x2)** — `DIM ma() = ANALYSIS.MACOEFFICIENTS(d, 2)` (shared/samples/Analysis/AnalysisInterpReal.bas:21)
- **ANALYSIS.MAD(x1)** — `PRINT ANALYSIS.MAD(d)` (shared/minitests/analysis/desc.bas:10)
- **ANALYSIS.MEAN(x1)** — `PRINT "Media (Mean):         "; ANALYSIS.MEAN(Dataset1)` (shared/samples/Analysis/AnalysisStatsDescriptive.bas:43)
- **ANALYSIS.MEDIAN(x1)** — `PRINT "Mediana (Median):     "; ANALYSIS.MEDIAN(Dataset1)` (shared/samples/Analysis/AnalysisStatsDescriptive.bas:44)
- **ANALYSIS.PCA(x1)** — `p = ANALYSIS.Pca(data)` (shared/samples/Analysis/AnalysisMiniTest.bas:27)
- **ANALYSIS.REGRESS(x1, x2)** — `RegRes = ANALYSIS.REGRESS(DatiX, DatiY)` (shared/samples/Analysis/AnalysisTests.bas:44)
- **ANALYSIS.RFFT(x1)** — `DIM rf = ANALYSIS.Rfft(x)` (shared/samples/Analysis/AnalysisParityP0.bas:10)
- **ANALYSIS.SIMPSON(x1, x2, x3, x4)** — `PRINT "simpson     ="; ANALYSIS.SIMPSON(FN(x) => x * x, 0, 1, 256)` (shared/samples/Math/IntegrateInterpFftSciPyParity.bas:13)
- **ANALYSIS.SKEWNESS(x1)** — `PRINT "Asimmetria (Skewness):"; MATH.ROUND(ANALYSIS.SKEWNESS(DatasetSkew), 4)` (shared/samples/Analysis/AnalysisStatsDescriptive.bas:58)
- **ANALYSIS.SOLVEIVP(x1, x2, x3, x4)** — `DIM npts = ANALYSIS.SolveIvp({|t, y| RETURN -1}, y0, 0.0, 1.0)` (shared/samples/Analysis/AnalysisParityP0.bas:15)
- **ANALYSIS.SOLVEODE(x1, x2, x3, x4, x5)** — `DIM ys() = ANALYSIS.SOLVEODE({ |t, y| RETURN -y }, 1.0, 0.0, 1.0, 10)` (shared/samples/Math/IntegrateInterpFftSciPyParity.bas:18)
- **ANALYSIS.SPLEV(x1, x2)** — `DIM sv() = ANALYSIS.Splev(sp, xq)` (shared/samples/Analysis/AnalysisParityP0.bas:30)
- **ANALYSIS.SPLREP(x1, x2)** — `DIM sp = ANALYSIS.Splrep(xs, ys)` (shared/samples/Analysis/AnalysisParityP0.bas:27)
- **ANALYSIS.STDDEV(x1)** — `PRINT "Dev. Standard (StdDev): "; MATH.ROUND(ANALYSIS.STDDEV(Dataset1), 4)` (shared/samples/Analysis/AnalysisStatsDescriptive.bas:51)
- **ANALYSIS.TRAPEZOID(x1, x2)** — `PRINT "trapezoid   ="; ANALYSIS.TRAPEZOID(yy, xx)` (shared/samples/Math/IntegrateInterpFftSciPyParity.bas:14)
- **ANALYSIS.VARIANCE(x1)** — `PRINT "Varianza (Variance):  "; MATH.ROUND(ANALYSIS.VARIANCE(Dataset1), 4)` (shared/samples/Analysis/AnalysisStatsDescriptive.bas:50)
- **ANALYSIS.YULEWALKER(x1, x2)** — `DIM yw() = ANALYSIS.YULEWALKER(d, 2)` (shared/samples/Analysis/AnalysisInterpReal.bas:19)
- **GRAPH.BETWEENNESS(x1)** — `PRINT GRAPH.BETWEENNESS("graph-vault")` (shared/samples/graph/GraphFunctions.bas:46)
- **GRAPH.BFS(x1, x2)** — `PRINT "bfs="; GRAPH.BFS("graph-vault", "index.md")` (shared/samples/graph/GraphAlgorithms.bas:24)
- **GRAPH.BIPARTITE(x1)** — `PRINT GRAPH.BIPARTITE("graph-vault")` (shared/samples/graph/GraphFunctions.bas:43)
- **GRAPH.BYFOLDER(x1, x2)** — `PRINT GRAPH.BYFOLDER("graph-vault", "strategies")` (shared/samples/graph/GraphFunctions.bas:68)
- **GRAPH.BYTAG(x1, x2)** — `PRINT GRAPH.BYTAG("graph-vault", "strategy")` (shared/samples/graph/GraphFunctions.bas:65)
- **GRAPH.CLUSTERING(x1, x2)** — `PRINT GRAPH.CLUSTERING("graph-vault", "index.md")` (shared/samples/graph/GraphFunctions.bas:49)
- **GRAPH.CONNECTED(x1)** — `PRINT GRAPH.CONNECTED("graph-vault")` (shared/samples/graph/GraphFunctions.bas:31)
- **GRAPH.DFS(x1, x2)** — `PRINT "dfs="; GRAPH.DFS("graph-vault", "index.md")` (shared/samples/graph/GraphAlgorithms.bas:27)
- **GRAPH.DIAMETER(x1)** — `PRINT GRAPH.DIAMETER("graph-vault")` (shared/samples/graph/GraphFunctions.bas:52)
- **GRAPH.DIJKSTRA(x1, x2, x3)** — `PRINT "dijkstra="; GRAPH.DIJKSTRA("graph-vault", "index.md", "strategies/mean-reversion.md")` (shared/samples/graph/GraphAlgorithms.bas:21)
- **GRAPH.DOT(x1)** — `PRINT GRAPH.DOT("graph-vault")` (shared/samples/graph/GraphFunctions.bas:59)
- **GRAPH.ECCENTRICITY(x1, x2)** — `PRINT "ecc="; GRAPH.ECCENTRICITY("graph-vault", "index.md")` (shared/samples/graph/GraphAlgorithms.bas:31)
- **GRAPH.HASCYCLE(x1)** — `PRINT "hasCycle="; GRAPH.HASCYCLE("graph-vault")` (shared/samples/graph/GraphAlgorithms.bas:17)
- **GRAPH.ISVALID(x1)** — `PRINT GRAPH.ISVALID("graph-vault")` (shared/samples/graph/GraphFunctions.bas:71)
- **GRAPH.LOUVAIN(x1)** — `PRINT GRAPH.LOUVAIN("graph-vault")` (shared/samples/graph/GraphLouvainMaxFlow.bas:14)
- **GRAPH.MAXFLOW(x1, x2, x3)** — `PRINT GRAPH.MAXFLOW("graph-vault", "index.md", "airline/load-factor.md")` (shared/samples/graph/GraphLouvainMaxFlow.bas:17)
- **GRAPH.MST(x1)** — `PRINT GRAPH.MST("graph-vault")` (shared/samples/graph/GraphFunctions.bas:55)
- **GRAPH.NEIGHBORS(x1, x2)** — `PRINT GRAPH.NEIGHBORS("graph-vault", "index.md")` (shared/samples/graph/GraphFunctions.bas:22)
- **GRAPH.PAGERANK(x1)** — `PRINT GRAPH.PAGERANK("graph-vault", 3)` (shared/samples/graph/GraphFunctions.bas:28)
- **GRAPH.PATH(x1, x2, x3)** — `PRINT GRAPH.PATH("graph-vault", "index.md", "trading/momentum.md")` (shared/samples/graph/GraphFunctions.bas:25)
- **GRAPH.QUERYMULTI(x1)** — `DIM g1$ = GRAPH.QUERYMULTI("BlackScholes", "finance")` (shared/samples/graph/FinanceBrainGraph.bas:33)
- **GRAPH.STATS(x1)** — `PRINT GRAPH.STATS("graph-vault")` (shared/samples/graph/GraphFunctions.bas:34)
- **GRAPH.STRONGLYCONNECTED(x1)** — `PRINT GRAPH.STRONGLYCONNECTED("graph-vault")` (shared/samples/graph/GraphFunctions.bas:37)
- **GRAPH.SUBGRAPH(x1, x2)** — `PRINT GRAPH.SUBGRAPH("graph-vault", "index.md", 1)` (shared/samples/graph/GraphFunctions.bas:62)
- **GRAPH.TOPOSORT(x1)** — `PRINT GRAPH.TOPOSORT("graph-vault")` (shared/samples/graph/GraphFunctions.bas:40)
- **MATH.ABS(x1)** — `PRINT "ABS(-10.5): "; MATH.ABS(-10.5)` (shared/samples/Math/MathLogarithms.bas:75)
- **MATH.ACOS(x1)** — `PRINT "Arcocoseno (ACOS) di 0:     "; MATH.ACOS(0); " (rad)"` (shared/samples/Math/MathTrigonometry.bas:48)
- **MATH.ANGLE2D(x1, x2, x3, x4)** — `DIM calcolatoDeg = MATH.RAD2DEG(MATH.ANGLE2D(0, 0, x, y))` (shared/samples/Math/MathCombinatorics.bas:60)
- **MATH.ASIN(x1)** — `PRINT "Arcoseno (ASIN) di 1:       "; MATH.ASIN(1); " (rad)"` (shared/samples/Math/MathTrigonometry.bas:45)
- **MATH.ATN(x1)** — `PRINT "ATN(1)  = "; ROUND(MATH.RAD2DEG(MATH.ATN(1)), 6); " deg"` (shared/samples/Math/MathAtnVectorAngle.bas:29)
- **MATH.AVG()** — `PRINT "AVG = "; MATH.AVG(v)` (shared/samples/Math/MathFullCoverage.bas:19)
- **MATH.COSH(x1)** — `PRINT "Coseno Iperbolico (COSH) di 1:  "; MATH.COSH(1)` (shared/samples/Math/MathTrigonometry.bas:64)
- **MATH.CEILING(x1)** — `PRINT "  CEILING(num):  "; MATH.CEILING(num_pos)` (shared/samples/Math/MathLogarithms.bas:60)
- **MATH.CLAMP(x1, x2, x3)** — `PRINT "CLAMP(150, 0, 100): "; MATH.CLAMP(val, 0, 100); " (Atteso 100)"` (shared/samples/Math/MathCombinatorics.bas:73)
- **MATH.COMB(x1, x2)** — `PRINT "COMB("; n; ", "; k; "):   "; MATH.COMB(n, k)` (shared/samples/Math/MathCombinatorics.bas:40)
- **MATH.COS(x1)** — `PRINT "Coseno (COS):     "; MATH.COS(angle_rad)` (shared/samples/Math/MathTrigonometry.bas:34)
- **MATH.DEG2RAD(x1)** — `DIM angoloRad = MATH.DEG2RAD(angoloDeg)` (shared/samples/Math/MathCombinatorics.bas:48)
- **MATH.DIST2D(x1, x2, x3, x4)** — `830 PRINT "DIST2D(0, 0, 3, 4) =", DIST2D(0, 0, 3, 4); "(Atteso: 5)"` (shared/samples/Types/TypesConversions.bas:137)
- **MATH.DIST3D(x1, x2, x3, x4, x5, x6)** — `840 PRINT "DIST3D(0, 0, 0, 1, 2, 2) =", DIST3D(0, 0, 0, 1, 2, 2); "(Atteso: 3)"` (shared/samples/Types/TypesConversions.bas:138)
- **MATH.EXP(x1)** — `DIM ModelFunc = FN(params() AS DOUBLE, x AS DOUBLE) AS DOUBLE => params(0) * Math.Exp(params(1) * x)` (shared/samples/Optimization/OptimizationTests.bas:134)
- **MATH.FACT(x1)** — `PRINT "FACT("; n; "):      "; MATH.FACT(n)` (shared/samples/Math/MathCombinatorics.bas:32)
- **MATH.FIX(x1)** — `PRINT "  FIX(num):      "; MATH.FIX(num_pos)` (shared/samples/Math/MathLogarithms.bas:57)
- **MATH.FLOOR(x1)** — `PRINT "  FLOOR(num):    "; MATH.FLOOR(num_pos)` (shared/samples/Math/MathLogarithms.bas:59)
- **MATH.HYPOT(x1, x2)** — `PRINT "HYPOT(3, 4):        "; MATH.HYPOT(3, 4)` (shared/samples/Math/MathCombinatorics.bas:69)
- **MATH.INT(x1)** — `PRINT "  INT(num):      "; MATH.INT(num_pos)` (shared/samples/Math/MathLogarithms.bas:58)
- **MATH.ISNUMERIC(x1)** — `IF MATH.ISNUMERIC(Input) THEN` (shared/samples/Types/TypesAny.bas:91)
- **MATH.LOG(x1)** — `PRINT "LOG(e):                     "; MATH.LOG(e); " (Atteso: 1)"` (shared/samples/Math/MathLogarithms.bas:35)
- **MATH.LOG10(x1)** — `PRINT "LOG10(1000):                "; MATH.LOG10(1000); " (Atteso: 3)"` (shared/samples/Math/MathLogarithms.bas:38)
- **MATH.MAX()** — `GmbStats = "mean=" + STR$(STATS.MEAN(n)) + " max=" + STR$(MATH.MAX(n[0], n[1], n[2])) + " min=" + ST…` (shared/samples/Ai/McpCapabilityServer.bas:51)
- **MATH.MIN()** — `PRINT "MIN(3, 1, 4, 1, 5, 9): "; MATH.MIN(3, 1, 4, 1, 5, 9) ' Atteso: 1` (shared/samples/Math/MathExtended.bas:44)
- **MATH.POLAR_X(x1, x2)** — `DIM x = MATH.POLAR_X(angoloRad, raggio)` (shared/samples/Math/MathCombinatorics.bas:52)
- **MATH.POLAR_Y(x1, x2)** — `DIM y = MATH.POLAR_Y(angoloRad, raggio)` (shared/samples/Math/MathCombinatorics.bas:53)
- **MATH.PERM(x1, x2)** — `PRINT "PERM("; n; ", "; k; "):   "; MATH.PERM(n, k)` (shared/samples/Math/MathCombinatorics.bas:36)
- **MATH.PI()** — `SerieStagionale(si) = (si * 0.1) + SIN(si * 2 * MATH.PI() / 12) * 5 + (MATH.RND() - 0.5)` (shared/samples/TimeSeries/TimeSeriesAdvanced.bas:64)
- **MATH.PYTHAGORAS(x1, x2)** — `820 PRINT "PYTHAGORAS(3, 4) =", PYTHAGORAS(3, 4); "(Atteso: 5)"` (shared/samples/Types/TypesConversions.bas:136)
- **MATH.RAD2DEG(x1)** — `DIM calcolatoDeg = MATH.RAD2DEG(MATH.ANGLE2D(0, 0, x, y))` (shared/samples/Math/MathCombinatorics.bas:60)
- **MATH.RANGE()** — `PRINT "MATH.RANGE — Sequenze numeriche"` (shared/samples/Math/RangeDemo.bas:13)
- **MATH.RND()** — `Prices(i) = Prices(i-1) * (1 + (MATH.RND() - 0.5) * 0.02)` (shared/samples/TimeSeries/TimeSeriesTests.bas:35)
- **MATH.ROUND(x1)** — `PRINT "Prezzo["; lastIdx; "]:     "; MATH.ROUND(Prices(lastIdx), 2)` (shared/samples/TimeSeries/TimeSeriesTests.bas:50)
- **MATH.SINH(x1)** — `PRINT "Seno Iperbolico (SINH) di 1:    "; MATH.SINH(1)` (shared/samples/Math/MathTrigonometry.bas:63)
- **MATH.SGN(x1)** — `PRINT "SGN(-10.5): "; MATH.SGN(-10.5)` (shared/samples/Math/MathLogarithms.bas:76)
- **MATH.SIN(x1)** — `PRINT "Seno (SIN):       "; MATH.SIN(angle_rad)` (shared/samples/Math/MathTrigonometry.bas:33)
- **MATH.SQR(x1)** — `PRINT "SQR(25):                    "; MATH.SQR(25); " (Atteso: 5)"` (shared/samples/Math/MathLogarithms.bas:44)
- **MATH.SUM()** — `SPEED SELECT Clienti.Nome AS NomeCliente, SUM(Ordini.Importo) AS TotaleImporto INTO ris() OF Risulta…` (shared/samples/Databases/DbSpeedFullTests.bas:261)
- **MATH.TANH(x1)** — `PRINT "Tangente Iperbolica (TANH) di 1: "; MATH.TANH(1)` (shared/samples/Math/MathTrigonometry.bas:65)
- **MATH.TAN(x1)** — `PRINT "Tangente (TAN):   "; MATH.TAN(angle_rad)` (shared/samples/Math/MathTrigonometry.bas:35)
- **MATH.TRUNC(x1)** — `PRINT "  TRUNC(num, 2): "; MATH.TRUNC(num_pos, 2)` (shared/samples/Math/MathLogarithms.bas:56)
- **MATRIX.ABOVEMASK(x1, x2)** — `DIM flatMask() = MATRIX.ABOVEMASK(rowsBroadcast, 15.0)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:52)
- **MATRIX.ABOVEMATRIX(x1, x2)** — `DIM matrixMask() = MATRIX.ABOVEMATRIX(rowsBroadcast, 15.0)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:53)
- **MATRIX.ADDCOLVECTOR(x1, x2)** — `DIM columnsBroadcast = MATRIX.ADDCOLVECTOR(m, columnVector)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:43)
- **MATRIX.ADDROWVECTOR(x1, x2)** — `DIM rowsBroadcast = MATRIX.ADDROWVECTOR(m, rowVector)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:42)
- **MATRIX.ARGMAX(x1)** — `DIM amax() = MATRIX.ARGMAX(m)` (shared/samples/Arrays/MatrixArgmaxShadow.bas:21)
- **MATRIX.ARGMIN(x1)** — `DIM amin() = MATRIX.ARGMIN(m)` (shared/samples/Arrays/MatrixArgmaxShadow.bas:22)
- **MATRIX.ASCSV(x1)** — `PRINT "ASCSV="; MATRIX.ASCSV(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:58)
- **MATRIX.ASJSON(x1)** — `PRINT "ASJSON="; MATRIX.ASJSON(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:59)
- **MATRIX.BLOCKDIAG(x1, x2)** — `bd = MATRIX.BLOCKDIAG(h, h)` (shared/samples/Math/MatrixFunctionsSciPyParity.bas:28)
- **MATRIX.CHOLESKY(x1)** — `DIM ch = MATRIX.CHOLESKY(a)` (shared/samples/Matrix/MatrixDecompositions.bas:32)
- **MATRIX.CIRCULANT(x1)** — `cc = MATRIX.CIRCULANT(c1)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:50)
- **MATRIX.CLONEMATRIX(x1)** — `DIM cloned = MATRIX.CLONEMATRIX(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:61)
- **MATRIX.COLMEANS(x1)** — `DIM meansByColumn() = MATRIX.COLMEANS(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:14)
- **MATRIX.COLSUMS(x1)** — `DIM sumsByColumn() = MATRIX.COLSUMS(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:12)
- **MATRIX.COLUMNCOUNT(x1)** — `PRINT "COLUMNCOUNT="; MATRIX.COLUMNCOUNT(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:65)
- **MATRIX.COLVARIANCES(x1)** — `DIM variancesByColumn() = MATRIX.COLVARIANCES(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:16)
- **MATRIX.CONCATCOLS(x1, x2)** — `DIM columnsJoined = MATRIX.CONCATCOLS(m, m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:41)
- **MATRIX.CONCATROWS(x1, x2)** — `DIM rowsJoined = MATRIX.CONCATROWS(m, m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:40)
- **MATRIX.COND(x1)** — `PRINT "cond     ="; MATRIX.COND(g)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:38)
- **MATRIX.COSM(x1)** — `c = MATRIX.COSM(h)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:12)
- **MATRIX.CREATE(x1, x2)** — `h = MATRIX.CREATE(2, 2)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:8)
- **MATRIX.DETERMINANT(x1)** — `PRINT "det(expm(I)) ="; MATRIX.DETERMINANT(e)` (shared/samples/Math/MatrixFuncSciPyParity.bas:10)
- **MATRIX.EIG(x1)** — `ed = MATRIX.EIG(h)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:63)
- **MATRIX.EVD(x1)** — `DIM evd = MATRIX.EVD(a)` (shared/samples/Matrix/MatrixDecompositions.bas:38)
- **MATRIX.EXPM(x1)** — `e = MATRIX.EXPM(h)` (shared/samples/Math/MatrixFuncSciPyParity.bas:9)
- **MATRIX.FACTOR(x1, x2)** — `pu = MATRIX.FACTOR(pol, "U")` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:34)
- **MATRIX.FRACTIONALPOWER(x1, x2)** — `fp = MATRIX.FRACTIONALPOWER(h, 0.5)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:20)
- **MATRIX.FROBENIUSNORM(x1)** — `PRINT "FROBENIUS="; MATRIX.FROBENIUSNORM(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:26)
- **MATRIX.GETVALUE(x1, x2, x3)** — `FOR k = 0 TO 4: re(k) = MATRIX.GETVALUE(rf, k, 0): im(k) = MATRIX.GETVALUE(rf, k, 1): NEXT` (shared/samples/Analysis/AnalysisMiniTest.bas:20)
- **MATRIX.HANKEL(x1, x2)** — `hk = MATRIX.HANKEL(c1, r1)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:48)
- **MATRIX.HESSENBERG(x1)** — `hh = MATRIX.HESSENBERG(h3)` (shared/samples/Math/MatrixFunctionsSciPyParity.bas:24)
- **MATRIX.INFINITYNORM(x1)** — `PRINT "INFINITYNORM="; MATRIX.INFINITYNORM(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:28)
- **MATRIX.INVERSE(x1)** — `DIM inv = MATRIX.INVERSE(a)` (shared/samples/Matrix/MatrixDecompositions.bas:22)
- **MATRIX.ISLOWERTRIANGULAR(x1)** — `PRINT "LOWER="; MATRIX.ISLOWERTRIANGULAR(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:32)
- **MATRIX.ISORTHOGONAL(x1)** — `PRINT "ORTHOGONAL="; MATRIX.ISORTHOGONAL(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:30)
- **MATRIX.ISSYMMETRIC(x1)** — `PRINT "SYMMETRIC="; MATRIX.ISSYMMETRIC(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:29)
- **MATRIX.ISUPPERTRIANGULAR(x1)** — `PRINT "UPPER="; MATRIX.ISUPPERTRIANGULAR(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:31)
- **MATRIX.KRONECKER(x1, x2)** — `k = MATRIX.KRONECKER(h, h)` (shared/samples/Math/MatrixFuncSciPyParity.bas:11)
- **MATRIX.LOADCSV(x1)** — `DIM csvMatrix = MATRIX.LOADCSV("samples/Fixtures/MatrixIdentity.csv")` (shared/samples/Matrix/MatrixCommonLibSimd.bas:70)
- **MATRIX.LOADJSON(x1)** — `DIM jsonMatrix = MATRIX.LOADJSON("samples/Fixtures/MatrixIdentity.json")` (shared/samples/Matrix/MatrixCommonLibSimd.bas:71)
- **MATRIX.LOGM(x1)** — `l = MATRIX.LOGM(h)` (shared/samples/Math/MatrixFunctionsSciPyParity.bas:15)
- **MATRIX.LU(x1)** — `DIM lu = MATRIX.LU(a)` (shared/samples/Matrix/MatrixDecompositions.bas:41)
- **MATRIX.MAXVALUE(x1)** — `PRINT "MAXVALUE="; MATRIX.MAXVALUE(rowsBroadcast)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:54)
- **MATRIX.MINVALUE(x1)** — `PRINT "MINVALUE="; MATRIX.MINVALUE(rowsBroadcast)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:55)
- **MATRIX.MULTIPLY(x1, x2)** — `DIM mul = MATRIX.MULTIPLY(a, a)` (shared/samples/Matrix/MatrixDecompositions.bas:28)
- **MATRIX.MULTIPLYLEFTVECTOR(x1, x2)** — `DIM leftProduct() = MATRIX.MULTIPLYLEFTVECTOR(m, columnVector)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:63)
- **MATRIX.MULTIPLYVECTOR(x1, x2)** — `DIM rightProduct() = MATRIX.MULTIPLYVECTOR(m, rowVector)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:62)
- **MATRIX.NORM(x1)** — `PRINT "norm="; MATH.ROUND(MATRIX.NORM(a), 6)` (shared/samples/Matrix/MatrixDecompositions.bas:19)
- **MATRIX.NUCLEARNORM(x1)** — `PRINT "nuc      ="; MATRIX.NUCLEARNORM(g)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:39)
- **MATRIX.NULLSPACE(x1)** — `DIM ns = MATRIX.NULLSPACE(a)` (shared/samples/Matrix/MatrixDecompositions.bas:87)
- **MATRIX.ONENORM(x1)** — `PRINT "ONENORM="; MATRIX.ONENORM(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:27)
- **MATRIX.ORTH(x1)** — `o = MATRIX.ORTH(g)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:28)
- **MATRIX.PINVERSE(x1)** — `p = MATRIX.PINVERSE(h)` (shared/samples/Math/MatrixFunctionsSciPyParity.bas:33)
- **MATRIX.POLAR(x1)** — `pol = MATRIX.POLAR(g)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:33)
- **MATRIX.QR(x1)** — `DIM qr = MATRIX.QR(a)` (shared/samples/Matrix/MatrixDecompositions.bas:44)
- **MATRIX.RANK(x1)** — `PRINT "rank="; MATRIX.RANK(a)` (shared/samples/Matrix/MatrixDecompositions.bas:18)
- **MATRIX.RESHAPE(x1, x2, x3)** — `DIM reshaped = MATRIX.RESHAPE(m, 1, 9)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:39)
- **MATRIX.ROWCOUNT(x1)** — `PRINT "welchrows ="; MATRIX.ROWCOUNT(w)` (shared/samples/Math/SciPyP1Parity.bas:14)
- **MATRIX.ROWMEANS(x1)** — `DIM meansByRow() = MATRIX.ROWMEANS(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:13)
- **MATRIX.ROWSUMS(x1)** — `DIM sumsByRow() = MATRIX.ROWSUMS(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:11)
- **MATRIX.ROWVARIANCES(x1)** — `DIM variancesByRow() = MATRIX.ROWVARIANCES(m)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:15)
- **MATRIX.SCHUR(x1)** — `sd = MATRIX.SCHUR(h)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:70)
- **MATRIX.SETVALUE(x1, x2, x3, x4)** — `h = MATRIX.SETVALUE(h, 0, 0, 2) : h = MATRIX.SETVALUE(h, 0, 1, 1)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:9)
- **MATRIX.SIGNM(x1)** — `sg = MATRIX.SIGNM(h)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:18)
- **MATRIX.SINM(x1)** — `s = MATRIX.SINM(h)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:14)
- **MATRIX.SLICE(x1, x2, x3, x4, x5)** — `DIM sliced = MATRIX.SLICE(m, 0, 2, 0, 2)` (shared/samples/Matrix/MatrixCommonLibSimd.bas:38)
- **MATRIX.SOLVE(x1, x2)** — `DIM xs() = MATRIX.SOLVE(a, b)` (shared/samples/Matrix/MatrixDecompositions.bas:95)
- **MATRIX.SOLVETRIANGULAR(x1, x2, x3)** — `DIM vb() = MATRIX.SOLVETRIANGULAR(L, b, 1)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:59)
- **MATRIX.SQRTM(x1)** — `s = MATRIX.SQRTM(h)` (shared/samples/Math/MatrixFunctionsSciPyParity.bas:11)
- **MATRIX.SUBSPACEANGLES(x1, x2)** — `DIM ang() = MATRIX.SUBSPACEANGLES(h, g)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:30)
- **MATRIX.SVD(x1)** — `DIM svd = MATRIX.SVD(a)` (shared/samples/Matrix/MatrixDecompositions.bas:35)
- **MATRIX.TANM(x1)** — `tn = MATRIX.TANM(h)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:16)
- **MATRIX.TOEPLITZ(x1, x2)** — `tp = MATRIX.TOEPLITZ(c1, r1)` (shared/samples/Math/MatrixFunctionsSciPyParity2.bas:45)
- **MATRIX.TRACE(x1)** — `PRINT "trace="; MATH.ROUND(MATRIX.TRACE(a), 6)` (shared/samples/Matrix/MatrixDecompositions.bas:17)
- **MATRIX.TRANSPOSE(x1)** — `DIM tr = MATRIX.TRANSPOSE(a)` (shared/samples/Matrix/MatrixDecompositions.bas:26)
- **OPTIMIZE.BASINHOPPING(x1, x2)** — `DIM bh() = OPTIMIZE.BASINHOPPING(FN(p() AS DOUBLE) AS DOUBLE => (1 - p(0)) ^ 2 + 100 * (p(1) - p(0) …` (shared/samples/Math/OptimizeSciPyParity.bas:65)
- **OPTIMIZE.BFGS(x1, x2)** — `DIM MinBFGS() = OPTIMIZE.BFGS(Rosenbrock, InitialGuess)` (shared/samples/Optimization/OptimizationTests.bas:84)
- **OPTIMIZE.BISECTION(x1, x2, x3)** — `DIM RootBisect = OPTIMIZE.BISECTION(Func, 0, 5)` (shared/samples/Optimization/OptimizationTests.bas:44)
- **OPTIMIZE.BRENT(x1, x2, x3)** — `DIM RootBrent = OPTIMIZE.BRENT(Func, 0, 5) ' Cerca tra 0 e 5` (shared/samples/Optimization/OptimizationTests.bas:40)
- **OPTIMIZE.BRENTMIN(x1, x2, x3)** — `m1 = OPTIMIZE.BRENTMIN(FN(x AS DOUBLE) AS DOUBLE => (x - 3) * (x - 3), 0, 5)` (shared/samples/Math/OptimizeSciPyParity.bas:19)
- **OPTIMIZE.BRENTH(x1, x2, x3)** — `r2 = OPTIMIZE.BRENTH(FN(x AS DOUBLE) AS DOUBLE => x * x * x - 27, 2, 4)` (shared/samples/Math/OptimizeSciPyParity.bas:11)
- **OPTIMIZE.BRENTQ(x1, x2, x3)** — `r1 = OPTIMIZE.BRENTQ(FN(x AS DOUBLE) AS DOUBLE => x * x - 2, 0, 2)` (shared/samples/Math/OptimizeSciPyParity.bas:9)
- **OPTIMIZE.BROYDEN(x1, x2)** — `DIM RootBroyden() = OPTIMIZE.BROYDEN(RootSystem, RootGuess)` (shared/samples/Optimization/OptimizationTests.bas:97)
- **OPTIMIZE.BRUTE(x1, x2)** — `DIM br() = OPTIMIZE.BRUTE(FN(p() AS DOUBLE) AS DOUBLE => (p(0) - 1) * (p(0) - 1) + (p(1) + 2) * (p(1…` (shared/samples/Math/OptimizeSciPyParity.bas:63)
- **OPTIMIZE.CHOLESKYSOLVE(x1, x2)** — `DIM SolCh() = OPTIMIZE.CHOLESKYSOLVE(A, b)` (shared/samples/Optimization/OptimizeUncoveredCoverage.bas:30)
- **OPTIMIZE.COBYLA(x1, x2)** — `DIM cb() = OPTIMIZE.COBYLA(FN(p() AS DOUBLE) AS DOUBLE => (p(0) + 2) * (p(0) + 2) + (p(1) + 1) * (p(…` (shared/samples/Math/OptimizeSciPyParity.bas:35)
- **OPTIMIZE.CONJUGATEGRADIENT(x1, x2)** — `DIM ResultCG() = OPTIMIZE.CONJUGATEGRADIENT(RosenbrockCG, StartCG)` (shared/samples/Optimization/OptimizationTests.bas:144)
- **OPTIMIZE.CURVEFIT(x1, x2, x3, x4)** — `DIM Fit() = OPTIMIZE.CURVEFIT(FN(p() AS DOUBLE, xx AS DOUBLE) AS DOUBLE => p(0) * EXP(p(1) * xx) + p…` (shared/samples/Math/CurveFitSciPyParity.bas:9)
- **OPTIMIZE.DIFFERENTIALEVOLUTION(x1, x2)** — `DIM Best() = OPTIMIZE.DIFFERENTIALEVOLUTION(Rf, Bnd)` (shared/samples/Math/DifferentialEvolutionSciPyParity.bas:9)
- **OPTIMIZE.DUALANNEALING(x1, x2)** — `DIM da() = OPTIMIZE.DUALANNEALING(FN(p() AS DOUBLE) AS DOUBLE => (p(0) - 1) * (p(0) - 1) + (p(1) + 2…` (shared/samples/Math/OptimizeSciPyParity.bas:69)
- **OPTIMIZE.FSOLVE(x1, x2)** — `DIM fs() = OPTIMIZE.FSOLVE(FN(p() AS DOUBLE) AS ARRAY OF DOUBLE => {p(0) * p(0) + p(1) * p(1) - 1, p…` (shared/samples/Math/OptimizeSciPyParity.bas:42)
- **OPTIMIZE.GOLDENSECTION(x1, x2, x3)** — `DIM MinX = OPTIMIZE.GOLDENSECTION(MinFunc, 0, 6)` (shared/samples/Optimization/OptimizationTests.bas:59)
- **OPTIMIZE.LBFGS(x1, x2)** — `DIM MinLBFGS() = OPTIMIZE.LBFGS(Rosenbrock, InitialGuess)` (shared/samples/Optimization/OptimizationTests.bas:87)
- **OPTIMIZE.LEASTSQUARES(x1, x2)** — `DIM SolLS() = OPTIMIZE.LEASTSQUARES(MatA, b_ls)` (shared/samples/Optimization/OptimizationTests.bas:115)
- **OPTIMIZE.LEASTSQUARESFIT(x1, x2)** — `DIM ls() = OPTIMIZE.LEASTSQUARESFIT(FN(p() AS DOUBLE) AS ARRAY OF DOUBLE => {p(0) * 1 + p(1) - 2, p(…` (shared/samples/Math/OptimizeSciPyParity.bas:48)
- **OPTIMIZE.LEVENBERGMARQUARDT(x1, x2, x3, x4)** — `DIM ParamsLM() = OPTIMIZE.LEVENBERGMARQUARDT(ModelFunc, InitialParams, DataX, DataY)` (shared/samples/Optimization/OptimizationTests.bas:136)
- **OPTIMIZE.LEVENBERGMARQUARDTVEC(x1, x2, x3, x4)** — `DIM SolLmVec() = OPTIMIZE.LEVENBERGMARQUARDTVEC(VectorModel, PG, Inputs, Targets)` (shared/samples/Optimization/OptimizeUncoveredCoverage.bas:63)
- **OPTIMIZE.LINPROG(x1)** — `DIM lp() = OPTIMIZE.LINPROG({-1, -1}, Aub, bub)` (shared/samples/Math/OptimizeSciPyParity.bas:56)
- **OPTIMIZE.MINIMIZE(x1, x2)** — `DIM mn() = OPTIMIZE.MINIMIZE(FN(p() AS DOUBLE) AS DOUBLE => (1 - p(0)) ^ 2 + 100 * (p(1) - p(0) * p(…` (shared/samples/Math/OptimizeSciPyParity.bas:37)
- **OPTIMIZE.MINIMIZESCALAR(x1, x2, x3)** — `m2 = OPTIMIZE.MINIMIZESCALAR(FN(x AS DOUBLE) AS DOUBLE => (x + 1) * (x + 1) + 2, -5, 5, "brent")` (shared/samples/Math/OptimizeSciPyParity.bas:21)
- **OPTIMIZE.NELDERMEAD(x1, x2)** — `DIM MinNM() = OPTIMIZE.NELDERMEAD(Rosenbrock, InitialGuess)` (shared/samples/Optimization/OptimizationTests.bas:80)
- **OPTIMIZE.NEWTON(x1, x2)** — `DIM Radice = OPTIMIZE.NEWTON({ |x| RETURN x^2 - 4 }, 1.0)` (shared/samples/Clipper/ClipperCodeBlocks.bas:127)
- **OPTIMIZE.POWELL(x1, x2)** — `DIM pw() = OPTIMIZE.POWELL(FN(p() AS DOUBLE) AS DOUBLE => (1 - p(0)) ^ 2 + 100 * (p(1) - p(0) * p(0)…` (shared/samples/Math/OptimizeSciPyParity.bas:26)
- **OPTIMIZE.RIDDER(x1, x2, x3)** — `r3 = OPTIMIZE.RIDDER(FN(x AS DOUBLE) AS DOUBLE => x * x * x - 27, 2, 4)` (shared/samples/Math/OptimizeSciPyParity.bas:13)
- **OPTIMIZE.SECANT(x1, x2, x3)** — `DIM RootSecant = OPTIMIZE.SECANT(Func, 1, 3)` (shared/samples/Optimization/OptimizationTests.bas:48)
- **OPTIMIZE.SIMPLEX(x1, x2, x3)** — `DIM SolSimplex() = OPTIMIZE.SIMPLEX(Objective, Constraints, Bounds)` (shared/samples/Optimization/OptimizationTests.bas:123)
- **OPTIMIZE.SLSQP(x1, x2)** — `DIM sl() = OPTIMIZE.SLSQP(FN(p() AS DOUBLE) AS DOUBLE => (1 - p(0)) ^ 2 + 100 * (p(1) - p(0) * p(0))…` (shared/samples/Math/OptimizeSciPyParity.bas:33)
- **OPTIMIZE.TNC(x1, x2)** — `DIM tn() = OPTIMIZE.TNC(FN(p() AS DOUBLE) AS DOUBLE => (1 - p(0)) ^ 2 + 100 * (p(1) - p(0) * p(0)) ^…` (shared/samples/Math/OptimizeSciPyParity.bas:31)
- **OPTIMIZE.TOMS748(x1, x2, x3)** — `r4 = OPTIMIZE.TOMS748(FN(x AS DOUBLE) AS DOUBLE => SIN(x) - 0.5, 0, 1)` (shared/samples/Math/OptimizeSciPyParity.bas:15)
- **STATS.ANOVA(x1)** — `result = STATS.ANOVA(groups)` (shared/samples/Analysis/StatsSciPyParity.bas:23)
- **STATS.BERNOULLIPMF(x1, x2)** — `PRINT "BERNOULLIPMF(1,0.5): "; FORMAT(STATS.BERNOULLIPMF(1,0.5), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:162)
- **STATS.BETACDF(x1, x2, x3)** — `PRINT "BETACDF(0.5,2,2): "; FORMAT(STATS.BETACDF(0.5,2,2), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:148)
- **STATS.BETAPDF(x1, x2, x3)** — `PRINT "BETAPDF(0.5,2,2): "; FORMAT(STATS.BETAPDF(0.5,2,2), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:147)
- **STATS.BETAPPF(x1, x2, x3)** — `PRINT "betappf  ="; STATS.BETAPPF(0.5, 2, 3)` (shared/samples/Math/StatsInverseSciPyParity.bas:16)
- **STATS.BINOMIALPMF(x1, x2, x3)** — `PRINT "BINOMIALPMF(3,10,0.5): "; FORMAT(STATS.BINOMIALPMF(3,10,0.5), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:163)
- **STATS.CHI2(x1, x2)** — `DIM chi2 = STATS.CHI2(observed, expected)` (shared/samples/Analysis/StatsCategoricalTests.bas:10)
- **STATS.CHI2CON(x1)** — `r = STATS.CHI2CON(tb)` (shared/samples/Math/StatsSciPyParityP0.bas:33)
- **STATS.CHICDF(x1, x2)** — `PRINT "CHICDF(1,2):   "; FORMAT(STATS.CHICDF(1,2), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:144)
- **STATS.CHIPDF(x1, x2)** — `PRINT "CHIPDF(1,2):   "; FORMAT(STATS.CHIPDF(1,2), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:143)
- **STATS.CHIPPF(x1, x2)** — `PRINT "chippf   ="; STATS.CHIPPF(0.9, 7)` (shared/samples/Math/StatsInverseSciPyParity.bas:14)
- **STATS.CHISQUAREDCDF(x1, x2)** — `PRINT "CHISQUAREDCDF(1,2): "; FORMAT(STATS.CHISQUAREDCDF(1,2), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:146)
- **STATS.CHISQUAREDISF(x1, x2)** — `PRINT "chi2isf  ="; STATS.CHISQUAREDISF(0.05, 5)` (shared/samples/Math/StatsInverseSciPyParity.bas:21)
- **STATS.CHISQUAREDPDF(x1, x2)** — `PRINT "CHISQUAREDPDF(1,2): "; FORMAT(STATS.CHISQUAREDPDF(1,2), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:145)
- **STATS.CHISQUAREDPPF(x1, x2)** — `PRINT "chi2ppf  ="; STATS.CHISQUAREDPPF(0.95, 5)` (shared/samples/Math/StatsInverseSciPyParity.bas:13)
- **STATS.CORRELATION(x1, x2)** — `DIM Corr = ANALYSIS.CORRELATION(Dataset1, Dataset2)` (shared/samples/Analysis/AnalysisStatsDescriptive.bas:61)
- **STATS.DESCRIBE(x1)** — `r = STATS.DESCRIBE(v)` (shared/samples/Math/StatsSciPyParityP0.bas:61)
- **STATS.EXPONENTIALCDF(x1, x2)** — `PRINT "EXPONENTIALCDF(1,2): "; FORMAT(STATS.EXPONENTIALCDF(1,2), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:156)
- **STATS.EXPONENTIALISF(x1, x2)** — `PRINT "expisf   ="; STATS.EXPONENTIALISF(0.05, 0.5)` (shared/samples/Math/StatsInverseSciPyParity.bas:23)
- **STATS.EXPONENTIALPDF(x1, x2)** — `PRINT "EXPONENTIALPDF(1,2): "; FORMAT(STATS.EXPONENTIALPDF(1,2), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:155)
- **STATS.EXPONENTIALPPF(x1, x2)** — `PRINT "expppf   ="; STATS.EXPONENTIALPPF(0.9, 0.5)` (shared/samples/Math/StatsInverseSciPyParity.bas:17)
- **STATS.FCDF(x1, x2, x3)** — `PRINT "FCDF(1,5,10):  "; FORMAT(STATS.FCDF(1,5,10), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:154)
- **STATS.FISF(x1, x2, x3)** — `PRINT "fisf     ="; STATS.FISF(0.05, 3, 8)` (shared/samples/Math/StatsInverseSciPyParity.bas:22)
- **STATS.FPDF(x1, x2, x3)** — `PRINT "FPDF(1,5,10):  "; FORMAT(STATS.FPDF(1,5,10), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:153)
- **STATS.FPPF(x1, x2, x3)** — `PRINT "fppf     ="; STATS.FPPF(0.95, 3, 8)` (shared/samples/Math/StatsInverseSciPyParity.bas:15)
- **STATS.FISHER(x1, x2, x3, x4)** — `DIM fisher = STATS.FISHER(8, 2, 1, 5)` (shared/samples/Analysis/StatsCategoricalTests.bas:14)
- **STATS.GAMMACDF(x1, x2, x3)** — `PRINT "gammacdf="; STATS.GAMMACDF(2, 3, 4)` (shared/samples/Math/StatsCdfPdfSciPyParity.bas:15)
- **STATS.GAMMAISF(x1, x2, x3)** — `PRINT "gammaisf ="; MATH.ROUND(STATS.GAMMAISF(2, 1, 0.05), 9)` (shared/samples/Math/StatsInverseSciPyParity.bas:24)
- **STATS.GAMMAPDF(x1, x2, x3)** — `PRINT "gammapdf="; STATS.GAMMAPDF(2, 3, 4)` (shared/samples/Math/StatsCdfPdfSciPyParity.bas:16)
- **STATS.GAMMAPPF(x1, x2, x3)** — `PRINT "gppf="; MATH.ROUND(STATS.GammaPpf(2, 1, 0.5), 6)` (shared/minitests/stats/dist_extra.bas:7)
- **STATS.GEOMETRICPMF(x1, x2)** — `PRINT "GEOMETRICPMF(3,0.5): "; FORMAT(STATS.GEOMETRICPMF(3,0.5), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:165)
- **STATS.HYPERGEOMETRICPMF(x1, x2, x3, x4)** — `PRINT "HYPERGEOMETRICPMF(2,5,10,20): "; FORMAT(STATS.HYPERGEOMETRICPMF(2,20,10,5), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:167)
- **STATS.IQR(x1)** — `PRINT "iqr      ="; MATH.ROUND(STATS.IQR(x), 6)` (shared/samples/Math/StatsSciPyParityP0.bas:21)
- **STATS.JARQUEBERA(x1)** — `r = STATS.JARQUEBERA(z)` (shared/samples/Math/StatsSciPyParityP0.bas:59)
- **STATS.KS(x1, x2)** — `result = STATS.KS(x, y)` (shared/samples/Analysis/StatsSciPyParity.bas:35)
- **STATS.KURTOSIS(x1)** — `DIM kurtosis() = TS.TSROLLKURTOSIS(values, 4)` (shared/samples/TimeSeries/TimeSeriesCommonLibNumerics.bas:17)
- **STATS.KURTOSISTEST(x1)** — `r = STATS.KURTOSISTEST(z)` (shared/samples/Math/StatsSciPyParityP0.bas:57)
- **STATS.LINREGRESS(x1, x2)** — `r = STATS.LINREGRESS(x, y)` (shared/samples/Math/StatsSciPyParityP0.bas:49)
- **STATS.LOGISTICPDF(x1, x2, x3)** — `PRINT "LOGISTICPDF(0,0,1): "; FORMAT(STATS.LOGISTICPDF(0,0,1), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:158)
- **STATS.LOGISTICPPF(x1, x2, x3)** — `PRINT "logippf  ="; STATS.LOGISTICPPF(0.5, 0, 1)` (shared/samples/Math/StatsInverseSciPyParity.bas:25)
- **STATS.LOGNORMALCDF(x1)** — `PRINT "LOGNORMALCDF(1): "; FORMAT(STATS.LOGNORMALCDF(1), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:142)
- **STATS.LOGNORMALPDF(x1)** — `PRINT "LOGNORMALPDF(1): "; FORMAT(STATS.LOGNORMALPDF(1), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:141)
- **STATS.LOGNORMALPPF(x1)** — `PRINT "logppf   ="; STATS.LOGNORMALPPF(0.9, 1, 0.5)` (shared/samples/Math/StatsInverseSciPyParity.bas:19)
- **STATS.MAD(x1)** — `PRINT "MAD (Median Abs Dev): "; STATS.MAD(Dataset1)` (shared/samples/Analysis/AnalysisStatsDescriptive.bas:52)
- **STATS.MANNWHITNEY(x1, x2)** — `r = STATS.MANNWHITNEY(x, y)` (shared/samples/Math/StatsSciPyParityP0.bas:51)
- **STATS.MEAN(x1)** — `GmbStats = "mean=" + STR$(STATS.MEAN(n)) + " max=" + STR$(MATH.MAX(n[0], n[1], n[2])) + " min=" + ST…` (shared/samples/Ai/McpCapabilityServer.bas:51)
- **STATS.MEDIAN(x1)** — `SQL QUERY T, rs, "SELECT ROUND(VAR_POP(p),4) AS var_pop, ROUND(VAR_SAMP(p),4) AS var_samp, ROUND(STD…` (shared/samples/Databases/SqlStatsAggregates.bas:21)
- **STATS.MODE(x1)** — `PRINT "mode="; STATS.Mode(a)` (shared/minitests/stats/dist_extra.bas:11)
- **STATS.MOMENT(x1, x2)** — `PRINT "STATS.MOMENT — momenti del campione 1..10"` (shared/samples/Analysis/StatsMoment.bas:25)
- **STATS.NEGATIVEBINOMIALPMF(x1, x2, x3)** — `PRINT "NEGATIVEBINOMIALPMF(3,5,0.5): "; FORMAT(STATS.NEGATIVEBINOMIALPMF(3,5,0.5), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:166)
- **STATS.NORMALCDF(x1)** — `PRINT "NORMALCDF(0):  "; FORMAT(STATS.NORMALCDF(0), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:140)
- **STATS.NORMALISF(x1)** — `PRINT "normisf  ="; STATS.NORMALISF(0.025, 0, 1)` (shared/samples/Math/StatsInverseSciPyParity.bas:20)
- **STATS.NORMALPDF(x1)** — `PRINT "STATS.NORMALPDF(0):  "; FORMAT(STATS.NORMALPDF(0), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:139)
- **STATS.NORMALPPF(x1)** — `PRINT "normppf  ="; STATS.NORMALPPF(0.975, 1, 2)` (shared/samples/Math/StatsInverseSciPyParity.bas:10)
- **STATS.NORMALTEST(x1)** — `result = STATS.NORMALTEST(normal)` (shared/samples/Analysis/StatsSciPyParity.bas:31)
- **STATS.PARETOPDF(x1, x2, x3)** — `PRINT "PARETOPDF(1,2,3): "; FORMAT(STATS.PARETOPDF(1,2,3), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:157)
- **STATS.PARETOPPF(x1, x2, x3)** — `PRINT "paretoppf="; STATS.PARETOPPF(0.9, 3, 2)` (shared/samples/Math/StatsInverseSciPyParity.bas:26)
- **STATS.PEARSONR(x1, x2)** — `r = STATS.PEARSONR(x, y)` (shared/samples/Math/StatsSciPyParityP0.bas:45)
- **STATS.PERCENTILE(x1, x2)** — `DIM P90 = STATS.PERCENTILE(Dataset1, 0.90)` (shared/samples/Analysis/AnalysisStatsDescriptive.bas:69)
- **STATS.POISSONPMF(x1, x2)** — `PRINT "POISSONPMF(3,2): "; FORMAT(STATS.POISSONPMF(3,2), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:164)
- **STATS.SHAPIRO(x1)** — `result = STATS.SHAPIRO(normal)` (shared/samples/Analysis/StatsSciPyParity.bas:29)
- **STATS.SKEWTEST(x1)** — `r = STATS.SKEWTEST(z)` (shared/samples/Math/StatsSciPyParityP0.bas:55)
- **STATS.SKEWEDNORMALPDF(x1)** — `PRINT "SKEWEDNORMALPDF(0): "; FORMAT(STATS.SKEWEDNORMALPDF(0), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:161)
- **STATS.SKEWNESS(x1)** — `DIM skewness() = TS.TSROLLSKEWNESS(values, 3)` (shared/samples/TimeSeries/TimeSeriesCommonLibNumerics.bas:16)
- **STATS.SPEARMANR(x1, x2)** — `r = STATS.SPEARMANR(x, y)` (shared/samples/Math/StatsSciPyParityP0.bas:47)
- **STATS.STDDEV(x1)** — `SQL QUERY A, rs, "SELECT TIMEBUCKET(ts,300) AS bucket, ROUND(VAR_POP(price),4) AS varpop, ROUND(VAR_…` (shared/samples/Databases/SqlAnalyticsFull.bas:29)
- **STATS.STUDENTTCDF(x1, x2)** — `PRINT "STUDENTTCDF(0,5): "; FORMAT(STATS.STUDENTTCDF(0,5), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:150)
- **STATS.STUDENTTISF(x1, x2)** — `PRINT "tisf     ="; STATS.STUDENTTISF(0.025, 10)` (shared/samples/Math/StatsInverseSciPyParity.bas:12)
- **STATS.STUDENTTPDF(x1, x2)** — `PRINT "STUDENTTPDF(0,5): "; FORMAT(STATS.STUDENTTPDF(0,5), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:149)
- **STATS.STUDENTTPPF(x1, x2)** — `PRINT "tppf     ="; STATS.STUDENTTPPF(0.975, 10)` (shared/samples/Math/StatsInverseSciPyParity.bas:11)
- **STATS.TTEST(x1, x2)** — `DIM result = STATS.TTEST(x, y)` (shared/samples/Analysis/StatsSciPyParity.bas:17)
- **STATS.TTESTONESAMPLE(x1, x2)** — `result = STATS.TTESTONESAMPLE(x, 5.0)` (shared/samples/Analysis/StatsSciPyParity.bas:21)
- **STATS.TTESTPAIRED(x1, x2)** — `result = STATS.TTESTPAIRED(x, paired)` (shared/samples/Analysis/StatsSciPyParity.bas:19)
- **STATS.TRIANGULARPDF(x1)** — `PRINT "TRIANGULARPDF(0.5): "; FORMAT(STATS.TRIANGULARPDF(0.5), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:159)
- **STATS.TRIANGULARPPF(x1)** — `PRINT "trianppf ="; STATS.TRIANGULARPPF(0.5, 0, 1, 0.5)` (shared/samples/Math/StatsInverseSciPyParity.bas:27)
- **STATS.UNIFORMPDF(x1)** — `PRINT "UNIFORMPDF(0.5):  "; FORMAT(STATS.UNIFORMPDF(0.5), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:160)
- **STATS.UNIFORMPPF(x1)** — `PRINT "unifppf  ="; STATS.UNIFORMPPF(0.5, 0, 1)` (shared/samples/Math/StatsInverseSciPyParity.bas:28)
- **STATS.VARIANCE(x1)** — `DIM varj = LEDGER.VARIANCE(h, "Attivita:Cassa", "2026-01")` (shared/samples/Databases/DbLedgerCompleteCoverage.bas:56)
- **STATS.WEIBULLCDF(x1, x2, x3)** — `PRINT "WEIBULLCDF(1,2,3): "; FORMAT(STATS.WEIBULLCDF(1,2,3), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:152)
- **STATS.WEIBULLPDF(x1, x2, x3)** — `PRINT "WEIBULLPDF(1,2,3): "; FORMAT(STATS.WEIBULLPDF(1,2,3), "0.0000")` (shared/samples/Analysis/AnalysisTests.bas:151)
- **STATS.WEIBULLPPF(x1, x2, x3)** — `PRINT "weibppf  ="; STATS.WEIBULLPPF(0.8, 1.5, 2)` (shared/samples/Math/StatsInverseSciPyParity.bas:18)
- **STATS.WILCOXON(x1, x2)** — `r = STATS.WILCOXON(x, y)` (shared/samples/Math/StatsSciPyParityP0.bas:53)
- **STATS.ZTEST(x1, x2, x3)** — `result = STATS.ZTEST(x, 5.0, 2.0)` (shared/samples/Analysis/StatsSciPyParity.bas:33)
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


## 1. Riferimento funzioni
| NOME | SINTASSI / ESEMPIO | DESCRIZIONE |
|---|---|---|
| `ADFTEST` | `ANALYSIS.AdfTest()` | Test Dickey-Fuller aumentato. |
| `ANOVA` | `ANALYSIS.Anova(groups)` | Analisi varianza. |
| `ARIMA` | `ANALYSIS.Arima(1, 1, 1)` | Modello ARIMA. |
| `CORRELATION` | `ANALYSIS.Correlation()` | Correlazione. |
| `CUBICSPLINE` | `ANALYSIS.CubicSpline(x, y, x_new)` | Spline cubica not-a-knot. |
| `CUMTRAPEZOID` | `ANALYSIS.CumTrapezoid(y, x)` | Integrale trapezoidale cumulativo. |
| `DIFFERENTIATE` | `ANALYSIS.Differentiate(1)` | Derivazione numerica. |
| `EUCLIDEANDISTANCESQUARED` | `ANALYSIS.EuclideanDistanceSquared()` | Distanza euclidea quadrata. |
| `FFT` | `ANALYSIS.Fft()` | Trasformata Fourier. |
| `FFTCOMPLEX` | `ANALYSIS.FftComplex()` | FFT complessa. |
| `FFTFREQ` | `ANALYSIS.FftFreq(n, d)` | Frequenze discrete FFT. |
| `FFTSHIFT` | `ANALYSIS.FftShift(x)` | Frequenza zero al centro. |
| `HOLTWINTERS` | `ANALYSIS.HoltWinters(1, 1, 1, 1)` | Previsione Holt-Winters. |
| `IFFT` | `ANALYSIS.Ifft()` | FFT inversa. |
| `IFFTCOMPLEX` | `ANALYSIS.IfftComplex()` | IFFT complessa. |
| `IFFTSHIFT` | `ANALYSIS.IfftShift(x)` | Inverso di FftShift. |
| `INTEGRATE` | `ANALYSIS.Integrate(1, 1)` | Integrazione numerica. |
| `INTERPOLATECLAMP` | `ANALYSIS.InterpolateClamp(1)` | Interp. clamp. |
| `INTERPOLATEGENERIC` | `ANALYSIS.InterpolateGeneric("s", 1)` | Interp. generica. |
| `INTERPOLATELINEAR` | `ANALYSIS.InterpolateLinear(1)` | Interpolazione lineare. |
| `INTERPOLATEPCHIP` | `ANALYSIS.InterpolatePchip(1)` | Interp. PCHIP. |
| `INTERPOLATEPOLYNOMIAL` | `ANALYSIS.InterpolatePolynomial(1)` | Interp. polinomiale. |
| `INTERPOLATESPLINE` | `ANALYSIS.InterpolateSpline(1)` | Interpolazione spline. |
| `INTERPOLATESTEP` | `ANALYSIS.InterpolateStep(1)` | Interp. a gradino. |
| `IRFFT` | `ANALYSIS.Irfft(re, im)` | FFT reale inversa. |
| `RFFT` | `ANALYSIS.Rfft(x)` | FFT reale (componenti non negative). |
| `SPLEV` | `ANALYSIS.Splev(spline, xq)` | Valuta spline cubica. |
| `SPLREP` | `ANALYSIS.Splrep(x, y)` | Spline cubica interpolante (not-a-knot). |
| `KMEANS` | `ANALYSIS.KMeans(1)` | Clustering K-means. |
| `KURTOSIS` | `ANALYSIS.Kurtosis()` | Curtosi. |
| `MACOEFFICIENTS` | `ANALYSIS.MaCoefficients(1)` | Coefficienti MA. |
| `MAD` | `ANALYSIS.Mad()` | Deviazione media assoluta. |
| `MEAN` | `ANALYSIS.Mean()` | Media aritmetica. |
| `MEDIAN` | `ANALYSIS.Median()` | Mediana. |
| `PCA` | `ANALYSIS.Pca()` | Analisi componenti principali. |
| `REGRESS` | `ANALYSIS.Regress()` | Regressione lineare. |
| `SIMPSON` | `ANALYSIS.Simpson(fn, a, b, n)` | Simpson composita. |
| `SOLVEIVP` | `ANALYSIS.SolveIvp(fn, y0, t0, t1)` | ODE adattiva RK45 (scipy solve_ivp). |
| `SOLVEODE` | `ANALYSIS.SolveOde(fn, y0, t0, t1, s)` | Solutore ODE 1o ordine (RK4). |
| `SKEWNESS` | `ANALYSIS.Skewness()` | Asimmetria. |
| `STDDEV` | `ANALYSIS.StdDev()` | Deviazione standard. |
| `TRAPEZOID` | `ANALYSIS.Trapezoid(y, x)` | Integrale trapezoidale. |
| `VARIANCE` | `ANALYSIS.Variance()` | Varianza. |
| `YULEWALKER` | `ANALYSIS.YuleWalker(1)` | Stima Yule-Walker. |

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

  PRINT "integrate   ="; ANALYSIS.INTEGRATE(FN(x) => x * x, 0, 1)
  PRINT "simpson     ="; ANALYSIS.SIMPSON(FN(x) => x * x, 0, 1, 256)
  PRINT "trapezoid   ="; ANALYSIS.TRAPEZOID(yy, xx)
  DIM ct() = ANALYSIS.CUMTRAPEZOID(yy, xx)
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


DIM r AS Analysis.RegressionResult
r = ANALYSIS.REGRESS(x, y)
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
| `CHOLESKY` | `MATRIX.CHOLESKY(1)` | CHOLESKY di MATRIX. |
| `CLONEMATRIX` | `MATRIX.CLONEMATRIX()` | CLONEMATRIX di MATRIX. |
| `COLMEANS` | `MATRIX.COLMEANS()` | COLMEANS di MATRIX. |
| `COLSUMS` | `MATRIX.COLSUMS()` | COLSUMS di MATRIX. |
| `COLUMNCOUNT` | `MATRIX.COLUMNCOUNT()` | COLUMNCOUNT di MATRIX. |
| `COLVARIANCES` | `MATRIX.COLVARIANCES()` | COLVARIANCES di MATRIX. |
| `CONCATCOLS` | `MATRIX.CONCATCOLS(1)` | CONCATCOLS di MATRIX. |
| `CONCATROWS` | `MATRIX.CONCATROWS(1)` | CONCATROWS di MATRIX. |
| `CREATE` | `MATRIX.CREATE(1, 1)` | CREATE di MATRIX. |
| `DETERMINANT` | `MATRIX.DETERMINANT(1)` | DETERMINANT di MATRIX. |
| `EVD` | `MATRIX.EVD(1)` | EVD di MATRIX. |
| `FACTOR` | `MATRIX.FACTOR(decompositionobject, "s")` | FACTOR di MATRIX. |
| `FROBENIUSNORM` | `MATRIX.FROBENIUSNORM()` | FROBENIUSNORM di MATRIX. |
| `INFINITYNORM` | `MATRIX.INFINITYNORM()` | INFINITYNORM di MATRIX. |
| `INVERSE` | `MATRIX.INVERSE(1)` | INVERSE di MATRIX. |
| `ISLOWERTRIANGULAR` | `MATRIX.ISLOWERTRIANGULAR()` | ISLOWERTRIANGULAR di MATRIX. |
| `ISORTHOGONAL` | `MATRIX.ISORTHOGONAL()` | ISORTHOGONAL di MATRIX. |
| `ISSYMMETRIC` | `MATRIX.ISSYMMETRIC()` | ISSYMMETRIC di MATRIX. |
| `ISUPPERTRIANGULAR` | `MATRIX.ISUPPERTRIANGULAR()` | ISUPPERTRIANGULAR di MATRIX. |
| `LOADCSV` | `MATRIX.LOADCSV("file.txt")` | LOADCSV di MATRIX. |
| `LOADJSON` | `MATRIX.LOADJSON("file.txt")` | LOADJSON di MATRIX. |
| `LU` | `MATRIX.LU(1)` | LU di MATRIX. |
| `MATRIX` | `MATRIX MATRIX` | MATRIX di MATRIX. |
| `MAXVALUE` | `MATRIX.MAXVALUE()` | MAXVALUE di MATRIX. |
| `MINVALUE` | `MATRIX.MINVALUE()` | MINVALUE di MATRIX. |
| `MULTIPLY` | `MATRIX.MULTIPLY(1, 1)` | MULTIPLY di MATRIX. |
| `MULTIPLYLEFTVECTOR` | `MATRIX.MULTIPLYLEFTVECTOR(42)` | MULTIPLYLEFTVECTOR di MATRIX. |
| `MULTIPLYVECTOR` | `MATRIX.MULTIPLYVECTOR(42)` | MULTIPLYVECTOR di MATRIX. |
| `NORM` | `MATRIX.NORM(1)` | NORM di MATRIX. |
| `ONENORM` | `MATRIX.ONENORM()` | ONENORM di MATRIX. |
| `PINVERSE` | `MATRIX.PINVERSE(1)` | PINVERSE di MATRIX. |
| `QR` | `MATRIX.QR(1)` | QR di MATRIX. |
| `RANK` | `MATRIX.RANK(1)` | RANK di MATRIX. |
| `RESHAPE` | `MATRIX.RESHAPE(1, 1)` | RESHAPE di MATRIX. |
| `ROWCOUNT` | `MATRIX.ROWCOUNT()` | ROWCOUNT di MATRIX. |
| `ROWMEANS` | `MATRIX.ROWMEANS()` | ROWMEANS di MATRIX. |
| `ROWSUMS` | `MATRIX.ROWSUMS()` | ROWSUMS di MATRIX. |
| `ROWVARIANCES` | `MATRIX.ROWVARIANCES()` | ROWVARIANCES di MATRIX. |
| `SLICE` | `MATRIX.SLICE(1, 1, 1, 1)` | SLICE di MATRIX. |
| `SOLVE` | `MATRIX.SOLVE(1, 1)` | SOLVE di MATRIX. |
| `SVD` | `MATRIX.SVD(1)` | SVD di MATRIX. |
| `TRACE` | `MATRIX.TRACE(1)` | TRACE di MATRIX. |
| `TRANSPOSE` | `MATRIX.TRANSPOSE(1)` | TRANSPOSE di MATRIX. |
| `EXPM` | `MATRIX.EXPM(h)` | Esponenziale di matrice (handle). |
| `KRONECKER` | `MATRIX.KRONECKER(h1, h2)` | Prodotto di Kronecker A⊗B. |
| `SQRTM` | `MATRIX.SQRTM(h)` | Radice quadrata di matrice, scipy.linalg.sqrtm. |
| `LOGM` | `MATRIX.LOGM(h)` | Logaritmo naturale di matrice, scipy.linalg.logm. |
| `HESSENBERG` | `MATRIX.HESSENBERG(h)` | Forma di Hessenberg superiore (Householder). |
| `BLOCKDIAG` | `MATRIX.BLOCKDIAG(h1, h2)` | Matrice diagonale a blocchi, scipy.linalg.block_diag. |
| `NULLSPACE` | `MATRIX.NULLSPACE(h)` | Base del kernel, scipy.linalg.null_space. |
| `SETVALUE` | `MATRIX.SETVALUE(h, r, c, v)` | Scrive v alla riga r, colonna c (0-based). |
| `GETVALUE` | `MATRIX.GETVALUE(h, r, c)` | Legge l'elemento riga r, colonna c (0-based). |
| `COSM` | `MATRIX.COSM(h)` | Coseno di matrice, scipy.linalg.cosm. |
| `SINM` | `MATRIX.SINM(h)` | Seno di matrice, scipy.linalg.sinm. |
| `TANM` | `MATRIX.TANM(h)` | Tangente di matrice, scipy.linalg.tanm. |
| `SIGNM` | `MATRIX.SIGNM(h)` | Segno di matrice, scipy.linalg.signm. |
| `FRACTIONALPOWER` | `MATRIX.FRACTIONALPOWER(h, p)` | Potenza frazionaria, scipy.linalg.fractional_matrix_power. |
| `ORTH` | `MATRIX.ORTH(h)` | Base ortonormale del range, scipy.linalg.orth. |
| `SUBSPACEANGLES` | `MATRIX.SUBSPACEANGLES(h1, h2)` | Angoli tra sottospazi, scipy.linalg.subspace_angles. |
| `POLAR` | `MATRIX.POLAR(h)` | Decomp. polare A=U·P (fattori U/P), scipy.linalg.polar. |
| `COND` | `MATRIX.COND(h)` | Numero di condizionamento (norma 2). |
| `NUCLEARNORM` | `MATRIX.NUCLEARNORM(h)` | Norma nucleare (somma valori singolari). |
| `TOEPLITZ` | `MATRIX.TOEPLITZ(c[], r[])` | Matrice di Toeplitz, scipy.linalg.toeplitz. |
| `HANKEL` | `MATRIX.HANKEL(c[], r[])` | Matrice di Hankel, scipy.linalg.hankel. |
| `CIRCULANT` | `MATRIX.CIRCULANT(c[])` | Matrice circolante, scipy.linalg.circulant. |
| `SOLVETRIANGULAR` | `MATRIX.SOLVETRIANGULAR(hA, b[], lower)` | Risolve sistema triangolare. |
| `EIG` | `MATRIX.EIG(h)` | Autovalori/autovettori generali (fattori DREAL/DIMAG/VRREAL/VRIMAG). |
| `SCHUR` | `MATRIX.SCHUR(h)` | Real Schur form (fattori T/Z). |

## 2. Esempio completo

' scipy.linalg: sqrtm, logm, hessenberg, block_diag (oracolo scipy 1.18)
IMPORT MATRIX

SUB Main()
  ' A = [[2,1],[1,2]]
  DIM h = MATRIX.CREATE(2, 2)
  h = MATRIX.SETVALUE(h, 0, 0, 2) : h = MATRIX.SETVALUE(h, 0, 1, 1)
  h = MATRIX.SETVALUE(h, 1, 0, 1) : h = MATRIX.SETVALUE(h, 1, 1, 2)

  DIM s = MATRIX.SQRTM(h)                    ' sqrtm diag 1.3660
  PRINT "sqrtm00  ="; MATRIX.GETVALUE(s, 0, 0)
  DIM l = MATRIX.LOGM(h)                     ' logm diag 0.5493
  PRINT "logm11   ="; MATRIX.GETVALUE(l, 1, 1)

  ' Hessenberg di una 3x3: Hh[2,0] deve essere ~0
  DIM h3 = MATRIX.CREATE(3, 3)


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
