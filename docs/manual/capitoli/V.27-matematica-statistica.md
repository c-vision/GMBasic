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
| quaternioni e rotazioni 3D | `QUAT`, `QUAT-Guida-Utente` | composizione, interpolazione (SLERP), matrice 3×3 |
| numeri complessi | `COMPLEX`, `COMPLEX-Guida-Utente` | aritmetica, modulo/fase, trascendenti e inverse |
| trasformazioni 3D | `XFORM`, `XFORM-Guida-Utente` | rotazioni/traslazioni, punto/direzione, inversa affine |
| trasformate (DCT, STFT, Haar, Hilbert, Z) | `SPECTRAL`, `SPECTRAL-Guida-Utente` | tempo-frequenza, compressione, inviluppo |
| vettori per la grafica | `VEC`, `VEC-Guida-Utente` | proiezione, Fresnel, bias, clamp, gamma |
| matrici sparse e solutori diretti | `SPARSE`, `SPARSE-Guida-Utente` | COO/CSR/CSC, Cholesky e LU sparsi |
| quadratura numerica | `QUAD`, `QUAD-Guida-Utente` | Gauss-Kronrod con errore, tanh-sinh, nodi |
| equazioni differenziali | `ODE`, `ODE-Guida-Utente` | multi-passo, BDF2 stiff, Verlet, BVP |
| calcolo vettoriale | `VECCALC`, `VECCALC-Guida-Utente` | gradiente, divergenza, rotore, Frenet, flussi |
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

#### Solver conici completi (`IPM*`, `SOCP*`, `SDP*`, `PRESOLVE*`, `STDFORM*`)

Gli stessi generatori sono il motore di quattro solver che `gmbconic` espone per intero.
La forma dei dati è sempre la stessa: **array piatti** con layout dichiarato, nessuna struct
da costruire e nessuna tabella di puntatori da passare in BASIC.

**IPM** risolve `min c'x` con `A x = b`, `x >= 0` (`IPMLP`) oppure `min 1/2 x'Qx + c'x`
(`IPMQP`). Le uscite sono `x` (`n`), `y` (`m`) e `z` (`n`); le varianti `*WARM` accettano un
punto iniziale, le varianti `*CSC` la matrice in forma sparsa. Per un QP sparso `Q` si passa
nel triangolo inferiore in CSC, con la diagonale presente.

**SOCP** risolve `min c'x` con `E x = d` e `G x + h` in un prodotto di coni. I coni sono tre
array paralleli: `ctype(k)` (`0` = `R+`, `1` = cono quadratico, `2` = quadratico ruotato),
`cnm(k)` (dimensione) e `cmem()` (indici dei membri dei soli coni SOC, concatenati in ordine
di cono). `G` ha `K = somma(cnm)` righe e `n` colonne. `SOCPSOLVESP` risolve lo stesso
problema con la LU sparsa: risultati identici, costo `O(fill)` invece di `O(N^3)`.

**SDP** (`SDPSOLVE`) è il problema conico unificato: variabili scalari non negative, blocchi
PSD `X_j`, blocchi SOC `z_i` e blocchi esponenziali/potenza. `dims(j)` dà l'ordine del blocco
PSD, `Cbar`/`Abar` sono le matrici consecutive (per `Abar`: per riga di uguaglianza, poi per
blocco), `socdims`, `Csoc`/`Asoc`, `ekind`/`ealpha`, `Cexp`/`Aexp` seguono lo stesso schema.
Le uscite hanno il medesimo layout, più `x`, `y` e `fb(0)` che segnala un punto di fallback
near-optimal.

**Presolve** (`PRESOLVENEW` … `PRESOLVEFREE`) riduce un LP in forma standard con riduzioni
strutturali sicure e un postsolve esatto. L'handle contiene lo stato; `PRESOLVEDIMS` dice
quante righe/colonne/`nnz` ha il ridotto, `PRESOLVEREDUCED` lo copia in CSC,
`PRESOLVEPOSTSOLVE` riporta la soluzione nello spazio originale e `PRESOLVEPOSTSOLVEDIR` fa
lo stesso per una direzione di Farkas. Se il ridotto è vuoto (`mr = nr = 0`) il postsolve
ricostruisce comunque tutta la soluzione.

**Forma standard** (`STDFORMBUILD` … `STDFORMFREE`) trasforma un LP/QP con bounds e vincoli
`<=`/`>=`/`=` nella forma `min 1/2 xt'Qs xt + cs'xt`, `As xt = bs`, `xt >= 0`, gestendo shift,
colonne negate, variabili libere spezzate e fisse. `STDFORMGETA/GETB/GETC/GETQ`
materializzano il problema standard per `IPMLP`/`IPMQP`; `STDFORMMAPX/MAPY/MAPDIR` riportano
primali, duali e direzioni nello spazio originale. I lati liberi si indicano con
`-1E308`/`+1E308`.

```basic
' dispacciamento: min 2g1+3g2+4g3  s.t. g1+g2+g3=100, g1+g2=60, g>=0
DIM A(5) AS DOUBLE
A(0)=1 : A(1)=1 : A(2)=1 : A(3)=1 : A(4)=1 : A(5)=0
DIM b(1) AS DOUBLE : b(0)=100 : b(1)=60
DIM c(2) AS DOUBLE : c(0)=2 : c(1)=3 : c(2)=4
DIM g(2) AS DOUBLE, y(1) AS DOUBLE, z(2) AS DOUBLE
PRINT OPTIMIZE.IPMLP(A, 2, 3, b, c, 1E-8, 1E-8, 1E-8, 200, g, y, z)
' 0 — g = (60, 0, 40), y = (4, -2)
```

Tre dettagli pratici. Primo: `tol` è la tolleranza **assoluta** di Newton e il pavimento del
double rende inutile scendere sotto `1e-8`; con `1e-10` il ciclo può esaurire le iterazioni
(`1`) pur avendo già il punto corretto. Secondo: i duali di un LP degenere **non sono unici**,
quindi `MAPY` restituisce un duale valido ma non necessariamente quello "atteso" — si verifica
la stazionarietà, non un numero fissato. Terzo: i ritorni sono `0` ottimo, `1` max iter,
`2` memoria, `3` singolare, `-1` argomenti; un `-1` non è "non risolto", è un errore di
chiamata.

La verifica segue il protocollo dei quattro backend: `16-ConicSolvers.bas` usa tutte le 26
funzioni di solver e trasformazione su uno scenario di dispacciamento e allocazione, con
`parity4` verde; `conic_ipm_presolve.bas`, `conic_socp_sdp.bas` e `conic_stdform.bas` coprono
i casi limite (ridotto vuoto, duale non unico, blocchi PSD/SOC/EXP). La guida di riferimento
è `Optimize-Conic-Guida-Utente`.

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

Un terzo pacchetto, **`scheduling`**, copre i **grafi temporali**: dato un grafo orientato in
cui ogni attività ha una finestra `[inizio, fine]` e serve un tempo minimo di connessione,
`SCHEDPATHSTIMEWINDOWS` elenca i percorsi ammissibili da un'attività all'altra (turni,
manutenzioni, sequenze di lavorazione). È l'unico caso in cui l'ammissibilità dipende dal
tempo e non dal costo.

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

## 8. Quaternioni e rotazioni 3D (`QUAT`)

Una rotazione tridimensionale si può descrivere con tre angoli (Eulero), con una matrice
3×3, oppure con quattro numeri: un **quaternione unitario**. La terza forma è quella che
usa un motore grafico o un simulatore, perché comporre due rotazioni costa 16 prodotti,
interpolare costa un `acos` e non esiste il blocco cardinale (gimbal lock) degli angoli.

`QUAT` (in `libgmbmath`, `IMPORT QUAT`) è il `TYPE Quat { X, Y, Z, W AS DOUBLE }` passato
**per valore**, senza handle né stato: diciannove funzioni pure.

```basic
IMPORT QUAT

DIM q AS Quat, v(2) AS DOUBLE
q = QUAT.FROMAXISANGLE(0, 0, 1, MATH.DEG2RAD(90))   ' 90° attorno a +Z
QUAT.ROTATEVEC3(q, 1, 0, 0, v)
PRINT v(0), v(1), v(2)                              ' 0  1  0
```

Le quattro convenzioni che generano errori se non si tengono presenti:

1. **i campi sono `X, Y, Z, W`** — parte vettoriale prima, parte scalare dopo; l'identità
   è `(0,0,0,1)`;
2. **`MUL(a, b)` applica prima `b`, poi `a`** (prodotto di Hamilton): l'ordine è
   l'opposto di come si legge;
3. **`FROMDIR` porta +Z** sulla direzione (convenzione camera); per una coppia arbitraria
   di vettori si usa `FROMTWOVECTORS`;
4. **`TOMAT3` scrive row-major** (`m(0..2)` = prima riga), mentre la struct C di origine è
   column-major: la conversione è fatta nello shim, chi legge la matrice vede righe.

Il gruppo completo:

| gruppo | funzioni | note |
|---|---|---|
| costruzione | `IDENTITY`, `MAKE`, `FROMDIR`, `FROMAXISANGLE`, `FROMTWOVECTORS`, `FROMMAT3` | `MAKE` non normalizza |
| algebra | `ADD`, `SCALE`, `NEG`, `MUL`, `CONJUGATE`, `DOT`, `LENGTH`, `NORMALIZE`, `INVERSE` | `DOT` = `cos(θ/2)` fra versori |
| interpolazione | `LERP`, `SLERP` | `SLERP` per animazioni (velocità angolare costante) |
| conversione | `TOMAT3`, `ROTATEVEC3` | ritornano `0` oppure `-1` se l'array è corto |

`INVERSE` è l'unica funzione con esito: su un quaternione nullo non esiste l'inversa e lo
stato in `st()` vale `-1`. `NORMALIZE` del nullo restituisce l'identità (scelta della
libreria C, documentata perché è un caso che si incontra con dati rumorosi).

Sample completo: `shared/samples/Quat/QuatOrientation.bas` (assetto di un drone, tutte e
19 le funzioni). MiniTest: `shared/minitests/quat/quat_core.bas`. Parità verificata su
IL, PCodeVM, VM/C e C99Native.

## 9. Numeri complessi (`COMPLEX`)

Un numero complesso è una coppia `(re, im)`: la parte reale e quella immaginaria. Serve
ogni volta che una soluzione esce dall'asse reale — un discriminante negativo, una
radice di un numero negativo, un'impedenza induttiva o capacitiva, un'esponenziale
oscillante. `COMPLEX` (in `libgmbmath`, `IMPORT COMPLEX`) è il `TYPE Complex` passato
per valore, senza handle né stato: ventotto funzioni pure.

```basic
IMPORT COMPLEX

DIM z AS Complex
z = COMPLEX.MAKE(3, 4)          ' 3 + 4i
PRINT COMPLEX.CABS(z)           ' 5        (modulo)
PRINT COMPLEX.ARG(z)            ' 0.927295 (fase, radianti)
PRINT COMPLEX.NORM2(z)          ' 25       (modulo al quadrato)
```

**Il prefisso `C` sulle funzioni trascendenti non è un vezzo.** `ABS`, `EXP`, `LOG`,
`SIN`, `COS`, `TAN`, `ASIN`, `ACOS`, `SINH`, `COSH`, `TANH` sono già funzioni scalari
del linguaggio: in `COMPLEX.ABS(z)` alcuni backend risolvono il nome sul builtin
scalare e il risultato sarebbe zero. La superficie usa quindi `CABS`, `CEXP`, `CLOG`,
`CSIN`, `CCOS`, `CTAN`, `CASIN`, `CACOS`, `CSINH`, `CCOSH`, `CTANH` — la stessa
convenzione di `<complex.h>` del C99. I nomi liberi restano naturali (`MAKE`, `ADD`,
`MUL`, `DIV`, `CONJ`, `ARG`, `NORM2`, `EQ`, `SQRT`, `POW`, …).

| gruppo | funzioni | note |
|---|---|---|
| aritmetica | `MAKE`, `ADD`, `SUB`, `MUL`, `DIV`, `NEG`, `CONJ`, `SCALE` | `DIV` per zero → `(0,0)`, documentato |
| misure | `CABS`, `ARG`, `NORM2`, `EQ` | `ARG` in `(-π, π]`; `EQ` con tolleranza esplicita |
| esponenziale | `CEXP`, `CLOG`, `CSQRT`, `POW` | ramo principale; `CLOG` di zero → `(-inf, 0)` |
| trigonometriche | `CSIN`, `CCOS`, `CTAN`, `CASIN`, `CACOS`, `CATAN` | `sin(i) = i·sinh(1)`, ecc. |
| iperboliche | `CSINH`, `CCOSH`, `CTANH`, `ASINH`, `ACOSH`, `ATANH` | round-trip con le inverse |

Due accortezze che evitano la maggior parte degli errori:

1. **non confrontare** `RE`/`IM` con `=` dopo una funzione trascendente: usare
   `EQ(a, b, tol)` con una tolleranza adeguata;
2. **assegnare a una variabile** il risultato di una funzione prima di passarlo a
   un'altra: `z = CSQRT(w)` e poi `CABS(z)`. Le chiamate innestate con un `TYPE` come
   argomento non sono gestite allo stesso modo da tutti i backend; la forma con le
   variabili è portabile e più leggibile.

Sample completo: `shared/samples/Complex/ComplexWorkbench.bas` (circuito RL-C, radici
di un polinomio, Mandelbrot, identità di Eulero). MiniTest:
`shared/minitests/complex/complex_core.bas`. Parità verificata su IL, PCodeVM, VM/C e
C99Native.

## 10. Trasformazioni 3D (`XFORM`)

Una scena 3D ha bisogno di tre operazioni che `MATRIX` non esprime: mettere un oggetto
in un punto (`T`), orientarlo (`R`), dargli una dimensione (`S`). `XFORM` (`IMPORT XFORM`)
le fornisce sulle **matrici piatte** che usa già `GFX3D` — 9 double per la 3×3, 16 per la
4×4 omogenea, **row-major** — con `0` = ok e `-1` per argomenti non validi.

```basic
DIM t(15) AS DOUBLE, r(15) AS DOUBLE, m(15) AS DOUBLE, p(2) AS DOUBLE
XFORM.MAT4ROTZ(MATH.DEG2RAD(90), r)    ' orientamento
XFORM.MAT4TRANSLATE(5, 0, 2, t)        ' posizione
XFORM.MAT4MUL(t, r, m)                 ' prima R, poi T
XFORM.MAT4POINT(m, 1, 0, 0, p)         ' (5, 1, 2)
```

| gruppo | funzioni |
|---|---|
| 3×3 | `MAT3IDENTITY`, `MAT3ROTX/Y/Z`, `MAT3MUL`, `MAT3NORMAL`, `MAT3FROM4` |
| 4×4 | `MAT4IDENTITY`, `MAT4TRANSLATE`, `MAT4SCALE`, `MAT4ROTX/Y/Z`, `MAT4MUL`, `MAT4INVERSEAFFINE`, `MAT4FROM3` |
| punti/direzioni | `MAT4POINT` (w = 1, divide per w), `MAT4DIR` (w = 0, ignora la traslazione) |

Tre punti che decidono la correttezza di una scena:

1. **`MUL(a, b)` applica prima `b`**: `MUL(T, R)` mette l'oggetto già orientato al suo
   posto; l'ordine inverso lo fa ruotare attorno all'origine del mondo;
2. **una direzione non è un punto**: con `MAT4DIR` la traslazione non entra, ed è quello
   che serve per le normali o per i raggi;
3. **con scale non uniformi le normali non usano la 3×3** ma la matrice normale
   `(M⁻¹)ᵀ` di `MAT3NORMAL`: senza, l'illuminazione delle superfici inclinate sbaglia.

Sample: `shared/samples/Xform/XformWorkbench.bas` (pannello solare su una torre: azimuth
e inclinazione, quattro angoli proiettati nel mondo, matrice normale sotto scala
`(2,1,1)`, inversa affine). MiniTest: `shared/minitests/xform/xform_core.bas`.

## 11. Trasformate: DCT, STFT, Haar, Hilbert (`SPECTRAL`)

`SPECTRAL` (`IMPORT SPECTRAL`) copre le trasformate che il linguaggio non aveva — DCT/DST
e inverse, DCT 2D, Haar, Hilbert, Z-trasformata, STFT/iSTFT — mentre i **filtri** restano
in `SIGNAL` (butter/cheby/ellip, lfilter, filtfilt, freqz, decimate, welch, savgol). La
regola dell'ABI è una sola e va ricordata: **la lunghezza del segnale è quella
dell'array dichiarato** (`DIM x(3)` = 4 campioni).

```basic
DIM x(7) AS DOUBLE, y(7) AS DOUBLE
FOR i = 0 TO 7 : x(i) = i : NEXT i
SPECTRAL.DCT(x, y)          ' DCT-II ortonormale: y(0) = somma/√8
y(6) = 0 : y(7) = 0         ' compressione: scarto il dettaglio fine
SPECTRAL.IDCT(y, x)         ' ricostruzione addolcita
```

| gruppo | funzioni |
|---|---|
| 1D | `DCT`, `DST`, `IDCT`, `HAAR`, `HILBERT` |
| 2D | `DCT2`, `IDCT2` (rows × cols, row-major) |
| spettro | `ZTRANSFORM` (in `z = re + i·im`), `STFT`, `ISTFT` |

Lo spettro della STFT è in coppie **(re, im) interleate**, frame per frame: per il frame
`f` e il bin `k`, `out(2*(f*(N/2+1)+k))` e `out(...+1)`. `STFT` ritorna il numero di
frame, `ISTFT` ricostruisce con overlap-add (esatto dove la finestra copre; con Hann e
`hop = N/2` i primi e gli ultimi `hop` campioni sono bordo).

`HILBERT` ruota la fase di -90° (`cos → -sin`): con il segnale originale dà il segnale
analitico, la cui ampiezza `sqrt(x² + H²)` è l'inviluppo. La DCT è la trasformata della
compressione: tenere i primi coefficienti e scartare gli ultimi è il gesto che sta dietro
a JPEG e MP3.

Sample: `shared/samples/Spectral/SpectralLab.bas` (tono a 1 kHz campionato a 8 kHz:
picco sul bin 1 con `|X| = 4`, ricostruzione esatta, compressione DCT di una rampa, DCT 2D
di una immagine 4×4, Haar su un gradino, inviluppo di Hilbert, Z-trasformata di
`[1, 0.5]`). MiniTest: `shared/minitests/spectral/spectral_core.bas`.

## 12. Vettori per la grafica (`VEC`)

`VECTOR` ha l'aritmetica dei vettori (somma, prodotto scalare e vettoriale, norma,
distanze, lerp, min/max). `VEC` (`IMPORT VEC`) aggiunge le undici operazioni che mancano a
una scena 3D: scomposizione rispetto a una direzione, Fresnel, bias dei raggi, limiti,
funzioni per componente e perpendicolare 2D.

```basic
DIM v(2) AS DOUBLE, n(2) AS DOUBLE, pr(2) AS DOUBLE, tg(2) AS DOUBLE
v(0)=3 : v(1)=-2 : v(2)=1
n(0)=0 : n(1)=1  : n(2)=0
VEC.PROJECT(v, n, pr)      ' (0, -2, 0)  componente lungo la normale
VEC.REJECT(v, n, tg)       ' (3,  0, 1)  componente tangente
```

| gruppo | funzioni | note |
|---|---|---|
| scomposizione | `PROJECT`, `REJECT` | `pr + tg = v`; direzione nulla → vettore zero |
| illuminazione | `SCHLICK` (Fresnel), `BIAS` | bias = `v + n·eps`, evita l'auto-intersezione |
| limiti | `VCLAMP`, `VCLAMP01`, `VCLAMPMAG` | modulo contro componente: sono cose diverse |
| per componente | `VABS`, `VSQRT`, `VPOW` | `VPOW(c, 1/2.2)` è la gamma sRGB |
| 2D | `PERP2D` | `(3,4) → (-4,3)` |

I prefissi `V` (`VABS`, `VSQRT`, …) hanno la stessa ragione di `COMPLEX`: `ABS`, `SQRT`,
`POW`, `CLAMP` sono funzioni scalari del linguaggio e in forma qualificata verrebbero
risolte sul builtin. `PROJECT`/`REJECT`/`SCHLICK`/`BIAS`/`PERP2D` non hanno questo
problema e restano naturali.

Il rimbalzo su un piano si scrive in tre righe: `PROJECT` per la normale, `REJECT` per la
tangente, poi la normale si inverte e perde energia (`pr(1) = -pr(1)*e`). L'esempio in
`shared/samples/Vec/VecWorkbench.bas` fa questo e in più Fresnel dell'acqua a tre angoli
(0.02 → 0.050625 → 0.598680), clamp di un colore HDR, gamma e perpendicolare 2D.
MiniTest: `shared/minitests/vec/vec_core.bas`.

## 13. Matrici sparse e solutori diretti (`SPARSE`)

Quando la matrice ha pochi non-zeri — griglie di un'equazione differenziale, grafi,
sistemi con migliaia di incognite — la rappresentazione densa di `MATRIX` costa `n²`.
`SPARSE` (`IMPORT SPARSE`) offre i tre contenitori classici e due solutori diretti, con
una superficie a **handle** (`FREE` per liberare):

| gruppo | funzioni |
|---|---|
| costruzione | `COONEW` (triplette), `COOSET`, `DENSETOCSR` (zeri scartati) |
| conversioni | `COOTOCSR`, `COOTOCSC`, `CSRTOCSC`, `CSCTOCSR`, `CSRTRANSPOSE`, `CSCTRANSPOSE` |
| ispezione | `SIZES` (righe, colonne, nnz), `TRIPLET` (riga, colonna, valore) |
| solutori | `SPCHOLFACTOR`/`SPCHOLSOLVE` (simmetrica definita positiva, CSC triangolo inferiore), `SPLUFACTOR`/`SPLUSOLVE` (completa, pivoting parziale) |

```basic
hcoo = SPARSE.COONEW(n, n, 3*n - 2)      ' tridiagonale
' ... COOSET per diagonale e sottodiagonali ...
hcsc = SPARSE.COOTOCSC(hcoo)
hf = SPARSE.SPLUFACTOR(hcsc)
IF hf > 0 THEN
  rc = SPARSE.SPLUSOLVE(hf, b)           ' b diventa la soluzione
  SPARSE.FREE(hf)
END IF
SPARSE.FREE(hcsc) : SPARSE.FREE(hcoo)
```

I fattori sono **riutilizzabili**: si fattorizza una volta e si risolve per più termini
notti (analisi con molti carichi, cicli a matrice costante). `CSRTRANSPOSE` restituisce
la CSC della trasposta: è il modo per passare da "righe" a "colonne" senza ricostruire.

Sample: `shared/samples/Sparse/SparseHeatRod.bas` (temperatura di regime in una barra:
13 non-zeri su 25, soluzione confrontata con `u(x) = x(1−x)/2`, errore 0). MiniTest:
`shared/minitests/sparse/sparse_core.bas` (Cholesky e LU con soluzione `(2/9, 1/9, 13/9)`,
trasposta non simmetrica, guardie).

## 14. Quadratura numerica (`QUAD`)

`NUMERICAL.INTEGRATE` è Simpson a passi fissi. `QUAD` (`IMPORT QUAD`) aggiunge i metodi
adattivi e quelli per gli integrali difficili. La funzione da integrare è una FUNCTION
passata per nome (`f(x AS DOUBLE) AS DOUBLE`).

| funzione | uso |
|---|---|
| `KRONROD(f, a, b, out)` | G7-K15 **con stima d'errore** in `out(1)`: se è grande si suddivide |
| `GAUSS(f, a, b, n)` | n nodi, esatto fino al grado `2n-1` |
| `ROMBERG(f, a, b, tol, maxit)` | estrapolazione, fino a tolleranza |
| `TANHSINH(f, a, b, acc, maxeval)` | singolarità agli estremi (`1/√x`), estremi infiniti |
| `TANHSINHCOMP(f2, …)` | come sopra, con `d` = distanza dall'estremo |
| `LEGENDRENODES`, `HERMITENODES` | nodi e pesi per quadrature proprie |

Il punto didattico è l'**errore stimato**: una regola a 15 punti non risolve una terza
armonica; la stima lo dice e si suddivide. Nel sample l'RMS di una tensione distorta su 20
sottointervalli dà `73.824115` con errore `1.8e-11`, mentre in un colpo solo la stima è
enorme.

```basic
DIM o(1) AS DOUBLE
QUAD.KRONROD(F, 0, 1, o)          ' o(0) = 1/3, o(1) = errore
QUAD.GAUSS(F, 0, 6.283185307179586, 10)
QUAD.TANHSINH(Portata, 0, 1, 1e-10, 2000)
```

Sample: `shared/samples/Quad/QuadLab.bas`. MiniTest: `shared/minitests/quad/quad_core.bas`.

## 15. Equazioni differenziali ordinarie (`ODE`)

`NUMERICAL.SOLVEODE` è un solo metodo esplicito. `ODE` (`IMPORT ODE`) aggiunge i metodi
impliciti, il simplettico e i problemi ai limiti.

| funzione | tipo | quando |
|---|---|---|
| `ADAMSBASHFORTH4`, `ADAMSMOULTON4` | multi-passo | problema liscio |
| `BDF2`, `IMPLICITMIDPOINT` | impliciti (maxit + tol) | **stiff**, conservazione |
| `VERLET` | simplettico | meccanica, energia a lungo termine |
| `BVPSHOOTING`, `BVPFD` | problemi ai limiti | `y'' = f(x,y,y')` con due estremi |

Due regole d'uso:

1. **lo stato non è in ingresso/uscita**: `y0()` in ingresso, `yout()` in uscita (la
   scrittura su un array in ingresso non è portabile);
2. **la callback riceve `(y(), t)`**: array prima, tempo dopo — è la forma che l'ABI
   regge su tutte e quattro le gambe (descrittore 148). Per i BVP la callback è scalare
   e `x` c'è.

```basic
FUNCTION Decay(y() AS DOUBLE, t AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(0) AS DOUBLE
  r(0) = -0.1 * (y(0) - 20)
  RETURN r
END FUNCTION

ODE.ADAMSBASHFORTH4(Decay, y0, 0, 60, 600, yout)     ' T(60) = 20.198300
ODE.VERLET(Acc, stato, 0, 2 * PI, 1000, yout)        ' energia 1.000000
ODE.BVPFD(Trave, 0, 1, 0, 0, nodi)                   ' y = x(1-x)/2
```

Sample: `shared/samples/Ode/OdeLab.bas` (raffreddamento, non autonomo, oscillatore,
trave). MiniTest: `shared/minitests/ode/ode_core.bas`.

## 16. Calcolo vettoriale (`VECCALC`)

`VECCALC` porta nel linguaggio il calcolo differenziale e integrale sui **campi dati
come formule**: si fornisce una `FUNCTION` e il punto in cui valutarla, e l'implementazione
approssima le derivate con **differenze centrali** di passo `h`. Non si costruisce una
griglia e non si precalcola nulla: il campo resta un'espressione BASIC, richiamata dal
runtime nativo (`libgmbmath`) durante il calcolo.

Serve a elettromagnetismo, fluidodinamica, meccanica e geometria differenziale, dove il
campo è una formula e servono gradiente, divergenza, rotore, laplaciano, curvatura,
integrali di linea e flusso. Per l'algebra vettoriale elementare (somma, prodotto scalare
e vettoriale, norma) si usano `VECTOR`/`VEC`; per l'evoluzione temporale, `ODE`.

### 16.1 Modello dei dati e contratto delle callback

Quattro forme di callback, secondo l'oggetto matematico:

| oggetto | firma BASIC | ritorno |
|---|---|---|
| campo scalare | `f(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE)` | `DOUBLE` |
| campo vettoriale | `F(x, y, z)` | `ARRAY OF DOUBLE` di 3 elementi |
| curva | `r(t AS DOUBLE)` | `ARRAY OF DOUBLE` di 3 elementi |
| patch | `S(u AS DOUBLE, v AS DOUBLE)` | `ARRAY OF DOUBLE` di 3 elementi |

Il runtime non deduce nulla: la callback deve restituire esattamente tre componenti.
Gli array di uscita (`out_()`) sono allocati dal chiamante con la dimensione minima
richiesta (3 per `GRADIENT`/`CURL`, 11 per `TNB`).

### 16.2 Parametri

| nome | tipo | significato |
|---|---|---|
| `h` | `DOUBLE` | passo delle differenze centrali, `> 0`; errore di troncamento `O(h²)` |
| `tol` | `DOUBLE` | soglia assoluta per `|∇·F|`/`|∇×F|` oppure tolleranza dell'integratore adattivo |
| `out_()` | `ARRAY OF DOUBLE` | buffer di uscita del chiamante |
| `x, y, z` | `DOUBLE` | punto di valutazione |
| `a, b` | `DOUBLE` | estremi dell'intervallo della curva |
| `u0,u1,v0,v1` | `DOUBLE` | dominio dei parametri della patch |

`h` controlla il compromesso tra troncamento e arrotondamento: `1e-4` è il valore
consigliato (errore assoluto tipico `1e-7`–`1e-8` su campi regolari), `1e-3` per campi con
derivate molto variabili, `1e-5` per funzioni molto lisce. Sotto `1e-7` domina il rumore
in virgola mobile. `h = 0` è rifiutato.

`tol` ha due usi distinti: nelle funzioni booleane è la soglia su `|∇·F|` o `|∇×F|`
(`1e-6` per campi esatti); negli integrali è la tolleranza di Simpson adattiva
(`1e-6` dà sei decimali stabili).

### 16.3 Riferimento delle funzioni

| funzione | firma | risultato |
|---|---|---|
| `GRADIENT` | `(f, x, y, z, h, out_())` | `out_(0..2) = (∂f/∂x, ∂f/∂y, ∂f/∂z)`; ritorna `0`/`-1` |
| `LAPLACIAN` | `(f, x, y, z, h)` | `∇²f` con stencil a 7 punti; `DOUBLE` |
| `DIVERGENCE` | `(F, x, y, z, h)` | `∇·F`; `DOUBLE` |
| `CURL` | `(F, x, y, z, h, out_())` | `out_(0..2) = ∇×F`; ritorna `0`/`-1` |
| `ISSOLENOIDAL` | `(F, x, y, z, h, tol)` | `1` se `|∇·F| ≤ tol`, `0` altrimenti, `-1` su parametri invalidi |
| `ISIRROTATIONAL` | `(F, x, y, z, h, tol)` | `1` se `|∇×F| ≤ tol`, `0` altrimenti, `-1` su parametri invalidi |
| `ARCLENGTH` | `(r, a, b, tol)` | `∫ |r'(t)| dt` su `[a,b]`; `DOUBLE` |
| `TNB` | `(r, t, h, out_())` | `out_(0..2)=T`, `(3..5)=N`, `(6..8)=B`, `out_(9)=κ`, `out_(10)=τ`; `0`/`-1` |
| `LINESCALAR` | `(f, r, a, b, tol)` | `∫ f(r(t))·|r'(t)| dt`; `DOUBLE` |
| `LINEWORK` | `(F, r, a, b, tol)` | `∫ F(r(t))·r'(t) dt`; `DOUBLE` |
| `FLUX` | `(F, S, u0, u1, v0, v1, tol)` | `∫∫ F(S)·(S_u × S_v) du dv`; `DOUBLE` |

Convenzioni sui valori di ritorno: le funzioni che scrivono in `out_()` ritornano
`LONG` (`0` successo, `-1` argomenti invalidi: `h = 0`, callback nulla, `out_()` troppo
corto); le funzioni che ritornano `DOUBLE` ritornano `0` con callback nulla o `h = 0`
(il controllo va fatto sui parametri). `TNB` richiede `out_()` di almeno 11 elementi;
per una curva rettilinea restituisce `N = B = 0`, `κ = τ = 0`, senza amplificare il
rumore numerico.

L'orientamento di `FLUX` è quello del prodotto `S_u × S_v`: scambiare i due parametri
cambia il segno del risultato.

### 16.4 Esempio eseguito: le identità di Maxwell

```basic
IMPORT VECCALC

FUNCTION Pot(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS DOUBLE
  RETURN 1 / MATH.SQR(x * x + y * y + z * z)      ' V = 1/r
END FUNCTION

FUNCTION Elec(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(2) AS DOUBLE, d AS DOUBLE
  d = (x * x + y * y + z * z)
  d = d * MATH.SQR(d)                             ' d = |r|^3
  r(0) = x / d : r(1) = y / d : r(2) = z / d      ' E = r/|r|^3
  RETURN r
END FUNCTION

DIM g(2) AS DOUBLE
VECCALC.GRADIENT(Pot, 1, 1, 1, 0.0001, g)         ' (-0.192450, -0.192450, -0.192450)
VECCALC.DIVERGENCE(Elec, 1, 1, 1, 0.0001)         ' 3e-9   -> solenoidale
VECCALC.LAPLACIAN(Pot, 1, 1, 1, 0.0001)           ' -3e-8  -> armonica
```

In `(1,1,1)` si ha `|r| = √3` ed `E = (1,1,1)/(3√3) ≈ (0.192450, …)`: il gradiente di
`1/r` è `−E`, la divergenza di `E` è nulla fuori dalla carica (campo solenoidale), il
rotore è nullo (campo conservativo) e il laplaciano di `1/r` è nullo (funzione armonica).
Il sample `shared/samples/VecCalc/VecCalcLab.bas` esegue queste verifiche e ne stampa
gli **errori** (`|grad V + E| ≈ 7e-10`, `|div E| ≈ 3e-9`), oltre a:

- campo non simmetrico `f = xyz` → gradiente `(12, 8, 6)` in `(2,3,4)` e
  `F = (x²,y²,z²)` → divergenza `18`, che verificano le tre derivate parziali distinte;
- rotazione rigida `(−y, x, 0)` → `div = 0`, `curl = (0,0,2)`;
- elica `(cos t, sin t, t/5)` → lunghezza `6.407617`, `κ = 0.961538`, `τ = 0.192308`
  (formule `2π√(1+a²)`, `1/(1+a²)`, `a/(1+a²)`);
- retta `(t, 2t, 3t)` → `κ = τ = 0`;
- lavoro e integrale scalare sul cerchio unitario → `2π`, flusso verticale sul
  paraboloide → `1`;
- guardie: `out_()` corto → `-1`, `h = 0` → `-1`/`0`.

### 16.5 Parità e limiti

La superficie è unica (`shared/include/veccalc.inc`), quindi firme e parametri sono
identici su IL, PCodeVM, VM/C e C99Native; il MiniTest
`shared/minitests/veccalc/veccalc_core.bas` e il sample sono verificati sulle quattro
gambe (`zsh tools/sh/test_minitests_parity.sh "veccalc/veccalc_core.bas"`,
`zsh tools/sh/parity4 ../shared/samples/VecCalc/VecCalcLab.bas`).

Il calcolo è in `double` e le derivate sono approssimate: il risultato è accurato quanto
`h` e la regolarità del campo consentono. Per derivate esatte di una funzione di una
variabile si usa `NUMERICAL.DIFFERENTIATE`; per campi definiti su griglie o dati
sperimentali servono strumenti diversi (interpolazione, `SPARSE`, `MESH`).

`VECCALC` non copre:
- algebra vettoriale elementare (somma, prodotto, norma, proiezione): `VECTOR`, `VEC`;
- matrici di trasformazione 3D: `XFORM`;
- integrazione di funzioni di una variabile e ODE: `QUAD`, `ODE`;
- differenziazione simbolica: non è un CAS.

## 17. Trading quantitativo (`QUANT`)

La superficie `TRADING.*` della StdLib è in migrazione verso una **libreria nativa FFI**
(`libgmbquant`) esterna al compilatore. Le ondate 1-2 espongono 69 funzioni in sei gruppi,
con gli stessi nomi e la stessa semantica dei builtin:

| file `.inc` | namespace | contenuto |
|---|---|---|
| `trading.inc` | `TRADING_CORE` | valutazione azionaria, CAPM, rendimenti, spread |
| `metrics.inc` | `METRICS` | performance e rischio (Sharpe/Sortino/Calmar, drawdown, VaR/CVaR, alpha/beta, momenti, Jarque-Bera) |
| `volatility.inc` | `VOLATILITY` | volatilità annualizzata, downside deviation |
| `fundamentals.inc` | `FUNDAMENTALS` | EPS/DPS/NAV, P/E, P/B, P/S, payout, dividend yield |
| `bonds.inc` | `BONDS` | prezzo/rendimento obbligazionario, duration, BEY, current yield, YTM, zero-coupon |
| `derivatives.inc` | `DERIVATIVES` | Black-Scholes, Merton, binomiale CRR, 11 greche |

L'ondata 3 aggiunge `PORTFOLIO` e `BACKTEST`.

### 17.1 Uso e convenzioni

```basic
IMPORT TRADING_CORE
IMPORT METRICS
IMPORT VOLATILITY
IMPORT FUNDAMENTALS

DIM r(4) AS DOUBLE
r(0) = 0.01 : r(1) = -0.02 : r(2) = 0.03 : r(3) = 0.00 : r(4) = 0.015

PRINT METRICS.TotalReturn(r)                   ' 0.034786 (frazione)
PRINT METRICS.SHARPERATIO(r)                   ' 5.982583
PRINT VOLATILITY.AnnualizedVolatility(r, 252)  ' 0.294856
PRINT FUNDAMENTALS.DividendYield(5, 100)       ' 5 (percentuale)
```

I **rendimenti sono frazioni** (`0.05` = 5%). `DividendYield` è l'eccezione: è in
percentuale, come il builtin. Un input non valido (denominatore zero, serie troppo
corta, puntatore nullo) restituisce `0`; nessuna eccezione attraversa l'FFI. Gli array
viaggiano per riferimento con l'estensione automatica `d0..dN-1` (la `DECLARE` non
elenca la lunghezza). `JarqueBeraTest` restituisce `ARRAY OF DOUBLE` `[statistica,
p-value]`.

Le funzioni di `BONDS` prendono le date come **`OADate`** — il seriale `DateTime` di
GMBasic, giorni dal 1899-12-30 — e si passano direttamente da
`DATETIME.DATEVALUE("AAAA-MM-GG")` (vocazione `DateTime`→`DOUBLE`, nessun tipo data
nell'FFI). `frequency` vale 1, 2 o 4; `model` (`0` = `Trading.Bonds`, `1` =
`Numerics.Price` con basis 0) è opzionale e vale 0.

Le funzioni di `DERIVATIVES` che distinguono la gamba prendono la stringa `type`
(`"CALL"`/`"PUT"`, case-insensitive); `vol` è la volatilità annualizzata in frazione e
`T` il tempo a scadenza in anni. Con `T ≤ 0` o `vol ≤ 0` il prezzo degenera nel valore
intrinseco. `Vega` e `Rho` sono per 1% di movimento, `Theta` e `Charm` per giorno.

Il nome `TRADING_CORE` è **di transizione**: finché esistono i builtin `TRADING.*` il
compilatore rifiuta una funzione utente con lo stesso nome/arità nel namespace
`TRADING`; a fine migrazione il namespace prenderà il nome `TRADING`.

### 17.2 Riferimento delle funzioni

`TRADING_CORE` (12):

| funzione | firma | risultato |
|---|---|---|
| `StockValueZeroGrowth` | `(dividend, requiredRate)` | `D/r` |
| `StockValueConstGrowth` | `(nextDividend, rate, growth)` | `D1/(r−g)` (Gordon) |
| `PreferredStockValue` | `(dividend, discountRate)` | `D/r` |
| `CAPM` | `(rf, beta, marketReturn)` | `rf + β(rm−rf)` |
| `RiskPremium` | `(assetReturn, rf)` | `r−rf` |
| `TotalStockReturn` | `(p0, p1, dividends)` | `((p1−p0)+div)/p0` |
| `CapitalGainsYield` | `(p0, p1)` | `(p1−p0)/p0` |
| `HoldingPeriodReturn` | `(returns())` | `∏(1+r)−1` |
| `GeometricMeanReturn` | `(returns())` | `∏(1+r)^(1/n)−1` |
| `GrowthRate` | `(retentionRate, roe)` | `retention·ROE` |
| `TaxEquivalentYield` | `(taxFreeYield, taxRate)` | `tfy/(1−tax)` |
| `BidAskSpread` | `(bid, ask)` | `ask−bid` |

`METRICS` (18):

| funzione | firma | risultato |
|---|---|---|
| `TotalReturn` | `(returns())` | `∏(1+r)−1` |
| `AnnualizedReturn` | `(returns(), ppy)` | `(1+TR)^(ppy/n)−1` |
| `SHARPERATIO` | `(returns())` | `mean/std·√252` |
| `SortinoRatio` | `(returns(), rf, ppy)` | `√ppy·mean(r−rf)/downside(r−rf,0)` |
| `CalmarRatio` | `(returns(), ppy)` | `AR/|maxDD|` |
| `InformationRatio` | `(returns(), bench(), ppy)` | `√ppy·mean(attivo)/std(attivo)` |
| `TreynorRatio` | `(returns(), bench(), rf, ppy)` | `(AR−AR(rf))/β` |
| `MaxDrawdown` | `(returns())` | minimo drawdown composto (negativo) |
| `ValueAtRisk` | `(returns(), confidence)` | percentile nearest al livello `1−conf` |
| `ConditionalValueAtRisk` | `(returns(), confidence)` | media della coda `≤ VaR` |
| `Beta` | `(returns(), bench())` | `cov/var` |
| `Alpha` | `(returns(), bench(), rf, ppy)` | Jensen annualizzato |
| `ReturnsSkewness` | `(returns())` | skew campionaria corretta |
| `ReturnsKurtosis` | `(returns())` | curtosi in eccesso non distorta |
| `JarqueBeraTest` | `(returns())` | `[statistica, p-value]` |
| `AnnualizedTrackingError` | `(returns(), bench(), ppy)` | `√ppy·std(attivo)` |
| `OmegaRatio` | `(returns(), threshold)` | guadagni/perdite (`+INF` se perdite 0) |
| `UlcerIndex` | `(returns())` | `√(media dd%² composti)` |

`VOLATILITY` (3): `AnnualizedVolatility(returns, ppy)` = `√ppy·std`;
`DownsideDeviation(returns, threshold)`; `AnnualizedDownsideDeviation(returns, threshold,
ppy)` = `√ppy·downside`.

`FUNDAMENTALS` (11): `BookValuePerShare(equity, azioni)`, `DilutedEPS(ni, azioni,
strumenti)`, `DividendPayoutRatio(div, ni)`, `DividendYield(div, prezzo)` (**×100**),
`DPS(div, azioni)`, `EPS(ni, azioni)`, `EquityMultiplier(attivo, equity)`, `NAV(attivo,
passivo, azioni)`, `PBRatio(prezzo, book)`, `PERatio(prezzo, eps)`, `PSRatio(prezzo, sps)`.

`BONDS` (9):

| funzione | firma | risultato |
|---|---|---|
| `BondPrice` | `(settle, maturity, rate, yld, redemption, frequency[, model])` | prezzo *dirty* (rateo incluso) |
| `BondYield` | `(settle, maturity, rate, price, redemption, frequency[, model])` | YTM dal prezzo (Newton) |
| `PRICESIMPLE` | `(settle, maturity, rate, yld, redemption, frequency)` | prezzo a periodi interi, senza rateo |
| `DURATION` | `(settle, maturity, rate, yld, frequency, basis, calcMethod)` | Macaulay; `calcMethod 1` → modificata |
| `BondEquivalentYield` | `(face, price, giorni)` | `((F−P)/P)·(365/giorni)` |
| `CurrentYield` | `(cedoleAnnuali, price)` | `cedole/prezzo` |
| `YieldToMaturity` | `(cedola, nominale, price, anni)` | `(C+(F−P)/t)/((F+P)/2)` |
| `ZeroCouponBondValue` | `(nominale, tasso, tempo)` | `F/(1+r)^t` |
| `ZeroCouponBondYield` | `(nominale, price, tempo)` | `(F/P)^(1/t)−1` |

Modello `BondPrice`: `N` cedole residue contate dal prossimo stacco, `DSC` giorni
settlement→stacco su base 30/360, `E` giorni del periodo cedolare; il prezzo somma le
cedole attualizzate a `1+y/f` e sottrae il rateo `C·(E−DSC)/E`. Il modello `1`
(`Numerics.Price`, basis 0) usa periodi frazionari `(DSC+E(k−1))/E` e rateo `1−DSC/E`.

`DERIVATIVES` (16):

| funzione | firma | risultato |
|---|---|---|
| `BsCall` / `BsPut` | `(S, K, T, r, vol[, q])` | Black-Scholes; `q ≠ 0` → Merton |
| `MertonCall` / `MertonPut` | `(S, K, T, r, vol[, q])` | `q` = dividend yield continuo |
| `BinomialOption` | `(S, K, T, r, vol, steps, type)` | albero CRR europeo |
| `Delta` | `(S, K, T, r, vol, type)` | `±N(d1)` |
| `Gamma` | `(S, K, T, r, vol)` | `φ(d1)/(S·σ√T)` |
| `Vega` | `(S, K, T, r, vol)` | per 1% di volatilità |
| `Theta` | `(S, K, T, r, vol, type)` | per giorno |
| `Rho` | `(S, K, T, r, vol, type)` | per 1% di tasso |
| `Vanna` / `Volga` | `(S, K, T, r, vol)` | derivate seconde miste |
| `Charm` | `(S, K, T, r, vol, type)` | delta-decay, per giorno |
| `DeltaSpeed` | `(S, K, T, r, vol)` | `−Γ/S·(d1/(σ√T)+1)` |
| `Zomma` | `(S, K, T, r, vol)` | `∂Γ/∂σ` |
| `GammaColor` | `(S, K, T, r, vol)` | `∂Γ/∂T` |

Il binomiale è Cox-Ross-Rubinstein: `u = e^{σ√Δt}`, `d = 1/u`,
`p = (e^{rΔt}−d)/(u−d)`; se `p ∉ [0,1]` restituisce `0`. Con `steps` crescenti
converge a Black-Scholes (200 passi: 10.440591 contro 10.450584).

### 17.3 Esempio eseguito

`shared/samples/Quant/QuantLab.bas` è un laboratorio in sei sezioni: un titolo (prezzo 50,
dividendo 2, required return 10%, crescita 3%), un fondo (5 rendimenti periodici contro un
benchmark), un BTP biennale cedola 5% e una call ATM (`S=K=100`, `T=1`, `r=5%`, `vol=20%`).
Valori prodotti (76 righe):

```
A) valutazione azionaria
 DDM crescita zero = 20.000000        DDM Gordon = 28.571429
 CAPM = 0.114000                      rendimento totale = 0.120000
 holding period = 0.059870            media geometrica = 0.019571
B) performance e rischio
 rendimento totale = 0.033953         annualizzato = 0.083432
 Sharpe = 6.831246                    Sortino = 5.997036
 Calmar = 243.370996                  Information = 5.328636
 max drawdown = -0.018000             VaR 80% = 0.004000
 CVaR 80% = -0.007000                 beta = 1.537791
 alpha = 0.831240                     Jarque-Bera = 1.437129, 0.487451
 skewness = -0.953739                 curtosi = 1.805478
 tracking error = 0.094583            Omega = 4.000000
C) volatilità
 annualizzata = 0.250847              downside = 0.018000
D) fondamentali
 EPS = 2.000000                       P/E = 25.000000
 dividend yield = 5.000000 %          P/B = 2.000000
E) obbligazioni (BTP 2024->2026, cedola 5%, y 5%)
 prezzo modello Bonds = 100.000000    prezzo modello Numerics = 95.680812
 duration = 1.952381                  duration modificata = 1.859410
 yield a 96 = 0.072191                current yield = 0.050000
 YTM (5,100,105,10) = 0.043902        ZCB valore = 78.352617
F) derivati (call ATM S=K=100, T=1, r=5%, vol=20%)
 Black-Scholes call = 10.450584       put = 5.573526
 call con q 2% = 9.227006             Merton put (q 2%) = 6.330081
 binomiale 200 passi = 10.440591      delta call/put = 0.636831/-0.363169
 gamma = 0.018762                     vega (1%) = 0.375240
 theta (giorno) = -0.017573           rho (1%) = 0.532325
 vanna/volga = -0.281430/9.850059     charm = -0.000253
 speed/zomma/color = -0.000516/-0.088885/-0.025798
```

La differenza fra i due modelli di `BondPrice` sullo stesso titolo (100 contro 95.680812)
è il rateo: il modello `Numerics.Price` sconta il rateo maturato `1−DSC/E`, quello
`Trading.Bonds` lo sottrae come `C·(E−DSC)/E` sulla cedola intera. Il binomiale a 200
passi conferma Black-Scholes a meno di 0.01.

### 17.4 Parità e limiti

La superficie è unica (`shared/include/*.inc`) e la libreria è una sola
(`libgmbquant`): firme, aritmetica e risultati sono identici su IL, PCodeVM, VM/C e
C99Native. I MiniTest `shared/minitests/quant/quant_core.bas` (44 funzioni) e
`shared/minitests/quant/quant_bonds_derivatives.bas` (25) verificano tutta la superficie
con valori chiusi; il sample `QuantLab.bas` produce 76 righe identiche sulle quattro
gambe.

Limiti dell'ondata 2: restano da migrare l'ottimizzazione di portafoglio e il backtest
(ondata 3); le funzioni non allocano e non ridimensionano array; i risultati `ARRAY`
sono solo quelli di `JarqueBeraTest` (2 elementi). La libreria è in `shared/libs/quant`
(sorgenti C + shim FFI) e in `GMBSuite/gmbquant` (copia di sviluppo con self-test).

## 18. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| percentile inatteso | convenzione o difetto del backend | verifica su un altro backend, non "aggiusta" l'input |
| asimmetria diversa da 0 su dati simmetrici | idem | confronta con SciPy (`StatsSciPyParity`) |
| varianza "sbagliata" | divisore `n` contro `n-1` | controlla la convenzione della guida |
| `NaN` in un risultato | dominio non valido (log di 0, sqrt negativo) | valida l'input prima |
| confronto fra double che fallisce | uguaglianza esatta | usa una tolleranza |
| rotazione che "scala" | quaternione non normalizzato | `QUAT.NORMALIZE` prima di usarlo |
| composizione invertita | ordine di `MUL(a, b)` | `MUL(a, b)` = prima `b`, poi `a` |

## 19. Dove andare adesso

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

### COMPLEX-Guida-Utente

*Fonte: `COMPLEX-Guida-Utente`*


## A cosa serve

## Modello dei dati

TYPE Complex
  RE AS DOUBLE
  IM AS DOUBLE
END TYPE

DIM z AS Complex
z = COMPLEX.MAKE(3, 4)
PRINT COMPLEX.CABS(z)        ' 5       (modulo)
PRINT COMPLEX.ARG(z)         ' 0.927295 (fase in radianti)
PRINT z.RE; " "; z.IM        ' 3  4


## Perché alcune funzioni hanno il prefisso `C`

## Convenzioni matematiche
| aspetto | convenzione |
|---|---|
| fase (`ARG`) | radianti, intervallo `(-π, π]`, come `atan2(IM, RE)` |
| modulo (`CABS`) | `sqrt(RE² + IM²)`; `NORM2` è il modulo al quadrato (evita la radice) |
| `CLOG` | ramo principale: `log|z| + i·arg(z)` |
| `CSQRT` | radice con parte immaginaria concorde a `IM` (ramo principale) |
| `POW(z, w)` | `exp(w · log z)`; l'esponente è complesso |
| `EQ` | tolleranza esplicita sullo scarto di `RE` e `IM` (stesso `tol` per entrambi) |
| confronto | mai `RE =` e `IM =` sui risultati di funzioni trascendenti: usare `EQ` |

## Aritmetica

a = COMPLEX.MAKE(1, 2)
b = COMPLEX.MAKE(3, 4)

COMPLEX.ADD(a, b)        '  4 + 6i
COMPLEX.SUB(a, b)        ' -2 - 2i
COMPLEX.MUL(a, b)        ' -5 + 10i     (i² = -1)
COMPLEX.DIV(a, b)        '  0.44 + 0.08i
COMPLEX.NEG(a)           ' -1 - 2i
COMPLEX.CONJ(a)          '  1 - 2i      (coniugato: cambia segno a IM)
COMPLEX.SCALE(a, 2)      '  2 + 4i      (scalare reale)


## Misure

z = COMPLEX.MAKE(3, 4)
COMPLEX.CABS(z)     ' 5
COMPLEX.ARG(z)      ' 0.927295  (rad)
COMPLEX.NORM2(z)    ' 25
u = COMPLEX.MAKE(3, 4)
COMPLEX.EQ(z, u, 0.000001)    ' 1
u = COMPLEX.MAKE(3, 4.1)
COMPLEX.EQ(z, u, 0.000001)    ' 0


## Esponenziale, logaritmo, potenza

' identità di Eulero: e^(i·pi) = -1
t = COMPLEX.MAKE(0, MATH.PI())
z = COMPLEX.CEXP(t)
PRINT z.RE; " + j("; z.IM; ")"        ' -1 + j(0)

' log(-1) = i·pi
t = COMPLEX.MAKE(-1, 0)
z = COMPLEX.CLOG(t)
PRINT z.RE; " + j("; z.IM; ")"        ' 0 + j(3.14159)

' i² = -1
t = COMPLEX.MAKE(0, 1)
u = COMPLEX.MAKE(2, 0)
z = COMPLEX.POW(t, u)

' radice quadrata di un numero negativo: sqrt(-1) = i


## Trigonometriche e iperboliche

' sin(i) = i·sinh(1) = 1.175201i
' cos(i) = cosh(1)     = 1.543081
' tan(i) = i·tanh(1)   = 0.761594i
' sinh(i) = i·sin(1)   = 0.841471i
' cosh(i) = cos(1)     = 0.540302
' tanh(i) = i·tan(1)   = 1.557408i


## Un esempio completo

## Stile portabile

z = COMPLEX.CSQRT(w)      ' prima
v = COMPLEX.CABS(z)       ' poi

' invece di: COMPLEX.CABS(COMPLEX.CSQRT(w))


## Riferimento completo delle funzioni
| funzione | firma | ritorna | significato |
|---|---|---|---|
| `MAKE` | `(re, im AS DOUBLE)` | Complex | numero esplicito |
| `ADD` | `(a, b AS Complex)` | Complex | somma |
| `SUB` | `(a, b AS Complex)` | Complex | differenza |
| `MUL` | `(a, b AS Complex)` | Complex | prodotto (i² = -1) |
| `DIV` | `(a, b AS Complex)` | Complex | quoziente; divisore nullo → `(0,0)` |
| `NEG` | `(a AS Complex)` | Complex | opposto |
| `CONJ` | `(a AS Complex)` | Complex | coniugato `(re, -im)` |
| `SCALE` | `(a AS Complex, s AS DOUBLE)` | Complex | prodotto per uno scalare reale |
| funzione | firma | ritorna | significato |
|---|---|---|---|
| `CABS` | `(a AS Complex)` | DOUBLE | modulo `|z|` |
| `ARG` | `(a AS Complex)` | DOUBLE | fase in radianti, `(-π, π]` |
| `NORM2` | `(a AS Complex)` | DOUBLE | modulo al quadrato |
| `EQ` | `(a, b AS Complex, tol AS DOUBLE)` | LONG | 1 se `|dre| <= tol` e `|dim| <= tol` |
| funzione | firma | ritorna | significato |
|---|---|---|---|
| `CEXP` | `(z AS Complex)` | Complex | `e^z` |
| `CLOG` | `(z AS Complex)` | Complex | ramo principale; zero → `(-inf, 0)` |
| `CSQRT` | `(z AS Complex)` | Complex | radice principale |
| `POW` | `(z, w AS Complex)` | Complex | `z^w` con esponente complesso |
| funzione | firma | ritorna | significato |
|---|---|---|---|
| `CSIN` | `(z AS Complex)` | Complex | seno |
| `CCOS` | `(z AS Complex)` | Complex | coseno |
| `CTAN` | `(z AS Complex)` | Complex | tangente (`sin/cos`) |
| `CASIN` | `(z AS Complex)` | Complex | arcoseno, ramo principale |
| `CACOS` | `(z AS Complex)` | Complex | arcocoseno, ramo principale |
| `CATAN` | `(z AS Complex)` | Complex | arcotangente |
| funzione | firma | ritorna | significato |
|---|---|---|---|
| `CSINH` | `(z AS Complex)` | Complex | seno iperbolico |
| `CCOSH` | `(z AS Complex)` | Complex | coseno iperbolico |
| `CTANH` | `(z AS Complex)` | Complex | tangente iperbolica |
| `ASINH` | `(z AS Complex)` | Complex | settore iperbolico inverso |
| `ACOSH` | `(z AS Complex)` | Complex | settore iperbolico inverso |
| `ATANH` | `(z AS Complex)` | Complex | settore iperbolico inverso |

## Parità e verifica

## Cosa non è qui, e perché
| bisogno | dove |
|---|---|
| aritmetica reale, arrotondamenti, combinatoria | `MATH` |
| matrici e decomposizioni (anche a coefficienti complessi) | `MATRIX`, `LINALG` |
| FFT e spettri da campioni reali | `NUMERICAL.FFT` / `FFTCOMPLEX` |
| tensori e autodiff | `TORCH` |
| numeri complessi come tipo del motore grafico (quaternioni) | `QUAT` |

### XFORM-Guida-Utente

*Fonte: `XFORM-Guida-Utente`*


## A cosa serve

## Modello dei dati

DIM m3(8) AS DOUBLE      ' 3x3: m3(riga*3 + colonna)
DIM m4(15) AS DOUBLE     ' 4x4: m4(riga*4 + colonna)

XFORM.MAT4TRANSLATE(5, 0, 2, m4)
PRINT m4(3); " "; m4(7); " "; m4(11)     ' 5  0  2   (ultima colonna = traslazione)


[ r00 r01 r02 tx ]
[ r10 r11 r12 ty ]
[ r20 r21 r22 tz ]
[  0   0   0  1  ]


## Convenzioni
| aspetto | convenzione |
|---|---|
| ordine dei valori | row-major, contiguo |
| angoli | radianti (usare `D2R` o `MATH.DEG2RAD`) |
| composizione | `MUL(a, b)` applica **prima `b`, poi `a`** (prodotto di matrici) |
| punto | `MAT4POINT` usa `w = 1` e divide per `w` se la prospettiva lo cambia |
| direzione | `MAT4DIR` usa `w = 0`: la traslazione **non** la tocca |
| normali | con scale non uniformi usare `MAT3NORMAL` (inversa trasposta), non la 3×3 |
| esiti | `0` = ok, `-1` = argomento non valido o matrice singolare |

## Costruire una trasformazione

DIM t(15) AS DOUBLE, r(15) AS DOUBLE, s(15) AS DOUBLE, m(15) AS DOUBLE, tmp(15) AS DOUBLE

XFORM.MAT4TRANSLATE(5, 0, 2, t)     ' sposta
XFORM.MAT4ROTZ(MATH.DEG2RAD(30), r) ' azimuth
XFORM.MAT4SCALE(2, 1, 1, s)         ' scala

XFORM.MAT4MUL(r, s, tmp)            ' prima la scala, poi la rotazione
XFORM.MAT4MUL(t, tmp, m)            ' infine la traslazione


## Punti, direzioni, normali

DIM p(2) AS DOUBLE

' un punto: risente della traslazione (w = 1)
XFORM.MAT4POINT(m, 1, 0, 0, p)

' una direzione: NON risente della traslazione (w = 0)
XFORM.MAT4DIR(m, 1, 0, 0, p)


## Inversa affine

DIM inv(15) AS DOUBLE
IF XFORM.MAT4INVERSEAFFINE(m, inv) = 0 THEN
  ' il punto del mondo torna alle coordinate locali
  XFORM.MAT4POINT(inv, wx, wy, wz, p)
END IF


## Un esempio completo

## Riferimento completo delle funzioni
| funzione | firma | significato |
|---|---|---|
| `MAT3IDENTITY` | `(m())` | matrice identità |
| `MAT3ROTX` | `(rad, m())` | rotazione attorno a X |
| `MAT3ROTY` | `(rad, m())` | rotazione attorno a Y |
| `MAT3ROTZ` | `(rad, m())` | rotazione attorno a Z |
| `MAT3MUL` | `(a(), b(), out_())` | prodotto `a·b` (prima `b`, poi `a`) |
| `MAT3NORMAL` | `(a(), out_())` | matrice normale `(M⁻¹)ᵀ`; `-1` se singolare |
| `MAT3FROM4` | `(m4(), out_())` | estrae la 3×3 da una 4×4 |
| funzione | firma | significato |
|---|---|---|
| `MAT4IDENTITY` | `(m())` | identità |
| `MAT4TRANSLATE` | `(x, y, z, m())` | traslazione |
| `MAT4SCALE` | `(x, y, z, m())` | scala sugli assi |
| `MAT4ROTX` | `(rad, m())` | rotazione attorno a X |
| `MAT4ROTY` | `(rad, m())` | rotazione attorno a Y |
| `MAT4ROTZ` | `(rad, m())` | rotazione attorno a Z |
| `MAT4MUL` | `(a(), b(), out_())` | composizione `a·b` |
| `MAT4INVERSEAFFINE` | `(a(), out_())` | inversa affine; `-1` se singolare |
| `MAT4POINT` | `(m(), x, y, z, out_())` | punto con divisione per `w` (3 double) |
| `MAT4DIR` | `(m(), x, y, z, out_())` | direzione (traslazione ignorata) |
| `MAT4FROM3` | `(m3(), out_())` | 4×4 con la 3×3 data e traslazione nulla |

## Parità e verifica

## Cosa non è qui, e perché
| bisogno | dove |
|---|---|
| rotazioni come quaternioni, SLERP | `QUAT` |
| matrice di vista/proiezione, look-at, frustum, ray casting | `GFX3D` |
| matrici generiche, decomposizioni, determinante | `MATRIX`, `LINALG` |
| rotazione di un poligono nel piano (gradi, attorno all'origine) | `GEO.ROTATE` |

### SPECTRAL-Guida-Utente

*Fonte: `SPECTRAL-Guida-Utente`*


## A cosa serve

## Modello dei dati

DIM x(3) AS DOUBLE, y(3) AS DOUBLE
x(0)=1 : x(1)=2 : x(2)=3 : x(3)=4
SPECTRAL.DCT(x, y)            ' DCT di 4 campioni; y(0)=5, ...


out(2*(f*(N/2+1) + k))      parte reale
out(2*(f*(N/2+1) + k) + 1)  parte immaginaria


## DCT / DST e inverse

DIM x(7) AS DOUBLE, y(7) AS DOUBLE, back(7) AS DOUBLE

FOR i = 0 TO 7 : x(i) = i : NEXT i
SPECTRAL.DCT(x, y)          ' coefficienti in y
y(5) = 0 : y(6) = 0 : y(7) = 0   ' scarto il dettaglio fine
SPECTRAL.IDCT(y, back)      ' ricostruzione addolcita


## DCT 2D

DIM img(15) AS DOUBLE, coeff(15) AS DOUBLE   ' 4x4
FOR i = 0 TO 15 : img(i) = i : NEXT i
SPECTRAL.DCT2(img, 4, 4, coeff)              ' coeff(0) = 30 = somma/4
SPECTRAL.IDCT2(coeff, 4, 4, img)             ' ricostruzione esatta


## Wavelet di Haar

## Trasformata di Hilbert

SPECTRAL.HILBERT(x, h)
FOR i = 0 TO n-1
  env = MATH.SQR(x(i)*x(i) + h(i)*h(i))     ' inviluppo
NEXT i


## Z-trasformata

DIM h(1) AS DOUBLE
' risposta all'impulso [1, 0.5, 0, 0]
SPECTRAL.ZTRANSFORM(x, 1, 0, h)      ' 1.5 + j0
SPECTRAL.ZTRANSFORM(x, -1, 0, h)     ' 0.5 + j0


## STFT / iSTFT

DIM x(15) AS DOUBLE, w(7) AS DOUBLE, st(29) AS DOUBLE, rec(15) AS DOUBLE
' tono a 1 kHz, fs 8 kHz, N = 8 -> bin 1
FOR i = 0 TO 15 : x(i) = MATH.COS(2 * MATH.PI() * i / 8) : NEXT i
FOR i = 0 TO 7  : w(i) = 1 : NEXT i            ' rettangolare (o Hann)
nf = SPECTRAL.STFT(x, 8, w, 4, st)             ' 3 frame, 5 bin
SPECTRAL.ISTFT(st, nf, 8, w, 4, rec)           ' ricostruzione


## Un esempio completo

## Riferimento completo delle funzioni
| funzione | firma | ritorna | significato |
|---|---|---|---|
| `DCT` | `(x(), out_())` | LONG | DCT-II ortonormale; n = lunghezza di `x()` |
| `DST` | `(x(), out_())` | LONG | DST-II ortonormale |
| `IDCT` | `(x(), out_())` | LONG | inversa della DCT-II |
| `DCT2` | `(x(), rows, cols, out_())` | LONG | DCT 2D separabile, row-major |
| `IDCT2` | `(x(), rows, cols, out_())` | LONG | inversa della DCT 2D |
| `HAAR` | `(x(), out_())` | LONG | wavelet di Haar (n potenza di 2) |
| `HILBERT` | `(x(), out_())` | LONG | trasformata di Hilbert (fase -90°) |
| `ZTRANSFORM` | `(x(), re, im, out_())` | LONG | Z-trasformata; `out_(0)=re`, `out_(1)=im` |
| `STFT` | `(x(), N, w(), hop, out_())` | LONG | spettrogramma; ritorna `nf` |
| `ISTFT` | `(X(), nframes, N, w(), hop, out_())` | LONG | ricostruzione; ritorna n |

## Parità e verifica

## Cosa non è qui, e perché
| bisogno | dove |
|---|---|
| filtri IIR/FIR, filtfilt, freqz, decimazione, welch, savgol | `SIGNAL` |
| FFT di un segnale reale o complesso, frequenze | `NUMERICAL.FFT`, `FFTCOMPLEX`, `FFTFREQ` |
| spettro di potenza, periodogramma | `SIGNAL.WELCH` |
| trasformata di Laplace inversa numerica | non esposta: richiede una callback di funzione; si scrive in BASIC con `SIGNAL`/`NUMERICAL` |
| numeri complessi come tipo | `COMPLEX` |

### VEC-Guida-Utente

*Fonte: `VEC-Guida-Utente`*


## A cosa serve

## Modello dei dati

DIM v(2) AS DOUBLE, o(2) AS DOUBLE
v(0)=3 : v(1)=4 : v(2)=0
VEC.VCLAMPMAG(v, 2, o)      ' o = (1.2, 1.6, 0)


## Perché i prefissi `V` e `PERP2D`

## Scomporre un vettore

' v = (3, -2, 1), piano con normale +Y
VEC.PROJECT(v, n, pr)     ' (0, -2, 0)   componente lungo la normale
VEC.REJECT(v, n, tg)      ' (3,  0, 1)   componente tangente


pr(1) = -pr(1) * e                 ' e = coefficiente di restituzione
v(0) = tg(0) + pr(0)
v(1) = tg(1) + pr(1)
v(2) = tg(2) + pr(2)


## Illuminazione

f0 = (0.02, 0.02, 0.02)      ' acqua
u = 1    -> 0.020000
u = 0.5  -> 0.050625
u = 0.1  -> 0.598680


## Limiti
| funzione | uso |
|---|---|
| `VCLAMP(v, lo, hi, o)` | limitare ogni componente in un intervallo |
| `VCLAMP01(v, o)` | colore/normale in `[0,1]` (tonemap, texture coordinate) |
| `VCLAMPMAG(v, max, o)` | limitare il **modulo** lasciando la direzione (velocità massima) |

## Per componente

## Perpendicolare 2D

## Un esempio completo

## Riferimento completo delle funzioni
| funzione | firma | significato |
|---|---|---|
| `PROJECT` | `(v(), on_(), out_())` | proiezione di `v` su `on` |
| `REJECT` | `(v(), on_(), out_())` | componente perpendicolare a `on` |
| `SCHLICK` | `(f0(), u, out_())` | Fresnel di Schlick per componente |
| `BIAS` | `(v(), nrm(), out_())` | `v + nrm·eps` (eps da `\|v\|`) |
| `PERP2D` | `(x, y, out_())` | perpendicolare 2D: `(-y, x)` (2 double) |
| `VCLAMP` | `(v(), lo, hi, out_())` | clamp per componente |
| `VCLAMP01` | `(v(), out_())` | clamp per componente in `[0,1]` |
| `VCLAMPMAG` | `(v(), maxlen, out_())` | limita il modulo |
| `VABS` | `(v(), out_())` | valore assoluto per componente |
| `VSQRT` | `(v(), out_())` | radice per componente |
| `VPOW` | `(v(), p, out_())` | potenza per componente |

## Parità e verifica

## Cosa non è qui, e perché
| bisogno | dove |
|---|---|
| somma, differenza, prodotto scalare/vettoriale, norma, distanze, lerp, min/max | `VECTOR` |
| rotazioni come quaternioni, SLERP | `QUAT` |
| matrici di trasformazione, punto/direzione, inversa affine | `XFORM` |
| riflessione di un raggio su una superficie | `GFX3D.RAYREFLECT` |
| rotazione di un poligono nel piano | `GEO.ROTATE` |

### SPARSE-Guida-Utente

*Fonte: `SPARSE-Guida-Utente`*


## A cosa serve

## Modello dei dati
| formato | contenuto |
|---|---|
| **COO** | triplette `(riga, colonna, valore)` in ordine di inserimento |
| **CSR** | `rowptr(righe+1)`, `colidx(nnz)`, `val(nnz)` — entry contigue per riga |
| **CSC** | `colptr(colonne+1)`, `rowidx(nnz)`, `val(nnz)` — entry contigue per colonna |

IMPORT SPARSE

DIM sz(2) AS LONG
' matrice 3x3 tridiagonale: diagonale 2, fuori diagonale -1
h = SPARSE.COONEW(3, 3, 7)
k = 0
FOR i = 0 TO 2
  SPARSE.COOSET(h, k, i, i, 2) : k = k + 1
NEXT i
FOR i = 1 TO 2
  SPARSE.COOSET(h, k, i, i - 1, -1) : k = k + 1
  SPARSE.COOSET(h, k, i - 1, i, -1) : k = k + 1
NEXT i
hcsr = SPARSE.COOTOCSR(h)
SPARSE.SIZES(hcsr, sz)          ' sz(0)=3, sz(1)=3, sz(2)=7


## Conversioni e ispezione
| funzione | fa |
|---|---|
| `DENSETOCSR(rows, cols, a())` | da una matrice densa row-major, scartando gli zeri |
| `COOTOCSR(h)`, `COOTOCSC(h)` | triplette → CSR / CSC (somma i duplicati, ordina) |
| `CSRTOCSC(h)`, `CSCTOCSR(h)` | cambio di formato, stessa matrice |
| `CSRTRANSPOSE(h)` | CSR → **CSC della trasposta** |
| `CSCTRANSPOSE(h)` | CSC → **CSR della trasposta** |
| `SIZES(h, out())` | `out(0)` righe, `out(1)` colonne, `out(2)` non-zeri |
| `TRIPLET(h, k, out())` | la k-esima entry nello stoccaggio: `(riga, colonna, valore)` |

## Solutori diretti

hcscl = SPARSE.COOTOCSC(hcoo_inferiore)
hf = SPARSE.SPCHOLFACTOR(hcscl)
IF hf > 0 THEN
  rc = SPARSE.SPCHOLSOLVE(hf, b)      ' b viene sovrascritto dalla soluzione
  SPARSE.FREE(hf)
END IF


hcsc = SPARSE.CSRTOCSC(hcsr)
hf = SPARSE.SPLUFACTOR(hcsc)
IF hf > 0 THEN
  rc = SPARSE.SPLUSOLVE(hf, b)
  SPARSE.FREE(hf)
END IF


## Un esempio completo

u = (0.069444, 0.111111, 0.125000, 0.111111, 0.069444)
differenza LU - Cholesky = 0.000000000000
errore rispetto alla soluzione esatta = 0.000000000000


## Riferimento completo delle funzioni
| funzione | firma | ritorna |
|---|---|---|
| `COONEW` | `(rows, cols, nnz)` | handle di triplette (max `nnz`) |
| `COOSET` | `(h, k, row, col, val)` | `0`/`-1` |
| `DENSETOCSR` | `(rows, cols, a())` | handle CSR (zeri scartati) |
| funzione | firma | ritorna |
|---|---|---|
| `COOTOCSR` | `(h)` | nuovo handle CSR |
| `COOTOCSC` | `(h)` | nuovo handle CSC |
| `CSRTOCSC` | `(h)` | nuovo handle CSC |
| `CSCTOCSR` | `(h)` | nuovo handle CSR |
| `CSRTRANSPOSE` | `(h)` | nuovo handle CSC (di `Aᵀ`) |
| `CSCTRANSPOSE` | `(h)` | nuovo handle CSR (di `Aᵀ`) |
| funzione | firma | ritorna |
|---|---|---|
| `SIZES` | `(h, out_())` | `0`; `out_` = righe, colonne, nnz |
| `TRIPLET` | `(h, k, out_())` | `0`; `out_` = riga, colonna, valore |
| funzione | firma | ritorna |
|---|---|---|
| `SPCHOLFACTOR` | `(h_csc)` | handle fattore o `-1` (non SPD) |
| `SPCHOLSOLVE` | `(hf, b())` | `0`; `b()` diventa la soluzione |
| `SPLUFACTOR` | `(h_csc)` | handle fattore o `-1` (singolare) |
| `SPLUSOLVE` | `(hf, b())` | `0`; `b()` diventa la soluzione |
| `FREE` | `(h)` | `0` (qualsiasi contenitore o fattore) |

## Parità e verifica

## Cosa non è qui, e perché
| bisogno | dove |
|---|---|
| matrici dense, determinante, trasposta, slicing | `MATRIX` |
| decomposizioni dense (SVD, QR, autovalori) | `LINALG` |
| sistemi conici/sparsi dei solver (KKT) | `OPTIMIZE` (conic) |
| calcolo scientifico su tensori | `TORCH` |

### QUAD-Guida-Utente

*Fonte: `QUAD-Guida-Utente`*


## A cosa serve
| metodo | quando serve |
|---|---|
| `KRONROD` | caso generale, **con stima d'errore** (G7-K15) |
| `GAUSS` | integrande lisce, n nodi (esatto fino al grado 2n-1) |
| `ROMBERG` | funzione liscia, con tolleranza richiesta dall'utente |
| `TANHSINH` | **singolarità agli estremi** (es. `1/√x`), estremi infiniti |
| `TANHSINHCOMP` | come tanh-sinh, con la distanza dall'estremo (`1/√d`) |
| `LEGENDRENODES`, `HERMITENODES` | nodi e pesi per costruire quadrature proprie |

## Modello dei dati

IMPORT QUAD

FUNCTION Quadrato(x AS DOUBLE) AS DOUBLE
  RETURN x * x
END FUNCTION

SUB Main()
  DIM o(1) AS DOUBLE
  QUAD.KRONROD(Quadrato, 0, 1, o)     ' o(0) = 1/3, o(1) = errore stimato
END SUB


## Metodi

### Gauss-Kronrod (con errore)

ns = 20 : tot = 0 : toterr = 0
FOR k = 0 TO ns - 1
  QUAD.KRONROD(F, k * T / ns, (k + 1) * T / ns, o)
  tot = tot + o(0) : toterr = toterr + o(1)
NEXT k


### Gauss-Legendre e Romberg

QUAD.GAUSS(f, a, b, 10)                    ' 10 nodi
QUAD.ROMBERG(f, a, b, 1e-10, 15)           ' tol assoluta, max 15 livelli


### tanh-sinh (singolarità e infiniti)

FUNCTION Portata(h AS DOUBLE) AS DOUBLE
  IF h <= 0 THEN RETURN 0
  RETURN 1 / MATH.SQR(h)
END FUNCTION

QUAD.TANHSINH(Portata, 0, 1, 1e-10, 2000)          ' ~ 2
QUAD.TANHSINHCOMP(Tensione, 0, 1, 1e-10, 2000)     ' f(x, d): d = distanza dall'estremo


### Nodi e pesi

QUAD.LEGENDRENODES(4, x, w)
acc = 0
FOR i = 0 TO 3 : acc = acc + w(i) * Quadrato(x(i)) : NEXT i   ' 2/3 su [-1,1]


## Un esempio completo

## Riferimento completo delle funzioni
| funzione | firma | ritorna |
|---|---|---|
| `KRONROD` | `(f, a, b, out_())` | `0`; `out_(0)=integrale`, `out_(1)=errore` |
| `GAUSS` | `(f, a, b, n)` | integrale (NaN se `f` è nulla) |
| `ROMBERG` | `(f, a, b, tol, maxit)` | integrale |
| `TANHSINH` | `(f, a, b, absacc, maxeval)` | integrale |
| `TANHSINHCOMP` | `(f2, a, b, absacc, maxeval)` | integrale; `f2(x, d)` |
| `LEGENDRENODES` | `(n, x(), w())` | `n` (o `-1` se gli array sono corti) |
| `HERMITENODES` | `(n, x(), w())` | `n` (o `-1`) |

## Parità e verifica

## Cosa non è qui, e perché
| bisogno | dove |
|---|---|
| Simpson, trapezi, cumulativa | `NUMERICAL.INTEGRATE`, `TRAPEZOID`, `CUMTRAPEZOID` |
| FFT, DCT | `NUMERICAL.FFT`, `SPECTRAL.DCT` |
| ODE e problemi ai limiti | `ODE` |
| integrazione di funzioni di più variabili | si compone: `GAUSS` su ogni asse |

### ODE-Guida-Utente

*Fonte: `ODE-Guida-Utente`*


## A cosa serve

## Modello dei dati

IMPORT ODE

FUNCTION Decay(y() AS DOUBLE, t AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(0) AS DOUBLE
  r(0) = -0.1 * (y(0) - 20)
  RETURN r
END FUNCTION

SUB Main()
  DIM y0(0) AS DOUBLE, yout(0) AS DOUBLE
  y0(0) = 100
  ODE.BDF2(Decay, y0, 0, 60, 600, 30, 0.0000000001, yout)
END SUB


## Metodi per il problema ai valori iniziali
| funzione | ordine | quando |
|---|---|---|
| `ADAMSBASHFORTH4` | 4, esplicito | problema liscio, costo minimo |
| `ADAMSMOULTON4` | 4, implicito | più stabile dell'esplicito |
| `BDF2` | 2, implicito | **problemi stiff** (maxit + tol dell'iterazione interna) |
| `IMPLICITMIDPOINT` | 2, implicito | conservativo su sistemi hamiltoniani lineari |
| `VERLET` | 2, simplettico | meccanica: conserva l'energia a lungo termine |

ODE.ADAMSBASHFORTH4(f, y0, 0, 1, 100, yout)
ODE.BDF2(f, y0, 0, 60, 600, 30, 1e-10, yout)   ' 30 iterazioni di punto fisso
ODE.VERLET(acc, y0, 0, 2*PI, 1000, yout)       ' y = (pos, vel), 2 componenti


## Problemi ai limiti

FUNCTION Trave(x AS DOUBLE, y AS DOUBLE, dy AS DOUBLE) AS DOUBLE
  RETURN -1
END FUNCTION

DIM g(10) AS DOUBLE
ODE.BVPFD(Trave, 0, 1, 0, 0, g)        ' 11 nodi, differenze finite
ODE.BVPSHOOTING(Trave, 0, 1, 0, 0, g)  ' 11 nodi, shooting


## Un esempio completo

## Riferimento completo delle funzioni
| funzione | firma | ritorna |
|---|---|---|
| `ADAMSBASHFORTH4` | `(f, y0(), a, b, steps, yout())` | `0` / `-1` |
| `ADAMSMOULTON4` | `(f, y0(), a, b, steps, yout())` | `0` / `-1` |
| `BDF2` | `(f, y0(), a, b, steps, maxit, tol, yout())` | `0` / `-1` |
| `IMPLICITMIDPOINT` | `(f, y0(), a, b, steps, maxit, tol, yout())` | `0` / `-1` |
| `VERLET` | `(acc, y0(), a, b, steps, yout())` | `0` / `-1` |
| `BVPSHOOTING` | `(fn, a, b, alpha, beta, yout())` | `0` / `-1` |
| `BVPFD` | `(fn, a, b, alpha, beta, yout())` | `0` / `-1` |

## Parità e verifica

## Cosa non è qui, e perché
| bisogno | dove |
|---|---|
| un metodo esplicito semplice (Euler/RK) | `NUMERICAL.SOLVEODE`, `NUMERICAL.SOLVEIVP` |
| integrali e quadrature | `QUAD` |
| sistemi lineari sparsi (per i metodi impliciti scritti a mano) | `SPARSE` |
| PDE (equazioni alle derivate parziali) | si discretizzano in BASIC: `SPARSE` + `ODE` |

### VECCALC-Guida-Utente

*Fonte: `VECCALC-Guida-Utente`*


## A cosa serve

## Modello dei dati
| oggetto | callback BASIC | ritorno |
|---|---|---|
| campo scalare | `f(x, y, z)` | `DOUBLE` |
| campo vettoriale | `F(x, y, z)` | `ARRAY OF DOUBLE` (3) |
| curva | `r(t)` | `ARRAY OF DOUBLE` (3) |
| patch | `S(u, v)` | `ARRAY OF DOUBLE` (3) |

IMPORT VECCALC

' campo scalare: distanza al quadrato
FUNCTION Sph(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS DOUBLE
  RETURN x * x + y * y + z * z
END FUNCTION

' campo vettoriale: rotazione rigida
FUNCTION Swirl(x AS DOUBLE, y AS DOUBLE, z AS DOUBLE) AS ARRAY OF DOUBLE
  DIM r(2) AS DOUBLE
  r(0) = -y : r(1) = x : r(2) = 0
  RETURN r
END FUNCTION

SUB Main()
  DIM g(2) AS DOUBLE


## Cosa calcola
| funzione | campo | risultato |
|---|---|---|
| `GRADIENT(f, x, y, z, h, out)` | scalare | `(∂f/∂x, ∂f/∂y, ∂f/∂z)` in `out(0..2)` |
| `LAPLACIAN(f, x, y, z, h)` | scalare | `∇²f` (stencil a 7 punti) |
| `DIVERGENCE(F, x, y, z, h)` | vettoriale | `∇·F` |
| `CURL(F, x, y, z, h, out)` | vettoriale | `∇×F` in `out(0..2)` |
| `ISSOLENOIDAL(F, x, y, z, h, tol)` | vettoriale | 1 se `|∇·F| ≤ tol` |
| `ISIRROTATIONAL(F, x, y, z, h, tol)` | vettoriale | 1 se `|∇×F| ≤ tol` |
| `ARCLENGTH(r, a, b, tol)` | curva | lunghezza d'arco su `[a,b]` |
| `TNB(r, t, h, out)` | curva | `T, N, B, κ, τ` in `out(0..10)` |
| `LINESCALAR(f, r, a, b, tol)` | scalare + curva | `∫ f(r(t))·|r'(t)| dt` |
| `LINEWORK(F, r, a, b, tol)` | vettoriale + curva | `∫ F(r(t))·r'(t) dt` (lavoro) |
| `FLUX(F, S, u0, u1, v0, v1, tol)` | vettoriale + patch | `∫∫ F(S)·(S_u×S_v)` |

## Le identità che si possono verificare

## Un esempio completo

## Riferimento completo delle funzioni
| funzione | firma | ritorna |
|---|---|---|
| `GRADIENT` | `(f, x, y, z, h, out_())` | `0` / `-1` |
| `LAPLACIAN` | `(f, x, y, z, h)` | DOUBLE |
| `DIVERGENCE` | `(F, x, y, z, h)` | DOUBLE |
| `CURL` | `(F, x, y, z, h, out_())` | `0` / `-1` |
| `ISSOLENOIDAL` | `(F, x, y, z, h, tol)` | `1` / `0` / `-1` |
| `ISIRROTATIONAL` | `(F, x, y, z, h, tol)` | `1` / `0` / `-1` |
| `ARCLENGTH` | `(r, a, b, tol)` | DOUBLE |
| `TNB` | `(r, t, h, out_())` | `0` / `-1` |
| `LINESCALAR` | `(f, r, a, b, tol)` | DOUBLE |
| `LINEWORK` | `(F, r, a, b, tol)` | DOUBLE |
| `FLUX` | `(F, S, u0, u1, v0, v1, tol)` | DOUBLE |

### Parametri ricorrenti
| nome | tipo BASIC | significato |
|---|---|---|
| `f` | `FUNCTION(x, y, z) AS DOUBLE` | campo scalare `f: R³ → R` |
| `F` | `FUNCTION(x, y, z) AS ARRAY OF DOUBLE` | campo vettoriale `F: R³ → R³`, 3 componenti |
| `r` | `FUNCTION(t) AS ARRAY OF DOUBLE` | curva parametrica `r: R → R³`, 3 componenti |
| `S` | `FUNCTION(u, v) AS ARRAY OF DOUBLE` | patch parametrica `S: R² → R³`, 3 componenti |
| `x, y, z` | `DOUBLE` | punto in cui si valuta la derivata |
| `h` | `DOUBLE` | passo delle differenze centrali, **> 0** (tipico `1e-4`) |
| `out_()` | `ARRAY OF DOUBLE` | buffer di uscita fornito dal chiamante |
| `tol` | `DOUBLE` | tolleranza assoluta (confronto o integrazione adattiva) |
| `a, b` | `DOUBLE` | estremi dell'intervallo della curva (`a < b`) |
| `u0, u1, v0, v1` | `DOUBLE` | dominio del parametro della patch |

### Valori di ritorno ed errori

### `VECCALC.GRADIENT(f, x, y, z, h, out_())`

DIM g(2) AS DOUBLE
VECCALC.GRADIENT(Pot, 1, 1, 1, 0.0001, g)   ' (-0.192450, -0.192450, -0.192450)


### `VECCALC.LAPLACIAN(f, x, y, z, h)`

VECCALC.LAPLACIAN(Pot, 1, 1, 1, 0.0001)      ' ~0: 1/r è armonica


### `VECCALC.DIVERGENCE(F, x, y, z, h)`

VECCALC.DIVERGENCE(Elec, 1, 1, 1, 0.0001)    ' ~0 fuori dalla carica


### `VECCALC.CURL(F, x, y, z, h, out_())`

DIM c(2) AS DOUBLE
VECCALC.CURL(Spin, 1, 2, 3, 0.0001, c)       ' (0, 0, 2) per G = (-y, x, 0)


### `VECCALC.ISSOLENOIDAL(F, x, y, z, h, tol)`

VECCALC.ISSOLENOIDAL(Spin, 1, 2, 3, 0.0001, 0.000001)   ' 1
VECCALC.ISSOLENOIDAL(Rad, 1, 2, 3, 0.0001, 0.000001)    ' 0


### `VECCALC.ISIRROTATIONAL(F, x, y, z, h, tol)`

VECCALC.ISIRROTATIONAL(Rad, 1, 2, 3, 0.0001, 0.000001)  ' 1
VECCALC.ISIRROTATIONAL(Spin, 1, 2, 3, 0.0001, 0.000001) ' 0


### `VECCALC.ARCLENGTH(r, a, b, tol)`

VECCALC.ARCLENGTH(Elix, 0, 2 * PIVAL, 0.000001)   ' 6.407617 (elica con a = 1/5)


### `VECCALC.TNB(r, t, h, out_())`
| indici | contenuto |
|---|---|
| 0–2 | tangente `T` |
| 3–5 | normale `N` |
| 6–8 | binormale `B` |
| 9 | curvatura `κ` |
| 10 | torsione `τ` |

### `VECCALC.LINESCALAR(f, r, a, b, tol)`

VECCALC.LINESCALAR(Pot, Ring, 0, 2 * PIVAL, 0.000001)   ' 2*pi (1/r = 1 sul cerchio)


### `VECCALC.LINEWORK(F, r, a, b, tol)`

VECCALC.LINEWORK(Spin, Ring, 0, 2 * PIVAL, 0.000001)    ' 2*pi


### `VECCALC.FLUX(F, S, u0, u1, v0, v1, tol)`

VECCALC.FLUX(Up, Bowl, 0, 1, 0, 1, 0.000001)   ' 1 (campo verticale su superficie piana)


## Parità e verifica

## Cosa non è qui, e perché
| bisogno | dove |
|---|---|
| algebra vettoriale (somma, prodotto, norma, proiezione) | `VECTOR`, `VEC` |
| matrici di trasformazione 3D | `XFORM` |
| integrazione e ODE | `QUAD`, `ODE` |
| differenziazione di una funzione di una variabile | `NUMERICAL.DIFFERENTIATE` |

### Quant-Guida-Utente

*Fonte: `Quant-Guida-Utente`*


## A cosa serve
| file `.inc` | namespace | contenuto |
|---|---|---|
| `trading.inc` | `TRADING_CORE` | valutazione azionaria, CAPM, rendimenti, spread |
| `metrics.inc` | `METRICS` | performance e rischio: Sharpe, Sortino, Calmar, drawdown, VaR/CVaR, alpha/beta, momenti, Jarque-Bera |
| `volatility.inc` | `VOLATILITY` | volatilità annualizzata, downside deviation |
| `fundamentals.inc` | `FUNDAMENTALS` | EPS/DPS/NAV, P/E, P/B, P/S, payout, dividend yield |

## Uso

IMPORT TRADING_CORE
IMPORT METRICS
IMPORT VOLATILITY
IMPORT FUNDAMENTALS

DIM r(4) AS DOUBLE
r(0) = 0.01 : r(1) = -0.02 : r(2) = 0.03 : r(3) = 0.00 : r(4) = 0.015

PRINT METRICS.TotalReturn(r)          ' 0.034786 (frazione: 3.4786%)
PRINT METRICS.SHARPERATIO(r)          ' 5.982583 (mean/std * sqrt(252))
PRINT VOLATILITY.AnnualizedVolatility(r, 252)   ' 0.294856
PRINT FUNDAMENTALS.DividendYield(5, 100)        ' 5 (percentuale)


## Convenzioni

## Da dove vengono (mapping dal vecchio namespace)
| vecchio | nuovo | note |
|---|---|---|
| `TRADING.<12>` | `TRADING_CORE.<12>` | stesso nome e argomenti |
| `TRADING.METRICS.<18>` | `METRICS.<...>` | performance e rischio |
| `TRADING.METRICS.AnnualizedVolatility` | `VOLATILITY.AnnualizedVolatility` | gruppo volatilità |
| `TRADING.METRICS.DownsideDeviation` | `VOLATILITY.DownsideDeviation` | gruppo volatilità |
| `TRADING.METRICS.AnnualizedDownsideDeviation` | `VOLATILITY.AnnualizedDownsideDeviation` | gruppo volatilità |
| `TRADING.METRICS.<11 fondamentali>` | `FUNDAMENTALS.<...>` | EPS, DPS, NAV, P/E, P/B, P/S, payout, dividend yield |
| `TRADING.BONDS` | `BONDS` | ondata 2 |
| `TRADING.OPTIONS` | `DERIVATIVES` | ondata 2 |
| `TRADING.PORTFOLIO` | `PORTFOLIO` | ondata 3 |
| `TRADING.BACKTESTS` | `BACKTEST` | ondata 3 |

## Riferimento: `TRADING_CORE` (12)
| funzione | firma | risultato / note |
|---|---|---|
| `StockValueZeroGrowth` | `(dividend, requiredRate)` | `D / r`; `r = 0 → 0` |
| `StockValueConstGrowth` | `(nextDividend, rate, growth)` | Gordon `D1/(r-g)`; `r ≤ g → 0` |
| `PreferredStockValue` | `(dividend, discountRate)` | `D / r` |
| `CAPM` | `(riskFreeRate, beta, marketReturn)` | `rf + β(rm − rf)` |
| `RiskPremium` | `(assetReturn, riskFreeReturn)` | `r − rf` |
| `TotalStockReturn` | `(initialPrice, endPrice, dividends)` | `((P1−P0)+div)/P0` |
| `CapitalGainsYield` | `(initialPrice, endPrice)` | `(P1−P0)/P0` |
| `HoldingPeriodReturn` | `(returns())` | `∏(1+r) − 1` |
| `GeometricMeanReturn` | `(returns())` | `∏(1+r)^(1/n) − 1` |
| `GrowthRate` | `(retentionRate, roe)` | `retention × ROE` |
| `TaxEquivalentYield` | `(taxFreeYield, taxRate)` | `tfy/(1−tax)`; `tax ∉ [0,1) → 0` |
| `BidAskSpread` | `(bid, ask)` | `ask − bid` |

## Riferimento: `METRICS` (18)
| funzione | firma | risultato / note |
|---|---|---|
| `TotalReturn` | `(returns())` | `∏(1+r) − 1` |
| `AnnualizedReturn` | `(returns(), ppy)` | `(1+TR)^(ppy/n) − 1` |
| `SHARPERATIO` | `(returns())` | `mean/std · √252` (canone 1-arg) |
| `SortinoRatio` | `(returns(), rf, ppy)` | `√ppy · mean(r−rf) / downside(r−rf, 0)` |
| `CalmarRatio` | `(returns(), ppy)` | `AR / |maxDD|` |
| `InformationRatio` | `(returns(), bench(), ppy)` | `√ppy · mean(attivo)/std(attivo)` |
| `TreynorRatio` | `(returns(), bench(), rf, ppy)` | `(AR − AR(rf))/β` |
| `MaxDrawdown` | `(returns())` | minimo drawdown composto (negativo) |
| `ValueAtRisk` | `(returns(), confidence)` | percentile *nearest* (MathNet) al livello `1−conf` |
| `ConditionalValueAtRisk` | `(returns(), confidence)` | media dei rendimenti `≤ VaR` |
| `Beta` | `(returns(), bench())` | `cov/var` campionarie |
| `Alpha` | `(returns(), bench(), rf, ppy)` | Jensen annualizzato |
| `ReturnsSkewness` | `(returns())` | skew campionaria corretta (Fisher) |
| `ReturnsKurtosis` | `(returns())` | curtosi in eccesso non distorta |
| `JarqueBeraTest` | `(returns())` | `ARRAY OF DOUBLE` `[stat, p]`; `n<4 → [0,1]` |
| `AnnualizedTrackingError` | `(returns(), bench(), ppy)` | `√ppy · std(attivo)` |
| `OmegaRatio` | `(returns(), threshold)` | guadagni/perdite; perdite=0 → `+INF` |
| `UlcerIndex` | `(returns())` | `√(media dei drawdown%² composti)` |

## Riferimento: `VOLATILITY` (3)
| funzione | firma | risultato |
|---|---|---|
| `AnnualizedVolatility` | `(returns(), ppy)` | `√ppy · std campionaria` |
| `DownsideDeviation` | `(returns(), threshold)` | `√(media dei (r−thr)² per r<thr)` |
| `AnnualizedDownsideDeviation` | `(returns(), threshold, ppy)` | `√ppy · DownsideDeviation` |

## Riferimento: `FUNDAMENTALS` (11)
| funzione | firma | risultato |
|---|---|---|
| `BookValuePerShare` | `(totalEquity, commonShares)` | `equity/azioni` |
| `DilutedEPS` | `(netIncome, avgShares, otherInstruments)` | `ni/(azioni+strumenti)` |
| `DividendPayoutRatio` | `(dividends, netIncome)` | `div/ni` |
| `DividendYield` | `(dividends, price)` | `div/prezzo × 100` **(percentuale)** |
| `DPS` | `(dividends, numShares)` | `div/azioni` |
| `EPS` | `(netIncome, avgShares)` | `ni/azioni` |
| `EquityMultiplier` | `(totalAssets, equity)` | `attivo/equity` |
| `NAV` | `(fundAssets, fundLiabilities, outstandingShares)` | `(attivo−passivo)/azioni` |
| `PBRatio` | `(price, bookValue)` | `P/B` |
| `PERatio` | `(price, eps)` | `P/E` |
| `PSRatio` | `(price, salesPerShare)` | `P/S` |

## Esempio completo

## Parità e verifica

## Cosa non è (ancora) qui
| bisogno | dove |
|---|---|
| obbligazioni, duration, ZCB | `BONDS` (ondata 2) |
| opzioni, Black-Scholes, greche, binomiale | `DERIVATIVES` (ondata 2) |
| ottimizzazione di portafoglio, Kelly, VaR multi-asset | `PORTFOLIO` (ondata 3) |
| backtest di strategia | `BACKTEST` (ondata 3) |
| calcolo vettoriale, quadratura, ODE | `VECCALC`, `QUAD`, `ODE` |
| algebra vettoriale e matrici | `VECTOR`, `MATRIX`, `LINALG` |

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

### Optimize-Conic-Guida-Utente

*Fonte: `Optimize-Conic-Guida-Utente`*


## A cosa serve

## Modello dei dati
| oggetto | layout |
|---|---|
| `A` (IPM) | `m` righe × `n` colonne, **row-major** |
| `Q` (QP) | `n × n` simmetrica, row-major |
| `A`, `Q` sparse | **CSC 0-based**: `Xptr` ha `n+1` elementi, `Xrow`/`Xval` hanno `nnz` elementi |
| `Cbar`, `Abar`, `Csoc`, `Asoc`, `Cexp`, `Aexp` (SDP) | blocchi **consecutivi**, offset deducibili dalle dimensioni |
| `dims`, `socdims`, `ekind` | un valore per blocco |

DIM ap(3) AS LONG, ar(4) AS LONG, av(4) AS DOUBLE
ap(0)=0 : ap(1)=2 : ap(2)=4 : ap(3)=5     ' colonne 0,1,2 -> offset
ar(0)=0 : ar(1)=1 : ar(2)=0 : ar(3)=1 : ar(4)=0
av(0)=1 : av(1)=1 : av(2)=1 : av(3)=1 : av(4)=1


## Convenzioni

## IPM: LP e QP

' min 2 g1 + 3 g2 + 4 g3  s.t.  g1+g2+g3 = 100, g1+g2 = 60, g >= 0
DIM A(5) AS DOUBLE
A(0)=1 : A(1)=1 : A(2)=1 : A(3)=1 : A(4)=1 : A(5)=0
DIM b(1) AS DOUBLE : b(0)=100 : b(1)=60
DIM c(2) AS DOUBLE : c(0)=2 : c(1)=3 : c(2)=4
DIM g(2) AS DOUBLE, y(1) AS DOUBLE, z(2) AS DOUBLE
PRINT OPTIMIZE.IPMLP(A, 2, 3, b, c, 1E-8, 1E-8, 1E-8, 200, g, y, z)   ' 0
' g = (60, 0, 40), y = (4, -2)


## SOCP
| array | significato |
|---|---|
| `ctype(k)` | `0` = `R+` (una sola riga di `G`), `1` = cono quadratico (`SOC`), `2` = quadratico ruotato |
| `cnm(k)` | dimensione del cono `k` |
| `cmem()` | concatenazione degli indici dei membri dei **soli** coni SOC, in ordine di cono |

' min t  s.t.  g = 30, (t, g) in SOC  ->  t = 30, lam = (1, -1), y = -1
DIM E(1) AS DOUBLE : E(0)=0 : E(1)=1
DIM d(0) AS DOUBLE : d(0)=30
DIM c(1) AS DOUBLE : c(0)=1 : c(1)=0
DIM ctype(0) AS LONG, cnm(0) AS LONG, cmem(1) AS LONG
ctype(0)=1 : cnm(0)=2 : cmem(0)=0 : cmem(1)=1
DIM G(3) AS DOUBLE : G(0)=1 : G(1)=0 : G(2)=0 : G(3)=1
DIM hh(1) AS DOUBLE : hh(0)=0 : hh(1)=0
DIM xs(1) AS DOUBLE, ys(0) AS DOUBLE, lam(1) AS DOUBLE
PRINT OPTIMIZE.SOCPSOLVE(2, 1, E, d, c, 1, ctype, cnm, cmem, G, hh, 1E-8, 1E-8, 200, xs, ys, lam)


## SDP e problema conico unificato

min  c'x + sum_j <C_j, X_j> + sum_i <c_i, z_i>
s.t. A x + sum_j <A_kj, X_j> + sum_i <a_ki, z_i> = b_k
     x >= 0,  X_j >= 0 (PSD),  z_i in cono (SOC o esponenziale/potenza)

| parametro | contenuto |
|---|---|
| `dims(j)` | ordine `d_j` del blocco PSD `j` |
| `Cbar` | matrici `d_j × d_j` consecutive (row-major) |
| `Abar` | per `k = 0..m-1`, per `j = 0..nb-1`: matrice `d_j × d_j` (lunghezza `m · Σ d_j²`) |
| `socdims(i)` | dimensione `k_i` del blocco SOC `i` |
| `Csoc`, `Asoc` | vettori `k_i` consecutivi; `Asoc` per `k`, per `i` (lunghezza `m · Σ k_i`) |
| `ekind(i)` | `0` PEXP, `1` PPOW, `2` RPOW (3 componenti per blocco) |
| `ealpha(i)` | parametro `alpha` del blocco |
| `Cexp`, `Aexp` | 3 valori per blocco; `Aexp` per `k`, per `i` (lunghezza `3 · m · nep`) |

' min X11 + 2 X22  s.t.  tr(X) = 2, X >= 0  ->  X = [[2,0],[0,0]], y = 1
DIM b(0) AS DOUBLE : b(0)=2
DIM dims(0) AS LONG : dims(0)=2
DIM Cbar(3) AS DOUBLE, Abar(3) AS DOUBLE
Cbar(0)=1 : Cbar(1)=0 : Cbar(2)=0 : Cbar(3)=2
Abar(0)=1 : Abar(1)=0 : Abar(2)=0 : Abar(3)=1
DIM Xb(3) AS DOUBLE, Sb(3) AS DOUBLE, y(0) AS DOUBLE, fb(0) AS LONG
DIM dL(0) AS LONG, dD(0) AS DOUBLE, zA(0) AS DOUBLE, zC(0) AS DOUBLE
PRINT OPTIMIZE.SDPSOLVE(1, 0, zA, b, zC, 1, dims, Cbar, Abar, 0, dL, dD, dD, 0, dL, dD, dD, dD, _
      300, 1E-8, 1E-7, 1E-7, 1E-7, 1000, zA, Xb, y, Sb, dD, dD, dD, dD, fb)


## Presolve LP

h = OPTIMIZE.PRESOLVENEW(Aptr, Arow, Aval, b, c, m, n, 1E-9)
IF h > 0 THEN
  PRINT OPTIMIZE.PRESOLVECHANGED(h)          ' 0/1
  DIM info(2) AS LONG
  w = OPTIMIZE.PRESOLVEDIMS(h, info)         ' info = (mr, nr, nnz)
  DIM rp(8) AS LONG, rr(32) AS LONG, rv(32) AS DOUBLE
  DIM rb(8) AS DOUBLE, rc(8) AS DOUBLE
  nr = OPTIMIZE.PRESOLVEREDUCED(h, rp, rr, rv, rb, rc)
  ' ... risolvi il ridotto con IPMLP o un altro solver ...
  w = OPTIMIZE.PRESOLVEPOSTSOLVE(h, xred, yred, xfull, yfull)
  w = OPTIMIZE.PRESOLVEFREE(h)
END IF


## Forma standard

min  1/2 xt' Qs xt + cs' xt     s.t.  As xt = bs,  xt >= 0


DIM lx(2) AS DOUBLE, ux(2) AS DOUBLE      ' 0 <= x <= 70
DIM lc(1) AS DOUBLE, uc(1) AS DOUBLE      ' due uguaglianze: 100 e 60
lc(0)=100 : uc(0)=100 : lc(1)=60 : uc(1)=60
h = OPTIMIZE.STDFORMBUILD(3, 2, c_int, dqL, dqL, dqD, 0, lx, ux, lc, uc, colptr, srow, val)


## Riferimento completo delle funzioni
| `EXPCONEBARRIER` | `(kind AS LONG, alpha AS DOUBLE, x() AS DOUBLE, f() AS DOUBLE)` | LONG | kind: 0=PEXP, 1=PPOW, 2=RPOW. Vettori dei membri: x()/s()/z() = 3. |
| `EXPCONEGRAD` | `(kind AS LONG, alpha AS DOUBLE, x() AS DOUBLE, g() AS DOUBLE)` | LONG | g() = gradiente (3). |
| `EXPCONEHESS` | `(kind AS LONG, alpha AS DOUBLE, x() AS DOUBLE, H() AS DOUBLE)` | LONG | H() = Hessiana 3x3 row-major (9). |
| `EXPCONEDUALIN` | `(kind AS LONG, alpha AS DOUBLE, s() AS DOUBLE)` | LONG | 1 se s e' strettamente interno al cono duale K*. |
| `EXPCONEDUALPOINT` | `(kind AS LONG, alpha AS DOUBLE, s() AS DOUBLE, W() AS DOUBLE)` | LONG | W() = punto di scaling (3). |
| `EXPCONENTMETRICS` | `(kind AS LONG, alpha AS DOUBLE, z() AS DOUBLE, s() AS DOUBLE, m() AS DOUBLE)` | LONG | m() = 7 metriche (GAP, CONDZ, CONDS, REL, MZ, MS, WN). |
| `EXPCONESCALING` | `(kind AS LONG, alpha AS DOUBLE, z() AS DOUBLE, s() AS DOUBLE, smu AS DOUBLE, out() AS DOUBLE)` | LONG | out() = Theta[9] | Thin[9] | rt[9] | scale (28). Ritorna il ramo (1 NT, 0 Hessiana, -1 errore). |
| `IPMLP` | `(A() AS DOUBLE, m AS LONG, n AS LONG, b() AS DOUBLE, c() AS DOUBLE, tolGap AS DOUBLE, tolPfeas AS DOUBLE, tolDfeas AS DOUBLE, maxIter AS LONG, x() AS DOUBLE, y() AS DOUBLE, z() AS DOUBLE)` | LONG | A = m x n row-major. Uscite: x (n), y (m), z (n) — le prime due sono i primali, |
| `IPMLPWARM` | `(A() AS DOUBLE, m AS LONG, n AS LONG, b() AS DOUBLE, c() AS DOUBLE, tolGap AS DOUBLE, tolPfeas AS DOUBLE, tolDfeas AS DOUBLE, maxIter AS LONG, x() AS DOUBLE, y() AS DOUBLE, z() AS DOUBLE, x0() AS DOUBLE, y0() AS DOUBLE)` | LONG | — |
| `IPMQP` | `(A() AS DOUBLE, Q() AS DOUBLE, m AS LONG, n AS LONG, b() AS DOUBLE, c() AS DOUBLE, tolGap AS DOUBLE, tolPfeas AS DOUBLE, tolDfeas AS DOUBLE, maxIter AS LONG, x() AS DOUBLE, y() AS DOUBLE, z() AS DOUBLE)` | LONG | — |
| `IPMQPWARM` | `(A() AS DOUBLE, Q() AS DOUBLE, m AS LONG, n AS LONG, b() AS DOUBLE, c() AS DOUBLE, tolGap AS DOUBLE, tolPfeas AS DOUBLE, tolDfeas AS DOUBLE, maxIter AS LONG, x() AS DOUBLE, y() AS DOUBLE, z() AS DOUBLE, x0() AS DOUBLE, y0() AS DOUBLE)` | LONG | — |
| `IPMLPCSC` | `(Aptr() AS LONG, Arow() AS LONG, Aval() AS DOUBLE, m AS LONG, n AS LONG, b() AS DOUBLE, c() AS DOUBLE, tolGap AS DOUBLE, tolPfeas AS DOUBLE, tolDfeas AS DOUBLE, maxIter AS LONG, x() AS DOUBLE, y() AS DOUBLE, z() AS DOUBLE)` | LONG | Varianti sparse: A in CSC (Aptr n+1, Arow/ Aval per gli nnz, 0-based); |
| `IPMQPCSC` | `(Aptr() AS LONG, Arow() AS LONG, Aval() AS DOUBLE, Qptr() AS LONG, Qrow() AS LONG, Qval() AS DOUBLE, m AS LONG, n AS LONG, b() AS DOUBLE, c() AS DOUBLE, tolGap AS DOUBLE, tolPfeas AS DOUBLE, tolDfeas AS DOUBLE, maxIter AS LONG, x() AS DOUBLE, y() AS DOUBLE, z() AS DOUBLE)` | LONG | — |
| `SOCPSOLVE` | `(n AS LONG, p AS LONG, E() AS DOUBLE, d() AS DOUBLE, c() AS DOUBLE, ncones AS LONG, ctype() AS LONG, cnm() AS LONG, cmem() AS LONG, G() AS DOUBLE, h() AS DOUBLE, tolGap AS DOUBLE, tolFeas AS DOUBLE, maxIter AS LONG, x() AS DOUBLE, y() AS DOUBLE, lam() AS DOUBLE)` | LONG | E = p x n row-major, d (p), c (n). G = K x n row-major con K = somma delle |
| `SOCPSOLVESP` | `(n AS LONG, p AS LONG, E() AS DOUBLE, d() AS DOUBLE, c() AS DOUBLE, ncones AS LONG, ctype() AS LONG, cnm() AS LONG, cmem() AS LONG, G() AS DOUBLE, h() AS DOUBLE, tolGap AS DOUBLE, tolFeas AS DOUBLE, maxIter AS LONG, x() AS DOUBLE, y() AS DOUBLE, lam() AS DOUBLE)` | LONG | — |
| `SDPSOLVE` | `(m AS LONG, n AS LONG, A() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE, nb AS LONG, dims() AS LONG, Cbar() AS DOUBLE, Abar() AS DOUBLE, nsoc AS LONG, socdims() AS LONG, Csoc() AS DOUBLE, Asoc() AS DOUBLE, nep AS LONG, ekind() AS LONG, ealpha() AS DOUBLE, Cexp() AS DOUBLE, Aexp() AS DOUBLE, maxIter AS LONG, tol AS DOUBLE, rtolPri AS DOUBLE, rtolDual AS DOUBLE, rtolGap AS DOUBLE, nearRel AS DOUBLE, x() AS DOUBLE, Xbar() AS DOUBLE, y() AS DOUBLE, Sbar() AS DOUBLE, Zsoc() AS DOUBLE, Ssoc() AS DOUBLE, Zexp() AS DOUBLE, Sexp() AS DOUBLE, fb() AS LONG)` | LONG | min c'x + <C,X> + <c_i,z_i>  s.t.  A x + <A_kj,X_j> + <a_ki,z_i> = b_k, |
| `PRESOLVENEW` | `(Aptr() AS LONG, Arow() AS LONG, Aval() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE, m AS LONG, n AS LONG, tol AS DOUBLE)` | LONG | PRESOLVENEW riduce min c'x, A x = b, x >= 0 (A in CSC, 0-based) e ritorna un |
| `PRESOLVECHANGED` | `(h AS LONG)` | LONG | — |
| `PRESOLVEDIMS` | `(h AS LONG, out_() AS LONG)` | LONG | out(0)=mr, out(1)=nr, out(2)=nnz(Ar). |
| `PRESOLVEREDUCED` | `(h AS LONG, Arptr() AS LONG, Arrow() AS LONG, Arval() AS DOUBLE, br() AS DOUBLE, cr() AS DOUBLE)` | LONG | Copia il ridotto in CSC: Arptr (nr+1), Arrow/Arval (nnz), br (mr), cr (nr). |
| `PRESOLVEPOSTSOLVE` | `(h AS LONG, xred() AS DOUBLE, yred() AS DOUBLE, xfull() AS DOUBLE, yfull() AS DOUBLE)` | LONG | Riporta la soluzione ridotta (xred nr, yred mr) nello spazio originale. |
| `PRESOLVEPOSTSOLVEDIR` | `(h AS LONG, rred() AS DOUBLE, yred() AS DOUBLE, rfull() AS DOUBLE, yfull() AS DOUBLE)` | LONG | Come sopra, per una direzione (raggio di Farkas) omogenea. |
| `PRESOLVEFREE` | `(h AS LONG)` | LONG | — |
| `STDFORMBUILD` | `(nvar AS LONG, ncon AS LONG, c_int() AS DOUBLE, qi() AS LONG, qj() AS LONG, qv() AS DOUBLE, nq AS LONG, lx() AS DOUBLE, ux() AS DOUBLE, lc() AS DOUBLE, uc() AS DOUBLE, colptr() AS LONG, srow() AS LONG, val() AS DOUBLE)` | LONG | STDFORMBUILD prende l'LP/QP utente (vincoli EQ/LO/UP/free, bounds, Q come |
| `STDFORMINFO` | `(h AS LONG, out_() AS DOUBLE)` | LONG | out(0)=m, out(1)=n, out(2)=nvar, out(3)=ncon, out(4)=cfix. |
| `STDFORMGETA` | `(h AS LONG, A_() AS DOUBLE)` | LONG | — |
| `STDFORMGETB` | `(h AS LONG, b_() AS DOUBLE)` | LONG | — |
| `STDFORMGETC` | `(h AS LONG, c_() AS DOUBLE)` | LONG | — |
| `STDFORMGETQ` | `(h AS LONG, Q_() AS DOUBLE)` | LONG | Q densa n x n; ritorna -1 se il problema e' un LP (nessuna Q). |
| `STDFORMMAPX` | `(h AS LONG, xt() AS DOUBLE, x_() AS DOUBLE)` | LONG | — |
| `STDFORMMAPDIR` | `(h AS LONG, dxt() AS DOUBLE, dxt_out() AS DOUBLE)` | LONG | — |
| `STDFORMMAPY` | `(h AS LONG, ystd() AS DOUBLE, y_() AS DOUBLE)` | LONG | — |
| `STDFORMFREE` | `(h AS LONG)` | LONG | — |

## Parità e verifica

## Cosa non è qui, e perché
| bisogno | dove |
|---|---|
| simplesso su tableau, LP con `<=`, analisi fine (duali, costi ridotti, Gomory) | `optimize.inc` / `mathprog.inc` |
| euristiche e MIP classici (packing, routing, scheduling) | `ortools.inc` (106 funzioni) |
| minimi quadrati, BFGS, Nelder-Mead, ricerca globale | `optimize.inc` |
| KMeans, PCA, SVM, regressione | `MULTIVARIATE` / `STATSX` |
| algebra lineare densa/sparsa | `LINALG` / `MATRIX` |

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

## 9. `scheduling` — cammini con finestre temporali

### `SCHEDPATHSTIMEWINDOWS` — percorsi che rispettano le finestre

DECLARE FUNCTION SCHEDPATHSTIMEWINDOWS LIB "gmbscheduling" ALIAS "ofx_sched_paths_time_window" (
    adj() AS LONG, windows() AS DOUBLE, n AS LONG, s AS LONG, t AS LONG,
    paths() AS LONG, lens() AS LONG, maxPaths AS LONG) AS LONG

| Parametro | Tipo | Significato |
|---|---|---|
| `adj()` | `LONG(n*n)` | Matrice di adiacenza row-major: `1` = arco `u -> v`. |
| `windows()` | `DOUBLE(2*n+1)` | `start(0..n-1) \| end(0..n-1) \| minConnect`: finestra di ogni attività e tempo minimo di connessione (setup/pulizia) fra due attività consecutive. |
| `n` | `LONG` | Numero di attività. |
| `s`, `t` | `LONG` | Attività iniziale e finale. |
| `paths()` | `LONG(maxPaths*n)` | **Uscita**: i cammini, uno per riga (`paths(k*n + j)` = j-esimo nodo del cammino k). |
| `lens()` | `LONG(maxPaths)` | **Uscita**: numero di nodi di ciascun cammino. |
| `maxPaths` | `LONG` | Quanti cammini restituire (i primi trovati in DFS). |

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
| `TSPERX` corrompeva l'heap (SIGBUS nella backend nativa) | **corretto**: guardie sugli estremi in `erx_add` e sui nodi invalidi; ora è nel MiniTest (`routing2.bas`) |
| `SUBSETSUM` restituiva l'esito giusto ma `take()` non ricostruiva la combinazione | **corretto**: ricostruzione con tabella per prefisso; il MiniTest verifica `Σ items·take = target` |

## 6. Note pratiche

## 7. Riferimenti

### QUAT-Guida-Utente

*Fonte: `QUAT-Guida-Utente`*


## A cosa serve

## Modello dei dati

TYPE Quat
  X AS DOUBLE
  Y AS DOUBLE
  Z AS DOUBLE
  W AS DOUBLE
END TYPE

DIM q AS Quat
q = QUAT.FROMAXISANGLE(0, 0, 1, 90 * 0.017453292519943295)   ' 90° attorno a +Z
PRINT q.X; " "; q.Y; " "; q.Z; " "; q.W                       ' 0 0 0.707107 0.707107


## Convenzioni da tenere presenti
| aspetto | convenzione |
|---|---|
| ordine dei campi | `X, Y, Z, W` — vettore prima, scalare dopo |
| rotazioni | il quaternione deve essere **normalizzato** (`NORMALIZE`, o gli esempi `FROM*`) |
| angoli | **radiante** (usare `D2R` o `MATH.DEG2RAD`) |
| matrici 3×3 | **row-major**: `m(0..2)` prima riga, `m(3..5)` seconda, `m(6..8)` terza |
| composizione | `MUL(a, b)` applica **prima `b`, poi `a`** (prodotto di Hamilton) |
| verso di rotazione | antiorario guardando l'asse dalla punta verso l'origine (regola della mano destra) |
| `FROMDIR` | porta **+Z** sulla direzione indicata (convenzione camera) |
| `FROMTWOVECTORS` | porta il primo vettore sul secondo, qualunque coppia |
| interpolazione | `t` in `[0, 1]`; `SLERP` sceglie l'emisfero corto |

## Costruzione di una rotazione

' 1) angolo attorno a un asse (il caso più comune)
yaw = QUAT.FROMAXISANGLE(0, 0, 1, MATH.DEG2RAD(90))

' 2) direzione da puntare: la camera guarda +Z, il bersaglio è (1,1,1)
look = QUAT.FROMDIR(1, 1, 1)

' 3) da un vettore a un altro
q = QUAT.FROMTWOVECTORS(1, 0, 0, 0, 1, 0)     ' +X su +Y = quarto di giro attorno a Z

' 4) quattro componenti già note (es. lette da un file o da una rete)
q = QUAT.NORMALIZE(QUAT.MAKE(0.5, -0.5, 0.5, 0.5))


## Algebra

## Interpolazione

a = QUAT.FROMAXISANGLE(0, 0, 1, 0)                 ' identità
b = QUAT.FROMAXISANGLE(0, 0, 1, MATH.DEG2RAD(90))
FOR i = 0 TO 4
  q = QUAT.SLERP(a, b, i / 4)
  QUAT.ROTATEVEC3(q, 1, 0, 0, v)                   ' il muso descrive l'arco
NEXT i


## Matrice e applicazione a un vettore

DIM m(8) AS DOUBLE, v(2) AS DOUBLE
q = QUAT.FROMAXISANGLE(0, 0, 1, MATH.DEG2RAD(90))
QUAT.TOMAT3(q, m)          ' riga 0 = 0 -1 0, riga 1 = 1 0 0, riga 2 = 0 0 1
QUAT.ROTATEVEC3(q, 1, 0, 0, v)
PRINT v(0); " "; v(1); " "; v(2)   ' 0  1  0


## Errori e casi limite
| situazione | cosa succede |
|---|---|
| `INVERSE` di un quaternione nullo | `st(0) = -1`, il valore restituito è `(0,0,0,0)` |
| `NORMALIZE` di un quaternione nullo | restituisce l'identità `(0,0,0,1)` (scelta della libreria C) |
| `TOMAT3` con `m()` più corto di 9 | ritorna `-1`, non scrive nulla |
| `ROTATEVEC3` con `out_()` più corto di 3 | ritorna `-1`, non scrive nulla |
| `FROMDIR(0,0,0)` | direzione non definita: il risultato non è significativo |
| `SLERP` fra quaternioni opposti (`dot ≈ -1`) | il percorso non è unico; `SLERP` usa l'emisfero corto |
| rotazione con quaternione non normalizzato | la matrice risultante non è una rotazione (può contenere scala) |

## Riferimento completo delle funzioni
| funzione | firma | ritorna | significato |
|---|---|---|---|
| `IDENTITY` | `()` | Quat | `(0,0,0,1)` |
| `MAKE` | `(x, y, z, w AS DOUBLE)` | Quat | quaternione esplicito, **non** normalizzato |
| `FROMDIR` | `(x, y, z AS DOUBLE)` | Quat | porta +Z sulla direzione indicata |
| `FROMAXISANGLE` | `(ax, ay, az, radians AS DOUBLE)` | Quat | rotazione di `radians` attorno all'asse |
| `FROMTWOVECTORS` | `(fx, fy, fz, tx, ty, tz AS DOUBLE)` | Quat | rotazione che porta il primo vettore sul secondo |
| `FROMMAT3` | `(m() AS DOUBLE)` | Quat | quaternione della matrice 3×3 row-major |
| funzione | firma | ritorna | significato |
|---|---|---|---|
| `ADD` | `(a, b AS Quat)` | Quat | somma componente per componente |
| `SCALE` | `(a AS Quat, s AS DOUBLE)` | Quat | moltiplicazione per uno scalare |
| `NEG` | `(a AS Quat)` | Quat | opposto (stessa rotazione) |
| `MUL` | `(a, b AS Quat)` | Quat | prodotto di Hamilton: prima `b`, poi `a` |
| `CONJUGATE` | `(a AS Quat)` | Quat | coniugato `(-x,-y,-z,w)` |
| `DOT` | `(a, b AS Quat)` | DOUBLE | prodotto scalare a 4 componenti |
| `LENGTH` | `(a AS Quat)` | DOUBLE | norma |
| `NORMALIZE` | `(a AS Quat)` | Quat | porta la norma a 1 (nullo → identità) |
| `INVERSE` | `(a AS Quat, st() AS LONG)` | Quat | inversa; `st(0)`: 0 = ok, -1 = nullo |
| funzione | firma | ritorna | significato |
|---|---|---|---|
| `LERP` | `(a, b AS Quat, t AS DOUBLE)` | Quat | lineare + rinormalizzazione |
| `SLERP` | `(a, b AS Quat, t AS DOUBLE)` | Quat | sferica, emisfero corto |
| funzione | firma | ritorna | significato |
|---|---|---|---|
| `TOMAT3` | `(a AS Quat, m() AS DOUBLE)` | LONG | matrice 3×3 row-major (0 = ok, -1 = array corto) |
| `ROTATEVEC3` | `(a AS Quat, x, y, z AS DOUBLE, out_() AS DOUBLE)` | LONG | ruota il vettore, 3 double in `out_()` |

## Un esempio completo

## Parità e verifica

## Cosa non è qui, e perché
| bisogno | dove |
|---|---|
| rotazione di un poligono nel piano | `GEO.ROTATE` (gradi, attorno all'origine) |
| vettori 2D/3D elementari (somma, prodotto vettoriale, norma) | `VECTOR` |
| matrici, decomposizioni, esponenziale di matrice | `MATRIX`, `LINALG` |
| trasformazioni di scena e proiezione prospettica | `GFX3D` (motore grafico) |
| assetto da misure (accelerometro/giroscopio, filtro) | si compone qui: `normalize` dell'integrazione + `SLERP` di correzione |

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

## 4. Integrali e funzioni ellittiche, Bessel sferiche (2026-11-07)
| gruppo | funzioni |
|---|---|
| Carlson | `ELLIPTICRF(x,y,z)`, `ELLIPTICRD(x,y,z)` |
| integrali completi | `ELLIPTICK(k)`, `ELLIPTICE(k)` |
| integrali incompleti | `ELLIPTICF(phi,k)`, `ELLIPTICEINC(phi,k)` |
| Jacobi | `JACOBIAM(u,k)`, `JACOBISN(u,k)`, `JACOBICN(u,k)`, `JACOBIDN(u,k)` |
| nome | `ELLIPTICNOME(k)` |
| Bessel sferiche | `BESSELSPHJ(n,x)`, `BESSELSPHY(n,x)` |

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
