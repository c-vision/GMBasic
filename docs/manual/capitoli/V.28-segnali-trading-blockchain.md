# V.28 — Segnali, trading e blockchain

## Cosa ottieni da questo capitolo

Il passaggio dai numeri alle decisioni: indicatori su serie temporali, metriche di rischio di un
portafoglio, e la struttura di una blockchain. È il capitolo in cui un errore non dà un `NaN`: dà
una strategia che sembra funzionare.

## 1. Serie temporali

Una serie temporale è una sequenza di valori con un ordine che conta. `TIMESERIES` offre medie
mobili, variazioni, allineamento e finestre: il sample `TimeSeriesTests.bas` è la suite,
`TimeSeriesAdvanced.bas` i casi d'uso, `TimeSeries-Guida-Utente` la reference.

La regola: una finestra mobile non deve **mai** guardare il futuro. Un indicatore calcolato con
dati successivi produce un backtest perfetto e un trading disastroso.

## 2. Segnali

`SIGNAL` costruisce indicatori e regole sopra una serie. Eseguito
(`shared/samples/Signals/SignalDemo.bas`, backend C99Native):

```
[iv] AAPL iv_rank_high =  85
[unusual] TSLA =  12
done
```

Due segnali: `iv_rank_high` (volatilità implicita in zona alta) e `unusual` (movimento anomalo).
L'output è una **lista di eventi**, non un consiglio: il segnale dice "questo valore è fuori
norma", la decisione resta al codice che lo consuma.

## 3. Metriche di portafoglio

Eseguito (`shared/samples/Trading/TradingMetrics.bas`):

```
--- Rendimento ---
Total Return:       31.78%
Annualized Return:  31.78%

--- Rischio ---
Annualized Volat.:  9.67%
Max Drawdown:       -1.37%
Value At Risk (95%):-0.80%
Downside Deviation: 1.12%

--- Risk Adjusted ---
Sortino Ratio:      3.23
Beta (vs Bench):    0.16

--- Statistica ---
Jarque-Bera (Stat): 0.73
Jarque-Bera (p-Val):0.70

--- Additional Metrics ---
TM.BookValuePerShare(1000,100)= 10
TM.EquityMultiplier(2000,500)= 4
```

| Metrica | Cosa dice | Come si legge |
|---|---|---|
| Total Return | guadagno complessivo | 31.78% sull'intero periodo |
| Annualized Volatility | oscillazione annua | 9.67%: bassa |
| Max Drawdown | peggior perdita dal picco | -1.37%: contenuta |
| VaR 95% | perdita che non superi nel 95% dei giorni | -0.80% |
| Sortino | rendimento per unità di rischio **negativo** | 3.23: alto |
| Beta | sensibilità al benchmark | 0.16: quasi indipendente |

Il Sortino alto con beta basso racconta una strategia poco correlata al mercato, non
necessariamente una strategia buona: con 31% di rendimento annuo e 9% di volatilità il numero è
plausibile, ma un campione corto basta a renderlo casuale. **Nessuna di queste metriche dice se la
strategia funzionerà**: dicono com'è andata su quei dati.

## 4. Backtest e opzioni

- `Trading/TradingBacktest.bas` — il ciclo di backtest;
- `Trading/TradingPortfolioCalculations.bas`, `TradingPortfolioAdvanced.bas` — posizioni e pesi;
- `Trading/TradingOptionsAndBonds.bas` — opzioni e obbligazioni (guida `Trading-Bonds-Guida-Utente`);
- `Trading/TradingEquityMetrics.bas` — metriche di capitale.

La trappola del backtest è sempre la stessa: costi di transazione, slippage e look-ahead. Se il
modello non li ha, il risultato è una simulazione di un mondo che non esiste.

## 5. Blockchain

`BLOCKCHAIN` copre hash a catena, blocchi, transazioni e Merkle tree; i sample
`Crypto/CryptoBlockchain.bas` e `Crypto/BlockchainFullCoverage.bas` mostrano la struttura,
`Crypto/CryptoWalletHD.bas` le chiavi derivate, `Crypto/CryptoERC20.bas` l'interazione con un
token. Guida: `Blockchain-Guida-Utente`.

Il punto che vale la pena capire: una blockchain non rende i dati veri, li rende **immutabili e
verificabili**. Se l'input è sbagliato, resta sbagliato per sempre.

## 6. Cosa non è coperto

Nessun sample eseguito in questo capitolo parla con un exchange o con una chain reale: le
chiamate di rete sono nei sample HTTP/WebSocket, e in un libro non avrebbe senso dipendere da un
servizio esterno che cambia. Le formule, invece, sono deterministiche e verificabili — ed è
quella la parte che si sbaglia.

## 7. Errori frequenti

| Sintomo | Causa | Correzione |
|---|---|---|
| backtest troppo bello | look-ahead nella finestra | sposta la finestra indietro |
| Sortino/Sharpe "infinito" | deviazione zero su pochi campioni | allunga il campione o gestisci il caso |
| VaR che non torna | orizzonte o confidenza diversi | dichiara sempre orizzonte e confidenza |
| segnale che scatta sempre | soglia non normalizzata | normalizza sulla volatilità |
| drawdown positivo | segno invertito nella formula | il drawdown è negativo o zero |

## 8. Dove andare adesso

- **V.27** — la statistica che sta sotto queste metriche.
- **IV.25** — come arriva un flusso di prezzi.
- **V.32** — quando la decisione la prende un modello.














<!-- BEGIN SURFACE (generato da tools/sh/manual_surface.sh — non modificare a mano) -->

## Riferimento completo: comandi, funzioni, parametri

> Sezione generata dalle guide del progetto per questo capitolo: contiene **tutte**
> le superfici documentate, non una selezione. La prosa dei capitoli precedenti le
> spiega; qui stanno i dettagli — parametri, varianti, esempi d'uso.

### Signal-Manual

*Fonte: `Signal-Manual`*


## 1. Riferimento funzioni
| NOME | SINTASSI / ESEMPIO | DESCRIZIONE |
|---|---|---|
| `CONVOLVE` | `SIGNAL.CONVOLVE(x, k)` | Convoluzione 1D (mode full). |
| `CORRELATE` | `SIGNAL.CORRELATE(x, k)` | Correlazione 1D (mode full). |
| `DECIMATE` | `SIGNAL.DECIMATE(x, q)` | Downsampling (FIR + decimazione). |
| `RESAMPLE` | `SIGNAL.RESAMPLE(x, num)` | Ricampionamento via FFT. |
| `SAVGOLFILTER` | `SIGNAL.SAVGOLFILTER(x, w, p)` | Smoothing Savitzky-Golay. |
| `WELCH` | `SIGNAL.WELCH(x, nperseg, fs)` | Densità spettrale (Welch). |
| `FINDPEAKS` | `SIGNAL.FINDPEAKS(x, dist)` | Picchi locali con distanza minima. |
| `FIRWIN` | `SIGNAL.FIRWIN(n, fc, btype, win)` | Design FIR a finestra (LP/HP/BP/BS). |
| `HANN` | `SIGNAL.HANN(4)` | Finestra di Hann simmetrica. |
| `HAMMING` | `SIGNAL.HAMMING(4)` | Finestra di Hamming simmetrica. |
| `BLACKMAN` | `SIGNAL.BLACKMAN(4)` | Finestra di Blackman simmetrica. |
| `BUTTER` | `SIGNAL.BUTTER(n, wn, btype)` | Design Butterworth (handle). |
| `CHEBY1` | `SIGNAL.CHEBY1(n, rp, wn, btype)` | Design Chebyshev I (handle). |
| `CHEBY2` | `SIGNAL.CHEBY2(n, rs, wn, btype)` | Design Chebyshev II (handle). |
| `ELLIP` | `SIGNAL.ELLIP(n, rp, rs, wn, btype)` | Design ellittico (handle). |
| `FILTFILT` | `SIGNAL.FILTFILT(h, x)` | Filtro zero-phase (filtfilt). |
| `FREQZ` | `SIGNAL.FREQZ(h, n)` | Risposta in ampiezza |H|. |
| `LFILTER` | `SIGNAL.LFILTER(h, x)` | Applica filtro IIR causale. |

## 2. Esempio completo

' SIGNAL — scipy.signal nucleo (convolve/correlate 1D + finestre)
' Input: x=[1,2,3], k=[0,1,0.5]
' Oracolo: convolve=[0,1,2.5,4,1.5], correlate=[0.5,2,3.5,3,0],
'          hann(4)=[0,0.75,0.75,0], hamming(4)=[0.08,0.77,0.77,0.08],
'          blackman(4)=[0,0.63,0.63,0]
IMPORT SIGNAL

SUB Main()
  DIM x(2) AS DOUBLE = {1, 2, 3}
  DIM k(2) AS DOUBLE = {0, 1, 0.5}
  DIM c() = SIGNAL.CONVOLVE(x, k)
  DIM r() = SIGNAL.CORRELATE(x, k)
  DIM h() = SIGNAL.HANN(4)
  DIM m() = SIGNAL.HAMMING(4)
  DIM b() = SIGNAL.BLACKMAN(4)
  PRINT "convolve   ="; c(0); c(1); c(2); c(3); c(4)


## 3. Note e limitazioni

### Blockchain-Guida-Utente

*Fonte: `Blockchain-Guida-Utente`*


## 1. Hashing

IMPORT BLOCKCHAIN

PRINT BLOCKCHAIN.STRKECCAK256("hello")     ' hash Keccak-256 (hex)
PRINT BLOCKCHAIN.STRRIPEMD160("hello")     ' hash RIPEMD-160 (hex)
DIM b() = FROMHEX("68656c6c6f")
PRINT TOHEX(BLOCKCHAIN.KECCAK256(b()))     ' Keccak-256 su byte[] → byte[]
PRINT TOHEX(BLOCKCHAIN.RIPEMD160(b()))     ' RIPEMD-160 su byte[] → byte[]
PRINT TOHEX(BLOCKCHAIN.BLAKE2B256(b()))    ' BLAKE2b-256 su byte[] → byte[]


## 2. Codifiche

PRINT BLOCKCHAIN.BASE32ENCODE("data")
PRINT BLOCKCHAIN.BASE32DECODE("ORSXG5A=")
PRINT BLOCKCHAIN.BASE58ENCODE("data")
PRINT BLOCKCHAIN.BASE58DECODE("...")
PRINT BLOCKCHAIN.RLPENCODE(data)           ' RLP (Ethereum)
PRINT BLOCKCHAIN.VARINTENCODE(150)         ' varint
PRINT BLOCKCHAIN.VARINTDECODE("...")


## 3. ABI (Ethereum)

PRINT BLOCKCHAIN.ABIENCODE(signature$, args)  ' codifica ABI
PRINT BLOCKCHAIN.ABIDECODE(data$, types$)      ' decodifica ABI
PRINT BLOCKCHAIN.IERC20(...)                   ' helper interfaccia ERC-20
PRINT BLOCKCHAIN.CONTRACT(...)                 ' descrizione contratto


## 4. Chiavi, indirizzi, wallet

PRINT BLOCKCHAIN.GENERATEMNEMONIC()        ' frase mnemonica BIP-39
PRINT BLOCKCHAIN.MNEMONICTOSEED(phrase$)   ' seed dal mnemonico
PRINT BLOCKCHAIN.DERIVEKEY(seed, path$)    ' chiave derivata (BIP-32/44)
PRINT BLOCKCHAIN.GETPUBLICKEY(privateKey)  ' chiave pubblica
PRINT BLOCKCHAIN.BTCADDRFROMPUB(pubKey)    ' indirizzo Bitcoin
PRINT BLOCKCHAIN.ETHADDRFROMPUB(pubKey)    ' indirizzo Ethereum
PRINT BLOCKCHAIN.ETHADDRCHECKSUM(addr$)    ' checksum EIP-55
PRINT BLOCKCHAIN.BLOCKDIFFICULTY(header, bits)  ' difficoltà (target/hash)
PRINT BLOCKCHAIN.SECP256K1SIGN(msg, priv)  ' firma ECDSA (DER)


## 5. Verifiche sui backend (2026-09-12)

### Signals-Guida-Utente

*Fonte: `Signals-Guida-Utente`*


## Comandi

### `SIGNAL SUBSCRIBE [TYPES("t1","t2")] [ASSET("...")] [MARKET("...")] [SYMBOLS("s1","s2")] [TIMEFRAME("...")] AS "id"`

### `ON SIGNAL "id" CALL <proc>(id, type, symbol, value[, tf])`

### `SIGNAL EMIT "type", "symbol", value [ASSET("...")] [MARKET("...")] [TIMEFRAME("...")]`

### `SIGNAL UNSUBSCRIBE ["id"]` — rimuove una sottoscrizione (o tutte se omesso).

### `SIGNAL PING` — keepalive (no-op sul bus locale).

## Filtri (semantica)

## TIMEFRAME — codici (parametro stringa unico)
| Codice | Classe | Significato |
|---|---|---|
| `m1 m5 m15 m30 h1 h4 w1` | **barre** | strategia su barre OHLCV (piramide multi-TF) |
| `T1` | **tick (tape/L1)** | strategia su flusso tick (trade + BBO): es. `tick_ret`, `tick_imb` |
| `T2` | **book (L2/BBO)** | strategia sull'order book: es. `obi`, `microprice`, `bbo_mid` |
| `S1` | **snapshot** | strategia sulla catena opzioni (greche/screener) |
| `""` | qualunque | nessun filtro di timeframe |

## Esempio (stessa strategia su TF diversi)

IMPORT SIGNAL
SUB OnSig(id AS STRING, type AS STRING, symbol AS STRING, value AS DOUBLE, tf AS STRING)
  PRINT id; " tf="; tf; " "; type; " "; symbol; " = "; value
END SUB
SUB Main()
  SIGNAL SUBSCRIBE TYPES("iv") TIMEFRAME("h1") AS "h1"
  SIGNAL SUBSCRIBE TYPES("iv") TIMEFRAME("T1") AS "t1"
  SIGNAL SUBSCRIBE TYPES("bar") AS "any"          ' nessun filtro TF (handler a 4 param)
  ON SIGNAL "h1" CALL OnSig
  ON SIGNAL "t1" CALL OnSig
  SIGNAL EMIT "iv", "AAPL", 25.5 ASSET("equity") TIMEFRAME("h1")
  SIGNAL EMIT "iv", "AAPL", 99   ASSET("equity") TIMEFRAME("T1")
  SLEEP 300
END SUB


## Producer (gmbsuite)

## Parità e test

## Note

### TimeSeries-Guida-Utente

*Fonte: `TimeSeries-Guida-Utente`*


## 1. Medie mobili

IMPORT TIMESERIES

s = TSMA(serie, 5)          ' media mobile semplice su 5 periodi
e = TSEMA(serie, 5)         ' media mobile esponenziale


## 2. Filtri

lp = TSLOWPASS(serie, 0.2, 2)      ' passa-basso (frequenza wn, ordine)
hp = TSHIGHPASS(serie, 0.2, 2)     ' passa-alto
bp = TSBANDPASS(serie, 0.1, 0.3, 2)' passa-banda (wn1..wn2)
bs = TSBANDSTOP(serie, 0.1, 0.3, 2)' elimina-banda


## 3. Rolling window

PRINT TSROLLMEAN(serie, 5)          ' media rolling
PRINT TSROLLMEDIAN(serie, 5)        ' mediana rolling
PRINT TSROLLSTDDEV(serie, 5)        ' deviazione standard rolling
PRINT TSROLLMAD(serie, 5)           ' MAD rolling
PRINT TSROLLZSCORE(serie, 5)        ' z-score rolling
PRINT TSROLLMIN(serie, 5)           ' minimo rolling (TSMINMAX)
PRINT TSROLLQUANTILE(serie, 5, 0.9) ' quantile rolling
PRINT TSROLLDELTA(serie, 5)         ' delta rolling
PRINT TSROLLSKEWNESS(serie, 5)      ' skewness rolling
PRINT TSROLLKURTOSIS(serie, 5)      ' kurtosis rolling
PRINT TSROLLSHARPE(serie, 5, rf)    ' Sharpe rolling (risk-free)
PRINT TSROLLSORTINO(serie, 5, target)' Sortino rolling


## 4. Rolling tra due serie

PRINT TSROLLCOVARIANCE(s1, s2, 5)   ' covarianza rolling
PRINT TSROLLCORR(s1, s2, 5)         ' correlazione rolling
PRINT TSROLLREGRESSION(s1, s2, 5)   ' regressione rolling
PRINT TSROLLREGRESSIONSINGLE(s1, s2, 5)


## 5. Trasformazioni e diagnostica

d  = TSDIFFERENCE(serie, 1)         ' differenza (lag)
r  = TSRETURNS(serie)               ' ritorni percentuali
v  = TSVOLATILITY(serie)            ' volatilità
a  = TSAUTOCORR(serie, 5)           ' autocorrelazione
ds = TSDESEASONALIZE(serie, 5)      ' destagionalizzazione
mm = TSMINMAX(serie)                ' min/max della serie
o  = TSOUTLIERS(serie, threshold)   ' outlier
ra = TSROLLAUTOCORR(serie, 5)       ' autocorrelazione rolling


## 6. Funzioni che ritornano matrici

mm  = TSROLLMINMAX(serie, 5)        ' [finestre][2] = [min, max] per finestra
dec = TSDECOMPOSE(serie, 2)         ' 3×n = [trend, seasonal, residual]
cv  = TSCOVARIANCEMATRIX(data)      ' m×m covarianza (n_obs×n_vars, Bessel n−1)
rr  = TSROLLREGRESSION(s1, s2, 5)   ' [finestre][2] = [slope, intercept]
rs  = TSROLLREGRESSIONSINGLE(s1, 5) ' [finestre][2] = [slope, intercept]


## Vedi anche

### Trading-Bonds-Guida-Utente

*Fonte: `Trading-Bonds-Guida-Utente`*


## 1. Panoramica

IMPORT TRADING.BONDS AS BND
SUB Main()
    PRINT BND.BONDPRICE(DATETIME.DATEVALUE("2024-01-01"), DATETIME.DATEVALUE("2026-01-01"), 0.05, 0.05, 100, 1)
END SUB

| Funzione | Che cosa calcola |
|---|---|
| `BONDPRICE` | Prezzo di un'obbligazione |
| `BONDYIELD` | Rendimento a scadenza (YTM) dato il prezzo |
| `DURATION` | Durata (Macaulay o modificata) |
| `PRICESIMPLE` | Prezzo di un'obbligazione a periodi interi |
| `BONDEQUIVALENTYIELD` | Rendimento equivalente |
| `CURRENTYIELD` | Rendimento corrente (cedola/prezzo) |
| `YIELDTOMATURITY` | Rendimento semplice a scadenza |
| `ZEROCOUPONBONDVALUE` | Valore di uno zero‑coupon |
| `ZEROCOUPONBONDYIELD` | Rendimento di uno zero‑coupon |

## 2. Il parametro `model`
| `model` | Modello | Convenzioni |
|---|---|---|
| `0` (default) | **trading** | date cedola reali, day‑count, rateo (AI) |
| `1` | **numerics** | modello semplificato a periodi interi |

## 3. BONDPRICE — prezzo

prezzo = BND.BONDPRICE(settlement, maturity, rate, yld, redemption, frequency [, model])


IMPORT TRADING.BONDS AS BND
SUB Main()
    PRINT MATH.ROUND(BND.BONDPRICE(DATETIME.DATEVALUE("2024-01-01"), DATETIME.DATEVALUE("2026-01-01"), 0.05, 0.05, 100, 1), 4)
    PRINT MATH.ROUND(BND.BONDPRICE(DATETIME.DATEVALUE("2024-01-01"), DATETIME.DATEVALUE("2026-01-01"), 0.05, 0.05, 100, 1, 1), 4)
END SUB


 100
 95.6808


## 4. BONDYIELD — rendimento a scadenza

ytm = BND.BONDYIELD(settlement, maturity, rate, price, redemption, frequency [, model])


IMPORT TRADING.BONDS AS BND
SUB Main()
    PRINT MATH.ROUND(BND.BONDYIELD(DATETIME.DATEVALUE("2024-01-01"), DATETIME.DATEVALUE("2026-01-01"), 0.05, 96, 100, 1), 6)
    PRINT MATH.ROUND(BND.BONDYIELD(DATETIME.DATEVALUE("2024-01-01"), DATETIME.DATEVALUE("2026-01-01"), 0.05, 96, 100, 1, 1), 6)
END SUB


 0.072191
 0.048775


## 5. DURATION — durata

dur = BND.DURATION(settlement, maturity, rate, yld, frequency, basis [, calcMethod])


IMPORT TRADING.BONDS AS BND
SUB Main()
    PRINT MATH.ROUND(BND.DURATION(DATETIME.DATEVALUE("2024-01-01"), DATETIME.DATEVALUE("2026-01-01"), 0.05, 0.05, 1, 0, 1), 6)
END SUB


## 6. PRICESIMPLE — prezzo a periodi interi

prezzo = BND.PRICESIMPLE(settlement, maturity, rate, yld, redemption, frequency)


IMPORT TRADING.BONDS AS BND
SUB Main()
    PRINT MATH.ROUND(BND.PRICESIMPLE(DATETIME.DATEVALUE("2024-01-01"), DATETIME.DATEVALUE("2026-01-01"), 0.05, 0.04, 100, 1), 2)
END SUB


## 7. Rendite e strumenti senza cedola

ey = BND.BONDEQUIVALENTYIELD(faceValue, bondPrice, daysToMaturity)
cy = BND.CURRENTYIELD(annualCoupons, bondPrice)
ytm = BND.YIELDTOMATURITY(couponPayment, faceValue, price, years)
z = BND.ZEROCOUPONBONDVALUE(faceValue, rate, time)
zy = BND.ZEROCOUPONBONDYIELD(faceValue, price, time)


IMPORT TRADING.BONDS AS BND
SUB Main()
    PRINT BND.BONDEQUIVALENTYIELD(100, 95, 180)
    PRINT BND.CURRENTYIELD(5, 95)
    PRINT BND.YIELDTOMATURITY(5, 100, 95, 5)
    PRINT BND.ZEROCOUPONBONDVALUE(100, 0.05, 3)
END SUB


## 8. Note e limiti

## 9. Altre funzioni obbligazionarie (verifiche 2026-09)

IMPORT TRADING.BONDS AS B
PRINT ROUND(B.BondEquivalentYield(100, 96, 365), 6)    ' 0.041667
PRINT ROUND(B.CurrentYield(5, 96), 6)                  ' 5.208333
PRINT ROUND(B.YieldToMaturity(5, 100, 96, 10), 6)      ' 0.055102
PRINT ROUND(B.ZeroCouponBondValue(100, 0.05, 5), 6)    ' 78.352617
PRINT ROUND(B.ZeroCouponBondYield(100, 80, 5), 6)      ' 0.04564


### Trading-Guida-Utente

*Fonte: `Trading-Guida-Utente`*


## 1. Valutazione azionaria

IMPORT TRADING AS T

PRINT T.StockValueZeroGrowth(2, 0.1)       ' 20      D/r (Gordon senza crescita)
PRINT ROUND(T.StockValueConstGrowth(2, 0.1, 0.04), 4)  ' 33.3333   D/(k-g)
PRINT T.PreferredStockValue(2, 0.08)       ' 25      dividendo/tasso

| Funzione | Formula | Esempio |
|---|---|---|
| `StockValueZeroGrowth(dividend, requiredRate)` | `D/r` | `(2, 0.1)` → 20 |
| `StockValueConstGrowth(nextDividend, k, g)` | `D/(k−g)` | `(2, 0.1, 0.04)` → 33.3333 |
| `PreferredStockValue(dividend, requiredRate)` | `D/r` | `(2, 0.08)` → 25 |

## 2. CAPM e tasse

PRINT ROUND(T.CAPM(0.03, 1.2, 0.1), 6)          ' 0.114   rf + β·(rm − rf)
PRINT ROUND(T.TaxEquivalentYield(0.04, 0.3), 6)  ' 0.057143  tfy/(1−t)


## 3. Rendimenti

PRINT ROUND(T.RiskPremium(0.12, 0.03), 6)     ' 0.09       E[r] − rf
PRINT ROUND(T.TotalStockReturn(100, 110, 5), 6) ' 0.15     (P1−P0+D)/P0
PRINT ROUND(T.CapitalGainsYield(100, 110), 6)  ' 0.1       (P1−P0)/P0
PRINT ROUND(T.GrowthRate(0.6, 0.15), 6)        ' 0.09       retention·ROE
PRINT ROUND(T.BidAskSpread(100, 101), 6)       ' 1          ask−bid (assoluto)


### Rendimenti su un array di ritorni

DIM ret(2) AS DOUBLE = {0.1, -0.05, 0.2}
PRINT ROUND(T.HoldingPeriodReturn(ret), 6)     ' 0.254   Π(1+r_i) − 1
PRINT ROUND(T.GeometricMeanReturn(ret), 6)     ' 0.078365 (Π(1+r_i))^(1/n) − 1


## 4. Opzioni (TRADING.OPTIONS)

IMPORT TRADING.OPTIONS AS O
PRINT ROUND(O.BsCall(100, 100, 1, 0.05, 0.2, 0), 4)   ' 10.4506
PRINT ROUND(O.BsPut(100, 100, 1, 0.05, 0.2, 0), 4)    ' 5.5735
PRINT ROUND(O.Delta(100, 100, 1, 0.05, 0.2, 1), 6)    ' -0.363169 (type 1 = put)
PRINT ROUND(O.Gamma(100, 100, 1, 0.05, 0.2), 8)       ' greca Gamma
PRINT ROUND(O.Vega(100, 100, 1, 0.05, 0.2), 8)        ' 0.37524035
PRINT ROUND(O.Rho(100, 100, 1, 0.05, 0.2, 1), 8)      ' -0.41890461 (put)

| Funzione | Note |
|---|---|
| `Charm(S,K,T,r,v[,type])` | derivata di Delta rispetto al tempo |
| `Vanna(S,K,T,r,v)` | derivata di Vega rispetto a S |
| `Volga(S,K,T,r,v)` | derivata di Vega rispetto a v |
| `Zomma(S,K,T,r,v)` | derivata di Gamma rispetto a v |
| `DeltaSpeed(S,K,T,r,v)` | derivata di Gamma rispetto a S |
| `GammaColor(S,K,T,r,v)` | derivata di Gamma rispetto al tempo |
| `BinomialOption(S,K,T,r,v,steps[,isCall])` | modello binomiale |
| `MertonCall(S,K,T,r,v,q)` / `MertonPut(...)` | Black-Scholes con dividend continuo |

## 5. Metriche per azione e bilancio (TRADING.METRICS)

IMPORT TRADING.METRICS AS M
PRINT M.BookValuePerShare(500, 100)      ' 5
PRINT M.EPS(50, 100)                     ' 0.5
PRINT ROUND(M.DilutedEPS(50, 100, 10), 4) ' 0.4545
PRINT M.DPS(20, 100)                     ' 0.2
PRINT M.EquityMultiplier(500, 250)       ' 2
PRINT M.NAV(1000, 200, 100)              ' 8
PRINT M.PSRatio(10, 2)                   ' 5
PRINT M.PBRatio(10, 4)                   ' 2.5
PRINT M.DividendPayoutRatio(20, 50)      ' 0.4
PRINT M.PERatio(10, 2)                   ' 5    (prezzo / EPS)
PRINT M.DividendYield(5, 100)            ' 5    (dividendo / prezzo * 100)

' Metriche su array di rendimenti (CAPM / performance)
DIM asset(11) AS DOUBLE = {0.02, 0.035, -0.01, 0.042, 0.008, 0.025, 0.015, -0.005, 0.03, 0.012, 0.018, 0.022}
DIM bench(11) AS DOUBLE = {0.01, 0.015, -0.008, 0.02, 0.004, 0.012, 0.009, -0.003, 0.014, 0.006, 0.01, 0.011}

| Funzione | Note |
|---|---|
| `TotalReturn(r)` / `AnnualizedReturn(r, ppy)` | rendimento totale / annualizzato |
| `AnnualizedVolatility(r, ppy)` | volatilità annualizzata |
| `AnnualizedDownsideDeviation(r, ppy)` / `DownsideDeviation(r, target)` | downside deviation |
| `AnnualizedTrackingError(r, bench, ppy)` / `InformationRatio(r, bench, ppy)` | tracking error / information ratio |
| `SHARPERATIO(r, rf, ppy)` / `SortinoRatio(r, rf, target, ppy)` / `TreynorRatio(r, bench, rf, ppy)` | indici di performance corretti per il rischio |
| `CalmarRatio(r, ppy)` / `OmegaRatio(r, threshold)` / `UlcerIndex(r)` | altre misure |
| `MaxDrawdown(r)` | massimo drawdown |
| `ConditionalValueAtRisk(r, conf)` | CVaR (expected shortfall) |
| `ReturnsSkewness(r)` / `ReturnsKurtosis(r)` | momenti dei rendimenti |
| `JarqueBeraTest(r)` | test di normalità |

## 5bis. Portafoglio (TRADING.PORTFOLIO)

IMPORT TRADING.PORTFOLIO AS P
DIM hist(1, 11) AS DOUBLE     ' serie storiche (asset × periodi)
DIM w = {0.5, 0.5}            ' pesi

PRINT P.EXPECTEDRETURN(w, r)          ' rendimento atteso
PRINT P.STDEV(w, hist)                ' deviazione standard
PRINT P.PVAR(w, hist, 0.95)           ' Value-at-Risk
PRINT P.CVAR(w, hist, 0.95)           ' Conditional VaR
PRINT P.PORTFOLIOMAD(w, hist)         ' MAD (scarto dalla mediana)
PRINT P.PORTFOLIOSEMIMAD(w, hist, 0)  ' semi-MAD
PRINT P.ISLONGONLY(w)                 ' -1/0: pesi tutti ≥ 0

' Ottimizzazione di portafoglio (bounds = MATRICE/ARRAY per asset)
PRINT P.MAXSHARPE(hist, bounds)       ' max Sharpe
PRINT P.MEANVARIANCE(hist, bounds)    ' media-varianza
PRINT P.MINVOLATILITY(hist, bounds)   ' min volatilità


## 6. Verifiche sui quattro backend (2026-09)

20
33.3333
25
0.114
0.057143
0.09
0.15
0.1
0.09
1
0.254
0.078365


## Vedi anche

### Trading-Guida-Utente

*Fonte: `Trading-Guida-Utente`*


## 1. Valutazione azionaria

IMPORT TRADING AS T

PRINT T.StockValueZeroGrowth(2, 0.1)       ' 20      D/r (Gordon senza crescita)
PRINT ROUND(T.StockValueConstGrowth(2, 0.1, 0.04), 4)  ' 33.3333   D/(k-g)
PRINT T.PreferredStockValue(2, 0.08)       ' 25      dividendo/tasso

| Funzione | Formula | Esempio |
|---|---|---|
| `StockValueZeroGrowth(dividend, requiredRate)` | `D/r` | `(2, 0.1)` → 20 |
| `StockValueConstGrowth(nextDividend, k, g)` | `D/(k−g)` | `(2, 0.1, 0.04)` → 33.3333 |
| `PreferredStockValue(dividend, requiredRate)` | `D/r` | `(2, 0.08)` → 25 |

## 2. CAPM e tasse

PRINT ROUND(T.CAPM(0.03, 1.2, 0.1), 6)          ' 0.114   rf + β·(rm − rf)
PRINT ROUND(T.TaxEquivalentYield(0.04, 0.3), 6)  ' 0.057143  tfy/(1−t)


## 3. Rendimenti

PRINT ROUND(T.RiskPremium(0.12, 0.03), 6)     ' 0.09       E[r] − rf
PRINT ROUND(T.TotalStockReturn(100, 110, 5), 6) ' 0.15     (P1−P0+D)/P0
PRINT ROUND(T.CapitalGainsYield(100, 110), 6)  ' 0.1       (P1−P0)/P0
PRINT ROUND(T.GrowthRate(0.6, 0.15), 6)        ' 0.09       retention·ROE
PRINT ROUND(T.BidAskSpread(100, 101), 6)       ' 1          ask−bid (assoluto)


### Rendimenti su un array di ritorni

DIM ret(2) AS DOUBLE = {0.1, -0.05, 0.2}
PRINT ROUND(T.HoldingPeriodReturn(ret), 6)     ' 0.254   Π(1+r_i) − 1
PRINT ROUND(T.GeometricMeanReturn(ret), 6)     ' 0.078365 (Π(1+r_i))^(1/n) − 1


## 4. Opzioni (TRADING.OPTIONS)

IMPORT TRADING.OPTIONS AS O
PRINT ROUND(O.BsCall(100, 100, 1, 0.05, 0.2, 0), 4)   ' 10.4506
PRINT ROUND(O.BsPut(100, 100, 1, 0.05, 0.2, 0), 4)    ' 5.5735
PRINT ROUND(O.Delta(100, 100, 1, 0.05, 0.2, 1), 6)    ' -0.363169 (type 1 = put)
PRINT ROUND(O.Gamma(100, 100, 1, 0.05, 0.2), 8)       ' greca Gamma
PRINT ROUND(O.Vega(100, 100, 1, 0.05, 0.2), 8)        ' 0.37524035
PRINT ROUND(O.Rho(100, 100, 1, 0.05, 0.2, 1), 8)      ' -0.41890461 (put)

| Funzione | Note |
|---|---|
| `Charm(S,K,T,r,v[,type])` | derivata di Delta rispetto al tempo |
| `Vanna(S,K,T,r,v)` | derivata di Vega rispetto a S |
| `Volga(S,K,T,r,v)` | derivata di Vega rispetto a v |
| `Zomma(S,K,T,r,v)` | derivata di Gamma rispetto a v |
| `DeltaSpeed(S,K,T,r,v)` | derivata di Gamma rispetto a S |
| `GammaColor(S,K,T,r,v)` | derivata di Gamma rispetto al tempo |
| `BinomialOption(S,K,T,r,v,steps[,isCall])` | modello binomiale |
| `MertonCall(S,K,T,r,v,q)` / `MertonPut(...)` | Black-Scholes con dividend continuo |

## 5. Metriche per azione e bilancio (TRADING.METRICS)

IMPORT TRADING.METRICS AS M
PRINT M.BookValuePerShare(500, 100)      ' 5
PRINT M.EPS(50, 100)                     ' 0.5
PRINT ROUND(M.DilutedEPS(50, 100, 10), 4) ' 0.4545
PRINT M.DPS(20, 100)                     ' 0.2
PRINT M.EquityMultiplier(500, 250)       ' 2
PRINT M.NAV(1000, 200, 100)              ' 8
PRINT M.PSRatio(10, 2)                   ' 5
PRINT M.PBRatio(10, 4)                   ' 2.5
PRINT M.DividendPayoutRatio(20, 50)      ' 0.4
PRINT M.PERatio(10, 2)                   ' 5    (prezzo / EPS)
PRINT M.DividendYield(5, 100)            ' 5    (dividendo / prezzo * 100)

' Metriche su array di rendimenti (CAPM / performance)
DIM asset(11) AS DOUBLE = {0.02, 0.035, -0.01, 0.042, 0.008, 0.025, 0.015, -0.005, 0.03, 0.012, 0.018, 0.022}
DIM bench(11) AS DOUBLE = {0.01, 0.015, -0.008, 0.02, 0.004, 0.012, 0.009, -0.003, 0.014, 0.006, 0.01, 0.011}

| Funzione | Note |
|---|---|
| `TotalReturn(r)` / `AnnualizedReturn(r, ppy)` | rendimento totale / annualizzato |
| `AnnualizedVolatility(r, ppy)` | volatilità annualizzata |
| `AnnualizedDownsideDeviation(r, ppy)` / `DownsideDeviation(r, target)` | downside deviation |
| `AnnualizedTrackingError(r, bench, ppy)` / `InformationRatio(r, bench, ppy)` | tracking error / information ratio |
| `SHARPERATIO(r, rf, ppy)` / `SortinoRatio(r, rf, target, ppy)` / `TreynorRatio(r, bench, rf, ppy)` | indici di performance corretti per il rischio |
| `CalmarRatio(r, ppy)` / `OmegaRatio(r, threshold)` / `UlcerIndex(r)` | altre misure |
| `MaxDrawdown(r)` | massimo drawdown |
| `ConditionalValueAtRisk(r, conf)` | CVaR (expected shortfall) |
| `ReturnsSkewness(r)` / `ReturnsKurtosis(r)` | momenti dei rendimenti |
| `JarqueBeraTest(r)` | test di normalità |

## 5bis. Portafoglio (TRADING.PORTFOLIO)

IMPORT TRADING.PORTFOLIO AS P
DIM hist(1, 11) AS DOUBLE     ' serie storiche (asset × periodi)
DIM w = {0.5, 0.5}            ' pesi

PRINT P.EXPECTEDRETURN(w, r)          ' rendimento atteso
PRINT P.STDEV(w, hist)                ' deviazione standard
PRINT P.PVAR(w, hist, 0.95)           ' Value-at-Risk
PRINT P.CVAR(w, hist, 0.95)           ' Conditional VaR
PRINT P.PORTFOLIOMAD(w, hist)         ' MAD (scarto dalla mediana)
PRINT P.PORTFOLIOSEMIMAD(w, hist, 0)  ' semi-MAD
PRINT P.ISLONGONLY(w)                 ' -1/0: pesi tutti ≥ 0

' Ottimizzazione di portafoglio (bounds = MATRICE/ARRAY per asset)
PRINT P.MAXSHARPE(hist, bounds)       ' max Sharpe
PRINT P.MEANVARIANCE(hist, bounds)    ' media-varianza
PRINT P.MINVOLATILITY(hist, bounds)   ' min volatilità


## 6. Verifiche sui quattro backend (2026-09)

20
33.3333
25
0.114
0.057143
0.09
0.15
0.1
0.09
1
0.254
0.078365


## Vedi anche

### Signal-Guida-Utente

*Fonte: `Signal-Guida-Utente`*


## 1. Panoramica
| Famiglia | Funzioni | Ritorno |
|---|---|---|
| Analisi / trasformate | `CONVOLVE`, `CORRELATE`, `FINDPEAKS`, `SAVGOLFILTER`, `RESAMPLE`, `DECIMATE`, `WELCH` | array / handle |
| Finestre | `HANN`, `HAMMING`, `BLACKMAN` | array |
| Filtri IIR | `BUTTER`, `CHEBY1`, `CHEBY2`, `ELLIP` (design → **handle**), `LFILTER`, `FILTFILT`, `FREQZ` (applicazione) | handle / array |

## 2. Finestre

IMPORT SIGNAL AS S
DIM w() = S.HANN(5)
PRINT w(0); w(1); w(2); w(3); w(4)   ' 0  0.5  1  0.5  0


## 3. Convoluzione, correlazione, picchi

DIM x(3) AS DOUBLE = {1, 2, 3, 4}
DIM k(1) AS DOUBLE = {1, 1}
DIM c() = S.CONVOLVE(x, k)     ' 1  3  5  7  4
DIM r() = S.CORRELATE(x, k)
DIM p() = S.FINDPEAKS(x, 1)    ' indici dei massimi locali


## 4. Filtri IIR (design + applicazione)

DIM wn(0) AS DOUBLE = {0.3}          ' frequenza normalizzata (0..1)
DIM h = S.BUTTER(2, wn, "low")       ' Butterworth ordine 2 low-pass
DIM y() = S.LFILTER(h, x)            ' filtro diretto (direct form II transposed)
DIM z() = S.FILTFILT(h, x)           ' zero-phase (forward+backward)
DIM f() = S.FREQZ(h, 4)              ' risposta in ampiezza a 4 punti

| Funzione | Firma | Note |
|---|---|---|
| `BUTTER` | `BUTTER(n, wn[], btype)` | Butterworth |
| `CHEBY1` | `CHEBY1(n, rp, wn[], btype)` | Chebyshev I (ripple `rp` in banda) |
| `CHEBY2` | `CHEBY2(n, rs, wn[], btype)` | Chebyshev II (attenuazione `rs`) |
| `ELLIP` | `ELLIP(n, rp, rs, wn[], btype)` | ellittico |

## 5. Welch

DIM h = S.WELCH(x, 4, 1.0)
IF h > 0 THEN PRINT "ok"


## 6. Note di parità

## 7. MiniTest e sample

<!-- END SURFACE -->







<!-- BEGIN DOMAIN TABLE -->
## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **FINANCE.CUMIPMT(x1, x2, x3, x4, x5, x6)** — `PRINT "CUMIPMT test:"; FINANCE.CUMIPMT(0.05, 360, 200000, 1, 12, 0)` (shared/samples/Finance/FinanceGeneralTests.bas:112)
- **FINANCE.CUMPRINC(x1, x2, x3, x4, x5, x6)** — `PRINT "CUMPRINC test:"; FINANCE.CUMPRINC(0.05, 360, 200000, 1, 12, 0)` (shared/samples/Finance/FinanceGeneralTests.bas:113)
- **FINANCE.DAYS360(x1, x2)** — `PRINT "Days360 (US):      "; FINANCE.DAYS360(StartD, EndD)` (shared/samples/Finance/FinanceGeneralTests.bas:108)
- **FINANCE.DB(x1, x2, x3, x4, x5)** — `MATH.ROUND(FINANCE.DB(costo, valoreResiduo, anni, a, 12), 2); "    "; _` (shared/samples/Finance/InvestimentoFurgone.bas:34)
- **FINANCE.DDB(x1, x2, x3, x4, x5)** — `MATH.ROUND(FINANCE.DDB(costo, valoreResiduo, anni, a, 2.0), 2)` (shared/samples/Finance/InvestimentoFurgone.bas:35)
- **FINANCE.FV(x1, x2, x3)** — `fv = FINANCE.FV(0.03 / 12, 12, -media, 0, 0)` (shared/samples/Math/PianificazioneRisparmi.bas:40)
- **FINANCE.FVEXT(x1, x2, x3)** — `PRINT "FVEXT test:"; FINANCE.FVEXT(0.05, 360, -1000)` (shared/samples/Finance/FinanceGeneralTests.bas:115)
- **FINANCE.IRR(x1)** — `rendimento = FINANCE.IRR(flussi, 0.1)` (shared/samples/Finance/InvestimentoFurgone.bas:68)
- **FINANCE.MIRR(x1, x2, x3)** — `mirr = FINANCE.MIRR(flussi, 0.06, 0.09)` (shared/samples/Finance/InvestimentoFurgone.bas:72)
- **FINANCE.MNPV(x1, x2)** — `PRINT "MNPV test:"; FINANCE.MNPV(0.1, Flussi)` (shared/samples/Finance/FinanceGeneralTests.bas:116)
- **FINANCE.NPER(x1, x2, x3)** — `mesi = FINANCE.NPER(0.027 / 12.0, rataB - 100.0, capitale, 0, 0)` (shared/samples/Finance/MutuoCasa.bas:62)
- **FINANCE.NPEREXT(x1, x2, x3)** — `N = FINANCE.NPEREXT(0.005, -100, 0, 10000)` (shared/samples/Trading/FinanceExtendedFunctions.bas:40)
- **FINANCE.NPV(x1)** — `attuale = FINANCE.NPV(0.08, flussiPositivi) - costo` (shared/samples/Finance/InvestimentoFurgone.bas:56)
- **FINANCE.PMT(x1, x2, x3)** — `DIM Rata = FINANCE.PMT(TassoAnnuo/12, Anni*12, -Importo)` (shared/samples/Finance/FinanceGeneralTests.bas:36)
- **FINANCE.PMTEXT(x1, x2, x3)** — `PMT = FINANCE.PMTEXT(RATE, 60, 10000)` (shared/samples/Trading/FinanceExtendedFunctions.bas:22)
- **FINANCE.PV(x1, x2, x3)** — `PRINT ROUND(FINANCE.PV(0.01, 12, -100, 0), 2)` (shared/minitests/finance/pv.bas:3)
- **FINANCE.PVEXT(x1, x2, x3)** — `PV = FINANCE.PVEXT(0.005, 60, -100)` (shared/samples/Trading/FinanceExtendedFunctions.bas:34)
- **FINANCE.RATE(x1, x2, x3)** — `DIM TassoCalc = FINANCE.RATE(3*12, -300, 10000, 0, 0) * 12` (shared/samples/Finance/FinanceGeneralTests.bas:46)
- **FINANCE.SLN(x1, x2, x3)** — `MATH.ROUND(FINANCE.SLN(costo, valoreResiduo, anni), 2); "   "; _` (shared/samples/Finance/InvestimentoFurgone.bas:32)
- **FINANCE.SYD(x1, x2, x3, x4)** — `MATH.ROUND(FINANCE.SYD(costo, valoreResiduo, anni, a), 2); "   "; _` (shared/samples/Finance/InvestimentoFurgone.bas:33)
- **FINANCE.XIRR(x1, x2)** — `xr = FINANCE.XIRR(flussi, dateFlussi, 0.1)` (shared/samples/Finance/InvestimentoFurgone.bas:84)
- **FINANCE.XNPV(x1, x2, x3)** — `PRINT "XNPV @8%:          "; MATH.ROUND(FINANCE.XNPV(0.08, flussi, dateFlussi), 2); " EUR"` (shared/samples/Finance/InvestimentoFurgone.bas:86)
- **FINANCE.YEARFRAC(x1, x2)** — `PRINT "YearFrac (30/360): "; MATH.ROUND(FINANCE.YEARFRAC(StartD, EndD, 0), 4)` (shared/samples/Finance/FinanceGeneralTests.bas:107)
- **SIGNAL.BLACKMAN(x1)** — `DIM b() = SIGNAL.BLACKMAN(4)` (shared/samples/Math/SignalSciPyParity.bas:14)
- **SIGNAL.BUTTER(x1, x2, x3)** — `h = SIGNAL.BUTTER(2, wn, "low")` (shared/samples/Math/SignalFiltersSciPyParity.bas:9)
- **SIGNAL.CHEBY1(x1, x2, x3, x4)** — `h3 = SIGNAL.CHEBY1(2, 1.0, wn, "low")` (shared/samples/Math/SignalFiltersSciPyParity.bas:23)
- **SIGNAL.CHEBY2(x1, x2, x3, x4)** — `h4 = SIGNAL.CHEBY2(2, 20.0, wn, "low")` (shared/samples/Math/SignalFiltersSciPyParity.bas:26)
- **SIGNAL.CONVOLVE(x1, x2)** — `DIM c() = SIGNAL.CONVOLVE(x, k)` (shared/samples/Math/SignalSciPyParity.bas:10)
- **SIGNAL.CORRELATE(x1, x2)** — `DIM r() = SIGNAL.CORRELATE(x, k)` (shared/samples/Math/SignalSciPyParity.bas:11)
- **SIGNAL.DECIMATE(x1, x2)** — `DIM dc() = SIGNAL.DECIMATE(n, 4)` (shared/samples/Math/SciPyP1Parity.bas:27)
- **SIGNAL.ELLIP(x1, x2, x3, x4, x5)** — `h2 = SIGNAL.ELLIP(3, 1.0, 20.0, wn, "low")` (shared/samples/Math/SignalFiltersSciPyParity.bas:18)
- **SIGNAL.FILTFILT(x1, x2)** — `DIM z() = SIGNAL.FILTFILT(h, x)` (shared/samples/Math/SignalFiltersSciPyParity.bas:12)
- **SIGNAL.FINDPEAKS(x1, x2)** — `DIM pk() = SIGNAL.FINDPEAKS(sig, 1)` (shared/samples/Math/SignalIntegrateP0Parity.bas:14)
- **SIGNAL.FIRWIN(x1, x2, x3, x4)** — `DIM taps() = SIGNAL.FIRWIN(11, fc, "lowpass", "hann")` (shared/samples/Math/SignalIntegrateP0Parity.bas:8)
- **SIGNAL.FREQZ(x1, x2)** — `DIM m() = SIGNAL.FREQZ(h, 8)` (shared/samples/Math/SignalFiltersSciPyParity.bas:14)
- **SIGNAL.HAMMING(x1)** — `DIM m() = SIGNAL.HAMMING(4)` (shared/samples/Math/SignalSciPyParity.bas:13)
- **SIGNAL.HANN(x1)** — `DIM h() = SIGNAL.HANN(4)` (shared/samples/Math/SignalSciPyParity.bas:12)
- **SIGNAL.LFILTER(x1, x2)** — `DIM y() = SIGNAL.LFILTER(h, x)` (shared/samples/Math/SignalFiltersSciPyParity.bas:10)
- **SIGNAL.RESAMPLE(x1, x2)** — `DIM rs() = SIGNAL.RESAMPLE(n, 128)` (shared/samples/Math/SciPyP1Parity.bas:23)
- **SIGNAL.SAVGOLFILTER(x1, x2, x3)** — `DIM sg() = SIGNAL.SAVGOLFILTER(q, 9, 2)` (shared/samples/Math/SciPyP1Parity.bas:18)
- **SIGNAL.WELCH(x1, x2, x3)** — `w = SIGNAL.WELCH(n, 64, 1.0)` (shared/samples/Math/SciPyP1Parity.bas:11)
<!-- END DOMAIN TABLE -->
