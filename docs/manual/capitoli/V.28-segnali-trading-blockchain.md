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













<!-- BEGIN DOMAIN TABLE -->
## Dizionario operativo del dominio TRADING

Le 77 funzioni `TRADING.*` che nelle sezioni precedenti comparivano soltanto nella tavola sinottica sono qui spiegate una per una: cosa calcolano, in che ordine vanno passati gli argomenti, che cosa restituiscono e quale chiamata reale le esercita. Quattro convenzioni valgono per tutto il dominio e vanno lette prima di qualsiasi altra cosa.

**Rendimenti decimali.** Un rendimento dell'1% si scrive `0.01`, non `1`. Tutte le funzioni che ricevono un array di ritorni (`TRADING.METRICS.*`, `TRADING.PORTFOLIO.*`) si aspettano questa forma; le poche eccezioni che lavorano in punti percentuali sono segnalate nella voce (per esempio `TRADING.METRICS.DividendYield`).

**Periodi per anno.** Il parametro `periodi` è il numero di osservazioni in un anno: `252` per le serie giornaliere di Borsa, `12` per quelle mensili, `4` per le trimestrali. È l'unico dato che distingue un rendimento annualizzato da uno periodale, quindi non è mai opzionale nelle funzioni che lo richiedono.

**Date come seriali.** Le funzioni obbligazionarie non accettano stringhe: `inizio` e `scadenza` sono seriali prodotti da `DATETIME.DATEVALUE("2024-01-01")`. Passare una stringa produce un errore di tipo, non una conversione implicita.

**Record di ritorno.** Le funzioni di ottimizzazione di portafoglio non restituiscono un vettore ma un record: i pesi si leggono nel campo `.Weights` (`LEN(x.Weights)`, `x.Weights(0)`). Un risultato `NULL` in quel campo significa che il problema non ha soluzione ammissibile con i vincoli dati, non che i pesi siano vuoti.

### Obbligazioni — `TRADING.BONDS` (9 funzioni)

Il blocco obbligazionario lavora su obbligazioni a cedola fissa e su zero-coupon. Il parametro `model` che compare in alcune firme sceglie la convenzione di attualizzazione: `0` = composta (la convenzione normale per titoli con più cedole), `1` = semplice (usata sul mercato monetario e sui titoli a vita breve). La `frequenza` è il numero di cedole per anno: `1` annuale, `2` semestrale, `4` trimestrale.

**TRADING.BONDS.BONDPRICE(inizio, scadenza, cedola, rendimento, nominale, frequenza [, model])** — prezzo tel-quel del titolo: attualizza le cedole future e il rimborso al rendimento richiesto, contando i giorni residui tra `inizio` e `scadenza`. È la funzione di prezzo di riferimento e la sua inversa è `BONDYIELD`. Esempio reale (`shared/minitests/trading/bond.bas:3`): `BND.BONDPRICE(DATETIME.DATEVALUE("2024-01-01"), DATETIME.DATEVALUE("2026-01-01"), 0.05, 0.05, 100, 1)` — un biennale con cedola 5% quotato a un rendimento 5%, che vale il nominale; la variante con `model = 1` mostra quanto la convenzione semplice sposti il prezzo.

**TRADING.BONDS.BONDYIELD(inizio, scadenza, cedola, prezzo, nominale, frequenza [, model])** — il rendimento a scadenza implicito in un prezzo osservato: risolve rispetto al rendimento l'equazione di `BONDPRICE`. È la funzione da usare quando il dato di mercato è il prezzo e si vuole la quotazione in termini di tasso. Esempio reale (`shared/minitests/trading/bond.bas:5`): `BND.BONDYIELD(..., 0.05, 96, 100, 1)` su un titolo comprato sotto la pari, che restituisce un rendimento superiore alla cedola.

**TRADING.BONDS.DURATION(inizio, scadenza, cedola, rendimento, frequenza, convenzione, model)** — durata del titolo, cioè la vita media finanziaria dei flussi. È la **durata di Macaulay**, non la durata modificata: sull'esempio del MiniTest (`shared/minitests/trading/duration.bas:6`) un biennale con cedola 5% a rendimento 5% e cedola annuale restituisce `1.952381`, valore coerente con la media ponderata dei tempi di incasso. Per ottenere la sensibilità al tasso si divide per `1 + rendimento / frequenza`.

**TRADING.BONDS.PRICESIMPLE(inizio, scadenza, cedola, rendimento, nominale, frequenza)** — prezzo calcolato senza frazionare il periodo in corso: si attualizza come se la prossima cedola cadesse esattamente tra un periodo intero. Serve a riprodurre le convenzioni di quei mercati che non ammettono giorni di rateo. Esempio reale (`shared/minitests/trading/pricesimple.bas:6`): `BND.PRICESIMPLE(..., 0.05, 0.04, 100, 1)` restituisce `101.886095`, cioè un titolo sopra la pari perché il rendimento richiesto (4%) è inferiore alla cedola (5%).

**TRADING.BONDS.CURRENTYIELD(cedola, prezzo)** — rendimento corrente: la cedola annua divisa per il prezzo, senza considerare il guadagno o la perdita in conto capitale fino a scadenza. È una misura grezza ma immediata, utile per confrontare titoli con vita residua molto diversa. Esempio reale (`shared/samples/Trading/TradingOptionsAndBonds.bas:105`): `BND.CURRENTYIELD(5, 95)` su una cedola 5 comprata a 95.

**TRADING.BONDS.YIELDTOMATURITY(cedola, nominale, prezzo, anni)** — rendimento a scadenza nella forma compatta a quattro argomenti, con gli anni espressi come numero e non come coppia di date. È la variante da usare quando si ragiona in «anni residui» invece che su un calendario. Esempio reale (`shared/samples/Trading/TradingOptionsAndBonds.bas:106`): `BND.YIELDTOMATURITY(5, 100, 95, 5)`.

**TRADING.BONDS.ZEROCOUPONBONDVALUE(nominale, rendimento, anni)** — valore attuale di uno zero-coupon: un solo flusso a scadenza, attualizzato per gli anni indicati. Esempio reale (`shared/minitests/trading.bonds/gen_TRADING.BONDS_ZeroCouponBondValue.bas:5`): `TRADING.BONDS.ZEROCOUPONBONDVALUE(0, 0, 0)`, il caso degenere usato dal MiniTest generato per verificare che tutti e quattro i backend rispondano `0` (oracolo `' EXPECT: 0`) invece di divergere su argomenti nulli.

**TRADING.BONDS.ZEROCOUPONBONDYIELD(nominale, prezzo, anni)** — rendimento implicito di uno zero-coupon acquistato a `prezzo` e rimborsato a `nominale` tra `anni`: l'inversa della funzione precedente. Esempio reale (`shared/samples/Trading/TradingOptionsAndBonds.bas:108`): `BND.ZEROCOUPONBONDYIELD(100, 78, 5)` su un titolo comprato a 78 e rimborsato a 100.

**TRADING.BONDS.BONDEQUIVALENTYIELD(nominale, prezzo, giorni)** — rendimento espresso su base annua con la convenzione del mercato monetario (`(nominale - prezzo) / prezzo * 365 / giorni`), che rende confrontabili titoli con durate inferiori all'anno. Esempio reale (`shared/samples/Trading/TradingOptionsAndBonds.bas:104`): `BND.BONDEQUIVALENTYIELD(100, 95, 180)` su uno strumento a sei mesi comprato a 95.

### Metriche azionarie e di rendimento — `TRADING.METRICS` (32 funzioni)

Questo blocco si divide in tre gruppi che rispondono a domande diverse: le misure di rischio e performance su una serie di ritorni, gli indicatori per azione ricavati da bilancio, e i test di forma della distribuzione. Il primo gruppo riceve sempre un array di ritorni e restituisce uno scalare; il secondo riceve valori di bilancio in unità monetarie e restituisce un rapporto; il terzo riceve la sola serie e restituisce una statistica o un record.

**TRADING.METRICS.Alpha(ritorni, benchmark, riskFree, periodi)** — alfa di Jensen: il rendimento in eccesso rispetto a quello spiegato dal beta, annualizzato sui `periodi`. Un alfa positivo indica che la strategia ha battuto il proprio benchmark a parità di rischio sistematico. Esempio reale (`shared/minitests/trading.metrics/alpha.bas:6`): `TRADING.METRICS.ALPHA(a, b, 0.0, 252)` con due serie di quattro ritorni, che restituisce `13.7262098682992` — valore grande perché quattro osservazioni annualizzate su 252 periodi amplificano enormemente la stima.

**TRADING.METRICS.AnnualizedReturn(ritorni, periodi)** — rendimento medio per periodo riportato su base annua. È la media aritmetica dei ritorni moltiplicata per `periodi`, non un tasso composto. Esempio reale (`shared/minitests/trading/annualizedreturn.bas:7`): cinque ritorni mensili con `periodi = 12` danno `0.1210136`.

**TRADING.METRICS.AnnualizedVolatility(ritorni, periodi)** — deviazione standard dei ritorni per la radice di `periodi`: la volatilità annualizzata. Sulla stessa serie dell'esempio precedente, `TRADING.METRICS.AnnualizedVolatility(r, 12)` restituisce `0.0325515` (`shared/minitests/trading/annualizedvolatility.bas:7`).

**TRADING.METRICS.AnnualizedDownsideDeviation(ritorni, MAR, periodi)** — deviazione standard calcolata sui soli ritorni sotto il rendimento minimo accettabile `MAR` (minimum acceptable return), annualizzata. È il denominatore del Sortino ratio: punisce solo la volatilità che fa male. Esempio reale (`shared/minitests/trading/annualizeddownsidedeviation.bas:7`): `TM.AnnualizedDownsideDeviation(r, 0, 12)` restituisce `0.01732051`.

**TRADING.METRICS.AnnualizedTrackingError(ritorni, benchmark, periodi)** — deviazione standard della differenza tra i ritorni della strategia e quelli del benchmark, annualizzata. Misura quanto una gestione si discosta dal proprio indice, non se lo batte. Esempio reale (`shared/minitests/trading/annualizedtrackingerror.bas:8`): `TM.AnnualizedTrackingError(r, b, 12)` restituisce `0.00997998`.

**TRADING.METRICS.Beta(ritorni, benchmark)** — beta di regressione: la covarianza tra titolo e mercato divisa per la varianza del mercato. `1` significa che il titolo si muove come il mercato, `0.5` metà, un valore negativo che si muove al contrario. La firma a due argomenti registra nel corpus solo la funzione (il primo uso reale trovato appartiene a un MiniTest di un altro dominio, `SPECIAL.BETALN`), quindi l'ordine da rispettare è ritorni dell'attività e poi quelli del benchmark.

**TRADING.METRICS.BookValuePerShare(patrimonio, azioni)** — patrimonio netto diviso il numero di azioni: il valore contabile di un'azione, usato come denominatore del price-to-book. Esempio reale (`shared/minitests/trading/met1.bas:3`): `M.BookValuePerShare(500, 100)` restituisce `5`.

**TRADING.METRICS.CalmarRatio(ritorni, periodi)** — rendimento annualizzato diviso il massimo drawdown in valore assoluto: quantifica quanto rendimento si ottiene per ogni punto di perdita massima sopportata. Esempio reale (`shared/minitests/trading/calmar.bas:7`): `TM.CalmarRatio(r, 12)` restituisce `24.20272006` sulla serie di cinque ritorni usata da tutto il blocco.

**TRADING.METRICS.ConditionalValueAtRisk(ritorni, confidenza)** — perdita media attesa nella coda oltre il quantile di confidenza (expected shortfall): a differenza del VaR dice *quanto* si perde nei casi peggiori, non solo che li si supera con probabilità `1 - confidenza`. Esempio reale (`shared/minitests/trading/conditionalvalueatrisk.bas:7`): `TM.ConditionalValueAtRisk(r, 0.95)` restituisce `-0.005`, cioè la peggiore perdita della serie.

**TRADING.METRICS.ValueAtRisk(ritorni, confidenza)** — quantile della distribuzione dei ritorni al livello di confidenza indicato: il VaR storico, che non assume normalità. Restituisce un numero negativo quando la soglia è una perdita. Esempio reale (`shared/minitests/trading/valueatrisk.bas:7`): `TM.ValueAtRisk(r, 0.95)` restituisce `-0.005`; con `0.95` su cinque osservazioni il quantile coincide di fatto con il minimo.

**TRADING.METRICS.DownsideDeviation(ritorni, MAR)** — la stessa misura della versione annualizzata ma senza scaling: deviazione dei soli ritorni negativi rispetto a `MAR`. Esempio reale (`shared/minitests/trading/downsidedeviation.bas:7`): `TM.DownsideDeviation(r, 0)` restituisce `0.005`.

**TRADING.METRICS.SortinoRatio(ritorni, MAR, periodi)** — rendimento in eccesso rispetto a `MAR` diviso la deviazione downside annualizzata: il fratello del Sharpe che non penalizza la volatilità positiva. Esempio reale (`shared/minitests/trading/sortinoratio.bas:7`): `TM.SortinoRatio(r, 0.01, 12)` restituisce `-0.12949347`, negativo perché la media dei ritorni resta sotto la soglia richiesta.

**TRADING.METRICS.SHARPERATIO(rendimentoPortafoglio, rischio, riskFree)** — la forma a tre scalari del Sharpe ratio: `(rendimento - riskFree) / rischio`. Non riceve una serie, quindi non calcola nulla: è l'utente a fornire rendimento e volatilità già stimati. Esempio reale (`shared/minitests/trading.portfolio/sharperatio.bas:5`): `TP.SHARPERATIO(0.15, 0.2, 0.01)` restituisce `0.7`.

**TRADING.METRICS.InformationRatio(ritorni, benchmark, periodi)** — rendimento attivo medio diviso il tracking error: misura la capacità di battere l'indice per unità di scostamento da esso. Esempio reale (`shared/minitests/trading/informationratio.bas:8`): `TM.InformationRatio(r, b, 12)` restituisce `3.12625878`.

**TRADING.METRICS.TreynorRatio(ritorni, benchmark, riskFree, periodi)** — rendimento in eccesso rispetto al risk-free diviso il beta: come il Sharpe, ma al posto del rischio totale usa quello sistematico. Esempio reale (`shared/minitests/trading/treynorratio.bas:8`): `TM.TreynorRatio(r, b, 0.01, 12)` restituisce `-0.00415436`.

**TRADING.METRICS.OmegaRatio(ritorni, soglia)** — rapporto tra la somma dei guadagni sopra la soglia e la somma delle perdite sotto: un valore `10.6` significa che, su quella serie, i guadagni in eccesso valgono dieci volte e mezzo le perdite. Esempio reale (`shared/minitests/trading/omegaratio.bas:7`): `TM.OmegaRatio(r, 0)` restituisce `10.6`.

**TRADING.METRICS.MaxDrawdown(ritorni)** — la perdita massima da un picco al minimo successivo, espressa come numero negativo. È l'unica metrica del blocco che restituisce un valore negativo per costruzione. Esempio reale (`shared/minitests/trading/maxdrawdown.bas:7`): `TM.MaxDrawdown(r)` restituisce `-0.005` sulla serie di prova.

**TRADING.METRICS.UlcerIndex(ritorni)** — radice della media dei quadrati dei drawdown: misura la profondità e la persistenza delle perdite, non solo il massimo. Esempio reale (`shared/minitests/trading/ulcerindex.bas:7`): `TM.UlcerIndex(r)` restituisce `0.2236068`.

**TRADING.METRICS.TotalReturn(ritorni)** — rendimento complessivo della serie, ottenuto componendo i ritorni periodali. Esempio reale (`shared/minitests/trading/totalreturn.bas:7`): `TM.TotalReturn(r)` restituisce `0.04874813` sui cinque ritorni di prova.

**TRADING.METRICS.ReturnsSkewness(ritorni)** — asimmetria della distribuzione dei ritorni: negativa indica una coda a sinistra, cioè perdite rare ma profonde. Esempio reale (`shared/minitests/trading/returnsskewness.bas:7`): `TM.ReturnsSkewness(r)` restituisce `-0.92089294`.

**TRADING.METRICS.ReturnsKurtosis(ritorni)** — curtosi (quarto momento standardizzato) della distribuzione dei ritorni: valori alti indicano code grasse. Esempio reale (`shared/minitests/trading/returnskurtosis.bas:7`): `TM.ReturnsKurtosis(r)` restituisce `1.29740191`.

**TRADING.METRICS.JarqueBeraTest(ritorni)** — test di normalità di Jarque-Bera: restituisce un record il cui primo campo è la statistica (`jb(0)`), da confrontare con il chi-quadro a due gradi di libertà. Esempio reale (`shared/minitests/trading/jarqueberatest.bas:7`): `DIM jb = TM.JarqueBeraTest(r)` seguito da `PRINT ROUND(jb(0), 8)` che stampa `1.05738062`.

**TRADING.METRICS.PERatio(prezzo, utiliPerAzione)** — price-to-earnings: quante volte il mercato paga gli utili correnti. Esempio reale (`shared/minitests/trading/peratio.bas:7`): `TM.PERatio(10, 2)` restituisce `5`.

**TRADING.METRICS.PBRatio(prezzo, patrimonioPerAzione)** — price-to-book: il multiplo del valore contabile. Esempio reale (`shared/minitests/trading/met1.bas:10`): `M.PBRatio(10, 4)` restituisce `2.5`.

**TRADING.METRICS.PSRatio(prezzo, ricaviPerAzione)** — price-to-sales: il multiplo dei ricavi, usato quando gli utili sono negativi e il P/E non è interpretabile. Esempio reale (`shared/minitests/trading/met1.bas:9`): `M.PSRatio(10, 2)` restituisce `5`.

**TRADING.METRICS.DividendYield(dividendoPerAzione, prezzo)** — rendimento da dividendo. Attenzione alla convenzione: **restituisce punti percentuali**, non un decimale — `DividendYield(5, 100)` vale `5` e significa 5%, non 500% (`shared/minitests/trading/dividendyield.bas:7`). È l'eccezione più insidiosa del dominio rispetto alla regola dei rendimenti decimali.

**TRADING.METRICS.DividendPayoutRatio(dividendoPerAzione, utiliPerAzione)** — quota degli utili distribuita come dividendo: `0.4` significa che il 40% degli utili è stato pagato e il resto trattenuto. Esempio reale (`shared/minitests/trading/met1.bas:11`): `M.DividendPayoutRatio(20, 50)`.

**TRADING.METRICS.EPS(utileNetto, azioni)** — earning per share di base: utile diviso il numero di azioni in circolazione. Esempio reale (`shared/minitests/trading/met1.bas:4`): `M.EPS(50, 100)` restituisce `0.5`.

**TRADING.METRICS.DilutedEPS(utileNetto, azioni, diluizione)** — earning per share diluito: al denominatore aggiunge le azioni potenziali (`diluizione`), quindi il valore è sempre minore o uguale a quello base. Esempio reale (`shared/minitests/trading/met1.bas:5`): `ROUND(M.DilutedEPS(50, 100, 10), 4)` su 50 di utile, 100 azioni e 10 potenziali.

**TRADING.METRICS.DPS(dividendiPagati, azioni)** — dividendo per azione: l'importo distribuito diviso il numero di azioni. Esempio reale (`shared/minitests/trading/met1.bas:6`): `M.DPS(20, 100)` restituisce `0.2`.

**TRADING.METRICS.EquityMultiplier(attivo, patrimonio)** — moltiplicatore del capitale: attivo diviso patrimonio, cioè la leva complessiva implicita nello stato patrimoniale. Esempio reale (`shared/minitests/trading/met1.bas:7`): `M.EquityMultiplier(500, 250)` restituisce `2`.

**TRADING.METRICS.NAV(attivo, passivo, azioni)** — net asset value per azione: `(attivo - passivo) / azioni`. Esempio reale (`shared/minitests/trading/met1.bas:8`): `M.NAV(1000, 200, 100)` restituisce `8`.

### Opzioni e greche — `TRADING.OPTIONS` (16 funzioni)

Il blocco prezzi usa Black-Scholes-Merton con rendimento da dividendo continuo opzionale in coda alla firma (`q`, assente o `0` per un'azione che non paga dividendi). Le greche seguono la convenzione dei desk: `flag` vale `1` per la call e `0` per la put nelle funzioni che ne hanno bisogno (`Delta`, `Theta`, `Rho`, `Charm`), mentre le greche di secondo ordine (`Gamma`, `Vanna`, `Volga`, `Zomma`, `GammaColor`, `DeltaSpeed`) non distinguono call e put perché coincidono. Tutti i tassi sono annui decimali, `T` è la vita residua in anni e `sigma` la volatilità annua.

**TRADING.OPTIONS.BsCall(prezzo, strike, scadenza, tasso, volatilita [, dividendo])** — prezzo di una call europea con la formula chiusa di Black-Scholes. Esempio reale (`shared/minitests/trading/options.bas:3`): `O.BsCall(100, 100, 1, 0.05, 0.2, 0)` su un'opzione at-the-money a un anno.

**TRADING.OPTIONS.BsPut(prezzo, strike, scadenza, tasso, volatilita [, dividendo])** — prezzo di una put europea. Deve rispettare la parità put-call: `C - P = S - K·e^(-rT)`, che è il modo più rapido per verificare che prezzo, tasso e dividendo siano coerenti. Esempio reale (`shared/minitests/trading/options.bas:4`): `O.BsPut(100, 100, 1, 0.05, 0.2, 0)`.

**TRADING.OPTIONS.MertonCall(prezzo, strike, scadenza, tasso, volatilita, dividendo)** — call con rendimento da dividendo continuo: la variante Merton di Black-Scholes, in cui il prezzo spot viene scontato al tasso di dividendo. Esempio reale (`shared/minitests/trading/mertoncall.bas:6`): `BND.MERTONCALL(100, 100, 1, 0.05, 0.2, 0)` restituisce `10.450584` (con dividendo `0` coincide con la formula base).

**TRADING.OPTIONS.MertonPut(prezzo, strike, scadenza, tasso, volatilita, dividendo)** — la corrispondente put con dividendo continuo. Esempio reale (`shared/minitests/trading/mertonput.bas:6`): `BND.MERTONPUT(100, 100, 1, 0.05, 0.2, 0)` restituisce `5.573526`.

**TRADING.OPTIONS.BinomialOption(prezzo, strike, scadenza, tasso, volatilita, passi, tipo)** — prezzo con albero binomiale a `passi` stadi e `tipo` stringa `"CALL"` o `"PUT"`. Serve per le opzioni americane e per i casi in cui la formula chiusa non basta. Esempio reale (`shared/minitests/trading/binomialoption.bas:6`): `BND.BINOMIALOPTION(100, 100, 1, 0.05, 0.2, 100, "CALL")` restituisce `10.430612`, leggermente sopra Black-Scholes perché l'albero a 100 passi approssima dal basso.

**TRADING.OPTIONS.Delta(prezzo, strike, scadenza, tasso, volatilita, flag)** — derivata del prezzo rispetto al sottostante: la probabilità risk-neutral di finire in-the-money, e il numero di azioni da coprire per essere delta-neutral. Esempio reale (`shared/minitests/trading/options.bas:5`): `O.Delta(100, 100, 1, 0.05, 0.2, 1)` su una call at-the-money.

**TRADING.OPTIONS.Gamma(prezzo, strike, scadenza, tasso, volatilita)** — derivata seconda rispetto al sottostante: la velocità con cui cambia il delta, massima quando l'opzione è at-the-money e vicina alla scadenza. Esempio reale (`shared/minitests/trading/options.bas:6`): `O.Gamma(100, 100, 1, 0.05, 0.2)`.

**TRADING.OPTIONS.Vega(prezzo, strike, scadenza, tasso, volatilita)** — sensibilità del prezzo alla volatilità implicita: di quanto cambia il premio per un punto di volatilità. Esempio reale (`shared/minitests/trading/options.bas:7`): `O.Vega(100, 100, 1, 0.05, 0.2)`.

**TRADING.OPTIONS.Theta(prezzo, strike, scadenza, tasso, volatilita, flag)** — decadimento temporale: quanto valore perde l'opzione in un giorno che passa, a tutto il resto invariato. È negativo per un'opzione lunga. Esempio reale (`shared/minitests/trading/theta.bas:6`): `BND.THETA(100, 100, 1, 0.05, 0.2, 0)` restituisce `-0.017573`.

**TRADING.OPTIONS.Rho(prezzo, strike, scadenza, tasso, volatilita, flag)** — sensibilità al tasso d'interesse: raramente decisiva per scadenze brevi, torna a contare su quelle lunghe. Esempio reale (`shared/minitests/trading/options.bas:8`): `O.Rho(100, 100, 1, 0.05, 0.2, 1)`.

**TRADING.OPTIONS.Charm(prezzo, strike, scadenza, tasso, volatilita, flag)** — derivata del delta rispetto al tempo (delta decay): dice come si muove la copertura man mano che l'opzione invecchia. Esempio reale (`shared/minitests/trading/charm.bas:6`): `BND.CHARM(100, 100, 1, 0.05, 0.2, 0)` restituisce `-0.000253`.

**TRADING.OPTIONS.Vanna(prezzo, strike, scadenza, tasso, volatilita)** — derivata incrociata di delta e volatilità (dDelta/dSigma): misura come cambia la copertura quando cambia la volatilità implicita. Esempio reale (`shared/minitests/trading/vanna.bas:6`): `BND.VANNA(100, 100, 1, 0.05, 0.2)` restituisce `-0.28143`.

**TRADING.OPTIONS.Volga(prezzo, strike, scadenza, tasso, volatilita)** — derivata seconda rispetto alla volatilità (vega convexity): quanto è convesso il premio nella volatilità. Esempio reale (`shared/minitests/trading/volga.bas:6`): `BND.VOLGA(100, 100, 1, 0.05, 0.2)` restituisce `9.850059`.

**TRADING.OPTIONS.Zomma(prezzo, strike, scadenza, tasso, volatilita)** — derivata terza rispetto al sottostante: sensibilità del gamma al movimento del prezzo, usata per raffinare la copertura di secondo ordine. Esempio reale (`shared/minitests/trading/zomma.bas:6`): `BND.ZOMMA(100, 100, 1, 0.05, 0.2)` restituisce `-0.088885`.

**TRADING.OPTIONS.GammaColor(prezzo, strike, scadenza, tasso, volatilita)** — derivata del gamma rispetto al tempo: indica come si è spostata nel tempo la convexity della posizione, e quindi quanto la copertura andrà riaggiornata. Esempio reale (`shared/minitests/trading/gammacolor.bas:6`): `BND.GAMMACOLOR(100, 100, 1, 0.05, 0.2)` restituisce `-0.025798`.

**TRADING.OPTIONS.DeltaSpeed(prezzo, strike, scadenza, tasso, volatilita)** — derivata terza del prezzo rispetto al sottostante, cioè la velocità di variazione del gamma: utile per stimare gli errori di copertura su movimenti ampi. Esempio reale (`shared/minitests/trading/deltaspeed.bas:6`): `BND.DELTASPEED(100, 100, 1, 0.05, 0.2)` restituisce `-0.000516`.

### Portafoglio e ottimizzazione — `TRADING.PORTFOLIO` (19 funzioni)

Le funzioni di ottimizzazione restituiscono tutte un record con campo `.Weights`: l'array dei pesi ottimi, che somma a uno. Gli argomenti `storico` sono matrici per attività (una riga per titolo, una colonna per periodo), `cov` è la matrice di covarianza e `riskFree` un tasso annuo decimale. Le due firme `SHARPERATIO` di questo blocco e di `TRADING.METRICS` sono intercambiabili: stesso calcolo, stesso risultato.

**TRADING.PORTFOLIO.EQUALLYWEIGHTED(attivita)** — pesi uniformi `1/n`: il portafoglio di riferimento contro cui si misura qualsiasi ottimizzazione. Esempio reale (`shared/minitests/trading.portfolio/equallyweighted.bas:12`): `DIM x = TP.EQUALLYWEIGHTED(3)` seguito da `PRINT LEN(x.Weights)` che stampa `3`.

**TRADING.PORTFOLIO.MAXRETURN(ritorni)** — concentra tutto sull'attività con il rendimento atteso più alto: la soluzione degenere del problema di ottimizzazione, utile come limite superiore. Esempio reale (`shared/minitests/trading.portfolio/maxreturn.bas:12`): `TP.MAXRETURN(r)` restituisce un record con tre pesi.

**TRADING.PORTFOLIO.MINVOLATILITY(covarianza)** — portafoglio a varianza minima: la combinazione che riduce al minimo il rischio totale senza vincoli di rendimento. Esempio reale (`shared/minitests/trading.portfolio/minvolatility.bas:12`): `TP.MINVOLATILITY(cov)` restituisce `LEN(x.Weights) = 3`.

**TRADING.PORTFOLIO.RISKPARITY(covarianza)** — pesi tali che ogni attività contribuisca in egual misura al rischio complessivo, invece che al capitale: diversificazione «a rischio uguale». Esempio reale (`shared/minitests/trading.portfolio/riskparity.bas:12`): `TP.RISKPARITY(cov)`.

**TRADING.PORTFOLIO.MAXSHARPE(ritorni, covarianza, riskFree)** — portafoglio tangente: massimizza il Sharpe ratio, cioè il rendimento in eccesso per unità di rischio. Esempio reale (`shared/minitests/trading.portfolio/maxsharpe.bas:12`): `TP.MAXSHARPE(r, cov, 0.01)`.

**TRADING.PORTFOLIO.MEANVARIANCE(ritorni, covarianza, rendimentoObiettivo)** — frontiera media-varianza di Markowitz: dato un rendimento obiettivo restituisce il portafoglio a varianza minima che lo raggiunge; con `NULL` al posto dell'obiettivo si comporta come `MINVOLATILITY`. Esempio reale (`shared/minitests/trading.portfolio/meanvariance.bas:12`): `TP.MEANVARIANCE(r, cov, NULL)`.

**TRADING.PORTFOLIO.MINCVAR(storico, confidenza, rendimentoObiettivo)** — minimizza il CVaR (perdita media in coda) invece della varianza: l'ottimizzazione preferita da chi vuole controllare le perdite estreme. Esempio reale (`shared/minitests/trading.portfolio/mincvar.bas:12`): `TP.MINCVAR(hist, 0.95, 0.01)` restituisce `LEN(x.Weights) = 1`.

**TRADING.PORTFOLIO.MINCDAR(storico, confidenza, rendimentoObiettivo)** — minimizza il CDaR, il drawdown medio condizionato: come il precedente, ma la misura di coda è il drawdown invece della perdita periodale. Esempio reale (`shared/minitests/trading.portfolio/mincdar.bas:12`): `TP.MINCDAR(hist, 0.95, 0.01)`.

**TRADING.PORTFOLIO.PVAR(storico, pesi, confidenza)** — VaR parametrico del portafoglio: combina media e covarianza dello storico con i pesi dati, assumendo normalità. A differenza del VaR storico restituisce un numero in punti percentuali. Esempio reale (`shared/minitests/trading.portfolio/pvar.bas:12`): `TP.PVAR(hist, w, 0.95)` restituisce `2`.

**TRADING.PORTFOLIO.CVAR(storico, pesi, confidenza)** — CVaR del portafoglio calcolato sullo storico, senza assumere una distribuzione: la perdita media oltre il quantile. Esempio reale (`shared/minitests/trading.portfolio/cvar.bas:12`): `TP.CVAR(hist, w, 0.95)` restituisce `0.02`.

**TRADING.PORTFOLIO.EXPECTEDRETURN(pesi, rendimentiAttesi)** — rendimento atteso del portafoglio: prodotto scalare tra pesi e rendimenti attesi delle singole attività. Esempio reale (`shared/minitests/trading.portfolio/expectedreturn.bas:12`): `TP.EXPECTEDRETURN(w, r)` restituisce `0.069`, cioè 6,9% annuo.

**TRADING.PORTFOLIO.STDEV(pesi, covarianza)** — volatilità del portafoglio: radice di `w'Σw`, il rischio totale implicito nei pesi scelti. Esempio reale (`shared/minitests/trading.portfolio/stdev.bas:12`): `TP.STDEV(w, cov)` restituisce `0.1118034`.

**TRADING.PORTFOLIO.SHARPERATIO(rendimentoPortafoglio, rischio, riskFree)** — Sharpe ratio sui valori già calcolati, identico alla voce omonima di `TRADING.METRICS`. Esempio reale (`shared/minitests/trading.portfolio/sharperatio.bas:5`): `TP.SHARPERATIO(0.15, 0.2, 0.01)` restituisce `0.7`.

**TRADING.PORTFOLIO.ISLONGONLY(pesi)** — verifica che i pesi siano tutti non negativi, cioè che il portafoglio non contenga posizioni corte. Il MiniTest di riferimento riporta `-1` sul proprio vettore di prova; la funzione è documentata ma **non è emessa dal backend C99Native**, che risponde «funzione non definita»: è un gap di backend, non un errore d'uso.

**TRADING.PORTFOLIO.PORTFOLIOMAD(ritorni)** — mean absolute deviation del portafoglio: misura di rischio alternativa alla deviazione standard, meno sensibile agli outlier. Esempio reale (`shared/minitests/trading.portfolio/portfoliomad.bas:12`): `TP.PORTFOLIOMAD(r)` restituisce `0.02666667`.

**TRADING.PORTFOLIO.PORTFOLIOSEMIMAD(ritorni [, MAR])** — la stessa misura calcolata sui soli scostamenti negativi rispetto a `MAR`: la versione semi-deviazione della MAD. Esempio reale (`shared/minitests/trading.portfolio/portfoliosemimad.bas:12`): `TP.PORTFOLIOSEMIMAD(r)` restituisce `0`, perché nella serie di prova nessun ritorno scende sotto la soglia zero.

**TRADING.PORTFOLIO.KELLYCRITERION(ritorni, riskFree)** — frazione di capitale da investire secondo il criterio di Kelly: massimizza la crescita logaritmica attesa, al prezzo di drawdown più profondi. Restituisce un record con i pesi. Esempio reale (`shared/minitests/trading.portfolio/kellycriterion.bas:12`): `TP.KELLYCRITERION(r, 0.01)` restituisce un record con un solo peso.

**TRADING.PORTFOLIO.BLACKLITTERMAN(capitalizzazioni, covarianza, delta, tau, P, Q, omega, riskFree)** — modello Black-Litterman: combina l'equilibrio di mercato con le view dell'investitore. `P` seleziona le view, `Q` ne dà i valori attesi, `omega` è la matrice di incertezza delle view; `delta` e `tau` pesano equilibrio e view. Esempio reale (`shared/minitests/trading.portfolio/blacklitterman.bas:13`): `TP.BLACKLITTERMAN(caps, cov, 2.5, 0.05, P, Q, omega, 0.01)` restituisce un record con tre pesi.

**TRADING.PORTFOLIO.EVOLVE(ritorni, covarianza, vincoli)** — ottimizzazione per evoluzione differenziale: esplora lo spazio dei pesi con una meta-euristica, utile quando la funzione obiettivo non è convessa o i vincoli non sono lineari. Esempio reale (`shared/minitests/trading.portfolio/gen_TRADING.PORTFOLIO_EVOLVE.bas:10`): `TP.EVOLVE(r, cov, NULL)`.

### Backtest — `TRADING.BACKTESTS` (1 funzione)

**TRADING.BACKTESTS.RUN(dati [, parametri] [, opzioni])** — esegue un backtest su una serie di prezzi e restituisce un record con la curva di equity e le metriche di sintesi. L'arità canonica registrata è 2–3: i primi due argomenti sono obbligatori (serie e parametri della strategia), il terzo raccoglie le opzioni di esecuzione. Nel corpus attuale non esiste un sample dedicato che la invochi con argomenti reali, quindi la voce è documentata per firma, arità e forma del risultato, non con un esempio eseguibile: è l'unico caso del dominio in questa condizione, e va verificato su un sample dedicato prima di scriverne la guida utente.

### Come leggere i risultati di questo dominio

Tre errori ricorrenti spiegano quasi tutte le risposte che sembrano sbagliate. Il primo è mescolare le unità: `DividendYield` e `PVAR` lavorano in punti percentuali, tutto il resto in decimali, quindi un risultato «100 volte troppo grande» è quasi sempre un 5% letto come 5. Il secondo è dimenticare `periodi`: una volatilità mensile passata come se fosse giornaliera cambia di un fattore `sqrt(21)`, e nessuna funzione può accorgersene. Il terzo è confrontare misure che non sono omogenee: `MaxDrawdown` è negativa per costruzione, `CalmarRatio` e `UlcerIndex` sono positive, e un `SortinoRatio` negativo non è un errore ma la prova che la serie non raggiunge la soglia `MAR`.

## Tavola sinottica del dominio

Ogni riga riporta la firma canonica (dal registry) e il primo uso reale trovato
nel corpus di test e sample `.bas`/`.inc`: dove il corpus non usa il simbolo è dichiarato.

- **BLOCKCHAIN.ABIDECODE(x1, x2)** — `DIM decoded = BLOCKCHAIN.ABIDECODE(abi_hex$, types)` (shared/samples/Crypto/CryptoFullCoverage.bas:16)
- **BLOCKCHAIN.ABIENCODE(x1)** — `DIM encodedData$ = ABIENCODE(signature$, { to$, amount$ })` (shared/samples/Crypto/CryptoERC20.bas:103)
- **BLOCKCHAIN.BASE32DECODE(x1)** — `DIM b32_decoded() = BASE32DECODE(b32_encoded$)` (shared/samples/Crypto/CryptoBlockchain.bas:74)
- **BLOCKCHAIN.BASE32ENCODE(x1)** — `DIM b32_encoded$ = BASE32ENCODE(dataBytes())` (shared/samples/Crypto/CryptoBlockchain.bas:72)
- **BLOCKCHAIN.BASE58DECODE(x1)** — `DIM b58_decoded() = BASE58DECODE("16UwLL9Risc3QfPqBUvKofHmBQ7wMtjvM")` (shared/samples/Crypto/CryptoBlockchain.bas:67)
- **BLOCKCHAIN.BASE58ENCODE(x1)** — `DIM b58_encoded$ = BASE58ENCODE(b58_source())` (shared/samples/Crypto/CryptoBlockchain.bas:64)
- **BLOCKCHAIN.BLAKE2B256(x1)** — `PRINT "BLAKE2B256(bytes):"; TOHEX(BLAKE2B256(dataBytes()))` (shared/samples/Crypto/CryptoBlockchain.bas:57)
- **BLOCKCHAIN.BLOCKDIFFICULTY(x1, x2)** — `DIM difficulty = BLOCKDIFFICULTY(diffHeader(), 486604799)` (shared/samples/Crypto/CryptoBlockchain.bas:113)
- **BLOCKCHAIN.BTCADDRFROMPUB(x1)** — `PRINT "Indirizzo Bitcoin da chiave pubblica (mainnet): "; BTCADDRFROMPUB(btcPubKeyBytes(), FALSE)` (shared/samples/Crypto/CryptoBlockchain.bas:96)
- **BLOCKCHAIN.CONTRACT(x1, x2)** — `token = BLOCKCHAIN.CONTRACT(usdtAddress$, "IERC20", rpcUrl$)` (shared/samples/Crypto/CryptoERC20.bas:57)
- **BLOCKCHAIN.DERIVEKEY(x1, x2)** — `DIM derivedKey AS DICT OF STRING, ARRAY OF BYTE = BLOCKCHAIN.DERIVEKEY(masterSeed, ethPath$)` (shared/samples/Crypto/CryptoWalletHD.bas:96)
- **BLOCKCHAIN.ETHADDRCHECKSUM(x1)** — `PRINT "Indirizzo ETH con Checksum:          "; ETHADDRCHECKSUM(ethAddress$)` (shared/samples/Crypto/CryptoBlockchain.bas:88)
- **BLOCKCHAIN.ETHADDRFROMPUB(x1)** — `DIM ethAddress$ = BLOCKCHAIN.ETHADDRFROMPUB(publicKey)` (shared/samples/Crypto/CryptoWalletHD.bas:108)
- **BLOCKCHAIN.GENERATEMNEMONIC()** — `DIM mnemonic$ = BLOCKCHAIN.GENERATEMNEMONIC(12)` (shared/samples/Crypto/CryptoWalletHD.bas:75)
- **BLOCKCHAIN.GETPUBLICKEY(x1)** — `DIM recalculatedPublicKey() AS BYTE = BLOCKCHAIN.GETPUBLICKEY(privateKey)` (shared/samples/Crypto/CryptoWalletHD.bas:114)
- **BLOCKCHAIN.KECCAK256(x1)** — `PRINT "KECCAK256(bytes):   "; TOHEX(KECCAK256(dataBytes()))` (shared/samples/Crypto/CryptoBlockchain.bas:52)
- **BLOCKCHAIN.MNEMONICTOSEED(x1)** — `DIM masterSeed() AS BYTE = BLOCKCHAIN.MNEMONICTOSEED(mnemonic$, passphrase$)` (shared/samples/Crypto/CryptoWalletHD.bas:83)
- **BLOCKCHAIN.RIPEMD160(x1)** — `PRINT "RIPEMD160(bytes): "; TOHEX(RIPEMD160(dataBytes()))` (shared/samples/Crypto/CryptoBlockchain.bas:55)
- **BLOCKCHAIN.RLPENCODE(x1)** — `DIM rlp_test() = RLPENCODE(FROMHEX("c88361626383646566"))` (shared/samples/Crypto/CryptoBlockchain.bas:103)
- **BLOCKCHAIN.SECP256K1SIGN(x1, x2)** — `DIM signature() = SECP256K1SIGN(msg_hash(), pvt_key())` (shared/samples/Crypto/CryptoBlockchain.bas:119)
- **BLOCKCHAIN.STRKECCAK256(x1)** — `PRINT "STRKECCAK256(data$):    "; STRKECCAK256(data$)` (shared/samples/Crypto/CryptoBlockchain.bas:51)
- **BLOCKCHAIN.STRRIPEMD160(x1)** — `PRINT "STRRIPEMD160(data$):   "; STRRIPEMD160(data$)` (shared/samples/Crypto/CryptoBlockchain.bas:54)
- **BLOCKCHAIN.VARINTDECODE(x1)** — `PRINT "VarInt Decode: "; VARINTDECODE(varint_test())` (shared/samples/Crypto/CryptoBlockchain.bas:109)
- **BLOCKCHAIN.VARINTENCODE(x1)** — `DIM varint_test() = VARINTENCODE(300)` (shared/samples/Crypto/CryptoBlockchain.bas:107)
- **FINANCE.CUMIPMT(x1, x2, x3, x4, x5, x6)** — `PRINT "CUMIPMT test:"; FINANCE.CUMIPMT(0.05, 360, 200000, 1, 12, 0)` (shared/samples/Finance/FinanceGeneralTests.bas:113)
- **FINANCE.CUMPRINC(x1, x2, x3, x4, x5, x6)** — `PRINT "CUMPRINC test:"; FINANCE.CUMPRINC(0.05, 360, 200000, 1, 12, 0)` (shared/samples/Finance/FinanceGeneralTests.bas:114)
- **FINANCE.DAYS360(x1, x2)** — `PRINT "Days360 (US):      "; FINANCE.DAYS360(StartD, EndD)` (shared/samples/Finance/FinanceGeneralTests.bas:109)
- **FINANCE.DB(x1, x2, x3, x4, x5)** — `PRINT "DB test:"; FINANCE.DB(10000, 1000, 5, 1, 12)` (shared/samples/Finance/FinanceGeneralTests.bas:115)
- **FINANCE.DDB(x1, x2, x3, x4, x5)** — `PRINT "Anno 1 (Double Declining):"; FINANCE.DDB(Costo, ValoreResiduo, VitaUtile, 1, 2)` (shared/samples/Finance/FinanceGeneralTests.bas:61)
- **FINANCE.FV(x1, x2, x3)** — `DIM ValFuturo = FINANCE.FV(0.07/12, 10*12, -100, 0, 0)` (shared/samples/Finance/FinanceGeneralTests.bas:42)
- **FINANCE.FVEXT(x1, x2, x3)** — `PRINT "FVEXT test:"; FINANCE.FVEXT(0.05, 360, -1000)` (shared/samples/Finance/FinanceGeneralTests.bas:116)
- **FINANCE.IRR(x1)** — `DIM IrrVal = FINANCE.IRR(Flussi)` (shared/samples/Finance/FinanceGeneralTests.bas:85)
- **FINANCE.MIRR(x1, x2, x3)** — `DIM MirrVal = FINANCE.MIRR(Flussi, 0.05, 0.08)` (shared/samples/Finance/FinanceGeneralTests.bas:97)
- **FINANCE.MNPV(x1, x2)** — `PRINT "MNPV test:"; FINANCE.MNPV(0.1, Flussi)` (shared/samples/Finance/FinanceGeneralTests.bas:117)
- **FINANCE.NPER(x1, x2, x3)** — `PRINT "NPER(0.05, -100, 1000) = "; FINANCE.NPER(0.05, -100, 1000)` (shared/samples/Finance/FinanceFullCoverage.bas:15)
- **FINANCE.NPEREXT(x1, x2, x3)** — `N = FINANCE.NPEREXT(0.005, -100, 0, 10000)` (shared/samples/Trading/FinanceExtendedFunctions.bas:40)
- **FINANCE.NPV(x1)** — `PRINT ROUND(FINANCE.NPV(0.1, 3000, 4200, 6800), 2)` (shared/minitests/finance/npv.bas:3)
- **FINANCE.PMT(x1, x2, x3)** — `DIM Rata = FINANCE.PMT(TassoAnnuo/12, Anni*12, -Importo)` (shared/samples/Finance/FinanceGeneralTests.bas:37)
- **FINANCE.PMTEXT(x1, x2, x3)** — `PMT = FINANCE.PMTEXT(RATE, 60, 10000)` (shared/samples/Trading/FinanceExtendedFunctions.bas:22)
- **FINANCE.PV(x1, x2, x3)** — `PRINT "PV(0.05, 10, 100) = "; FINANCE.PV(0.05, 10, 100)` (shared/samples/Finance/FinanceFullCoverage.bas:13)
- **FINANCE.PVEXT(x1, x2, x3)** — `PV = FINANCE.PVEXT(0.005, 60, -100)` (shared/samples/Trading/FinanceExtendedFunctions.bas:34)
- **FINANCE.RATE(x1, x2, x3)** — `DIM TassoCalc = FINANCE.RATE(3*12, -300, 10000, 0, 0) * 12` (shared/samples/Finance/FinanceGeneralTests.bas:47)
- **FINANCE.SLN(x1, x2, x3)** — `PRINT "Anno 1 (Straight Line):   "; FINANCE.SLN(Costo, ValoreResiduo, VitaUtile)` (shared/samples/Finance/FinanceGeneralTests.bas:59)
- **FINANCE.SYD(x1, x2, x3, x4)** — `PRINT "Anno 1 (Sum Years Digit): "; FINANCE.SYD(Costo, ValoreResiduo, VitaUtile, 1)` (shared/samples/Finance/FinanceGeneralTests.bas:60)
- **FINANCE.XIRR(x1, x2)** — `DIM XIrrVal = FINANCE.XIRR(Flussi, DateFlussi)` (shared/samples/Finance/FinanceGeneralTests.bas:89)
- **FINANCE.XNPV(x1, x2, x3)** — `DIM XNpvVal = FINANCE.XNPV(TassoSconto, Flussi, DateFlussi)` (shared/samples/Finance/FinanceGeneralTests.bas:93)
- **FINANCE.YEARFRAC(x1, x2)** — `PRINT "YearFrac (30/360): "; MATH.ROUND(FINANCE.YEARFRAC(StartD, EndD, 0), 4)` (shared/samples/Finance/FinanceGeneralTests.bas:108)
- **SIGNAL.BLACKMAN(x1)** — `DIM b() = SIGNAL.BLACKMAN(4)` (shared/samples/Math/SignalSciPyParity.bas:14)
- **SIGNAL.BUTTER(x1, x2, x3)** — `h = SIGNAL.BUTTER(2, wn, "low")` (shared/samples/Math/SignalFiltersSciPyParity.bas:9)
- **SIGNAL.CHEBY1(x1, x2, x3, x4)** — `h3 = SIGNAL.CHEBY1(2, 1.0, wn, "low")` (shared/samples/Math/SignalFiltersSciPyParity.bas:23)
- **SIGNAL.CHEBY2(x1, x2, x3, x4)** — `h4 = SIGNAL.CHEBY2(2, 20.0, wn, "low")` (shared/samples/Math/SignalFiltersSciPyParity.bas:26)
- **SIGNAL.CONVOLVE(x1, x2)** — `DIM c() = SIGNAL.CONVOLVE(x, k)` (shared/samples/Math/SignalSciPyParity.bas:10)
- **SIGNAL.CORRELATE(x1, x2)** — `DIM r() = SIGNAL.CORRELATE(x, k)` (shared/samples/Math/SignalSciPyParity.bas:11)
- **SIGNAL.DECIMATE(x1, x2)** — `DIM dc() = SIGNAL.DECIMATE(n, 4)` (shared/samples/Math/SciPyP1Parity.bas:29)
- **SIGNAL.ELLIP(x1, x2, x3, x4, x5)** — `h2 = SIGNAL.ELLIP(3, 1.0, 20.0, wn, "low")` (shared/samples/Math/SignalFiltersSciPyParity.bas:18)
- **SIGNAL.FILTFILT(x1, x2)** — `DIM z() = SIGNAL.FILTFILT(h, x)` (shared/samples/Math/SignalFiltersSciPyParity.bas:12)
- **SIGNAL.FINDPEAKS(x1, x2)** — `DIM pk() = SIGNAL.FINDPEAKS(sig, 1)` (shared/samples/Math/SignalIntegrateP0Parity.bas:16)
- **SIGNAL.FIRWIN(x1, x2, x3, x4)** — `DIM taps() = SIGNAL.FIRWIN(11, fc, "lowpass", "hann")` (shared/samples/Math/SignalIntegrateP0Parity.bas:10)
- **SIGNAL.FREQZ(x1, x2)** — `DIM m() = SIGNAL.FREQZ(h, 8)` (shared/samples/Math/SignalFiltersSciPyParity.bas:14)
- **SIGNAL.HAMMING(x1)** — `DIM m() = SIGNAL.HAMMING(4)` (shared/samples/Math/SignalSciPyParity.bas:13)
- **SIGNAL.HANN(x1)** — `DIM h() = SIGNAL.HANN(4)` (shared/samples/Math/SignalSciPyParity.bas:12)
- **SIGNAL.LFILTER(x1, x2)** — `DIM y() = SIGNAL.LFILTER(h, x)` (shared/samples/Math/SignalFiltersSciPyParity.bas:10)
- **SIGNAL.RESAMPLE(x1, x2)** — `DIM rs() = SIGNAL.RESAMPLE(n, 128)` (shared/samples/Math/SciPyP1Parity.bas:25)
- **SIGNAL.SAVGOLFILTER(x1, x2, x3)** — `DIM sg() = SIGNAL.SAVGOLFILTER(q, 9, 2)` (shared/samples/Math/SciPyP1Parity.bas:20)
- **SIGNAL.WELCH(x1, x2, x3)** — `w = SIGNAL.WELCH(n, 64, 1.0)` (shared/samples/Math/SciPyP1Parity.bas:13)
- **TIMESERIES.TSAUTOCORR(x1, x2)** — `DIM autoCorr = TS.TSAUTOCORR(rets, 1)` (shared/samples/TimeSeries/TimeSeriesTests.bas:100)
- **TIMESERIES.TSBANDPASS(x1, x2, x3, x4)** — `DIM bp() = TIMESERIES.TSBANDPASS(x, 0.2, 0.5, 2)` (shared/samples/Math/TimeSeriesFiltersSciPyParity.bas:11)
- **TIMESERIES.TSBANDSTOP(x1, x2, x3, x4)** — `DIM bs() = TIMESERIES.TSBANDSTOP(x, 0.2, 0.5, 2)` (shared/samples/Math/TimeSeriesFiltersSciPyParity.bas:13)
- **TIMESERIES.TSCOVARIANCEMATRIX(x1)** — `DIM cv = TIMESERIES.TSCOVARIANCEMATRIX(data)` (shared/samples/TimeSeries/TimeSeriesMatrixGaps.bas:35)
- **TIMESERIES.TSDECOMPOSE(x1, x2)** — `DIM dec = TIMESERIES.TSDECOMPOSE(s, 2)` (shared/samples/TimeSeries/TimeSeriesMatrixGaps.bas:25)
- **TIMESERIES.TSDESEASONALIZE(x1, x2)** — `DIM r() = TS.TSDESEASONALIZE(s, 2)` (shared/minitests/timeseries/gen_TIMESERIES_TSDESEASONALIZE.bas:9)
- **TIMESERIES.TSDIFFERENCE(x1, x2)** — `DIM diff() = TS.TSDIFFERENCE(Prices, 1)` (shared/samples/TimeSeries/TimeSeriesTests.bas:61)
- **TIMESERIES.TSEMA(x1, x2)** — `DIM ema() = TS.TSEMA(Prices, 3)` (shared/samples/TimeSeries/TimeSeriesTests.bas:47)
- **TIMESERIES.TSHIGHPASS(x1, x2, x3)** — `DIM hp() = TIMESERIES.TSHIGHPASS(x, 0.3, 2)` (shared/samples/Math/TimeSeriesFiltersSciPyParity.bas:9)
- **TIMESERIES.TSLOWPASS(x1, x2, x3)** — `DIM lp() = TIMESERIES.TSLOWPASS(x, 0.3, 2)` (shared/samples/Math/TimeSeriesFiltersSciPyParity.bas:7)
- **TIMESERIES.TSMA(x1, x2)** — `DIM ma() = TS.TSMA(Prices, 3)` (shared/samples/TimeSeries/TimeSeriesTests.bas:46)
- **TIMESERIES.TSMINMAX(x1)** — `DIM bounds() = TS.TSMINMAX(values)` (shared/samples/TimeSeries/TimeSeriesCommonLibNumerics.bas:14)
- **TIMESERIES.TSOUTLIERS(x1, x2)** — `DIM out = TIMESERIES.TSOUTLIERS(s, 2.0)` (shared/samples/TimeSeries/TimeSeriesMatrixGaps.bas:38)
- **TIMESERIES.TSRETURNS(x1)** — `DIM rets() = TS.TSRETURNS(Prices)` (shared/samples/TimeSeries/TimeSeriesTests.bas:60)
- **TIMESERIES.TSROLLAUTOCORR(x1, x2, x3)** — `DIM rac() = TS.TSROLLAUTOCORR(s, 3, 1)` (shared/samples/TimeSeries/TimeSeriesFullCoverage.bas:25)
- **TIMESERIES.TSROLLCORR(x1, x2, x3)** — `DIM rollCorr() = TS.TSROLLCORR(Prices, Benchmark, 5)` (shared/samples/TimeSeries/TimeSeriesTests.bas:83)
- **TIMESERIES.TSROLLCOVARIANCE(x1, x2, x3)** — `DIM rollCov() = TS.TSROLLCOVARIANCE(SerieA, SerieB, Periodo)` (shared/samples/TimeSeries/TimeSeriesAdvanced.bas:44)
- **TIMESERIES.TSROLLDELTA(x1, x2)** — `dl() = TS.TSROLLDELTA(S, 10)` (shared/samples/TimeSeries/TimeSeriesUncoveredCoverage.bas:37)
- **TIMESERIES.TSROLLKURTOSIS(x1, x2)** — `DIM kurtosis() = TS.TSROLLKURTOSIS(values, 4)` (shared/samples/TimeSeries/TimeSeriesCommonLibNumerics.bas:17)
- **TIMESERIES.TSROLLMAD(x1, x2)** — `DIM rm() = TS.TSROLLMAD(s, 3)` (shared/samples/TimeSeries/TimeSeriesFullCoverage.bas:19)
- **TIMESERIES.TSROLLMEAN(x1, x2)** — `DIM means() = TS.TSROLLMEAN(values, 3)` (shared/samples/TimeSeries/TimeSeriesCommonLibNumerics.bas:15)
- **TIMESERIES.TSROLLMEDIAN(x1, x2)** — `DIM medians() = TS.TSROLLMEDIAN(values, 3)` (shared/samples/TimeSeries/TimeSeriesCommonLibNumerics.bas:18)
- **TIMESERIES.TSROLLMINMAX(x1, x2)** — `DIM mm = TIMESERIES.TSROLLMINMAX(s, 5)` (shared/samples/TimeSeries/TimeSeriesMatrixGaps.bas:21)
- **TIMESERIES.TSROLLQUANTILE(x1, x2, x3)** — `DIM quantiles() = TS.TSROLLQUANTILE(values, 3, 0.75)` (shared/samples/TimeSeries/TimeSeriesCommonLibNumerics.bas:19)
- **TIMESERIES.TSROLLREGRESSION(x1, x2, x3)** — `DIM rr = TIMESERIES.TSROLLREGRESSION(s, s2, 5)` (shared/samples/TimeSeries/TimeSeriesMatrixGaps.bas:42)
- **TIMESERIES.TSROLLREGRESSIONSINGLE(x1, x2)** — `DIM rs = TIMESERIES.TSROLLREGRESSIONSINGLE(s, 5)` (shared/samples/TimeSeries/TimeSeriesMatrixGaps.bas:45)
- **TIMESERIES.TSROLLSHARPE(x1, x2, x3)** — `DIM sharpes() = TS.TSROLLSHARPE(returns, 3, 0.0)` (shared/samples/TimeSeries/TimeSeriesCommonLibNumerics.bas:20)
- **TIMESERIES.TSROLLSKEWNESS(x1, x2)** — `DIM skewness() = TS.TSROLLSKEWNESS(values, 3)` (shared/samples/TimeSeries/TimeSeriesCommonLibNumerics.bas:16)
- **TIMESERIES.TSROLLSORTINO(x1, x2, x3, x4)** — `DIM sortinos() = TS.TSROLLSORTINO(returns, 3, 0.0, 0.0)` (shared/samples/TimeSeries/TimeSeriesCommonLibNumerics.bas:21)
- **TIMESERIES.TSROLLSTDDEV(x1, x2)** — `DIM rollStd() = TS.TSROLLSTDDEV(Prices, 5)` (shared/samples/TimeSeries/TimeSeriesTests.bas:73)
- **TIMESERIES.TSROLLZSCORE(x1, x2)** — `DIM rollZ() = TS.TSROLLZSCORE(Prices, 5)` (shared/samples/TimeSeries/TimeSeriesTests.bas:78)
- **TIMESERIES.TSVOLATILITY(x1)** — `DIM totalVol = TS.TSVOLATILITY(rets)` (shared/samples/TimeSeries/TimeSeriesTests.bas:96)
- **TRADING.BIDASKSPREAD(x1, x2)** — `PRINT "BIDASKSPREAD: "; TRADING.BidAskSpread(99.5, 100.5)` (shared/samples/Trading/TradingFullCoverage.bas:54)
- **TRADING.CAPM(x1, x2, x3)** — `PRINT TRADING.CAPM(0, 0, 0)` (shared/minitests/trading/gen_TRADING_CAPM.bas:5)
- **TRADING.CAPITALGAINSYIELD(x1, x2)** — `PRINT "CAPITALGAINSYIELD: "; TRADING.CapitalGainsYield(100, 110)` (shared/samples/Trading/TradingFullCoverage.bas:50)
- **TRADING.GEOMETRICMEANRETURN(x1)** — `PRINT "GEOMETRICMEANRETURN: "; TRADING.GeometricMeanReturn(rets)` (shared/samples/Trading/TradingFullCoverage.bas:52)
- **TRADING.GROWTHRATE(x1, x2)** — `PRINT "GROWTHRATE: "; TRADING.GrowthRate(0.5, 0.15)` (shared/samples/Trading/TradingFullCoverage.bas:53)
- **TRADING.HOLDINGPERIODRETURN(x1)** — `PRINT "HOLDINGPERIODRETURN: "; TRADING.HoldingPeriodReturn(rets)` (shared/samples/Trading/TradingFullCoverage.bas:51)
- **TRADING.PREFERREDSTOCKVALUE(x1, x2)** — `PRINT "PreferredStockValue(5,0.08)="; TRADING.PREFERREDSTOCKVALUE(5, 0.08)` (shared/samples/Trading/TradingPortfolioCalculations.bas:98)
- **TRADING.RISKPREMIUM(x1, x2)** — `PRINT "RISKPREMIUM: "; TRADING.RiskPremium(0.08, 0.02)` (shared/samples/Trading/TradingFullCoverage.bas:49)
- **TRADING.STOCKVALUECONSTGROWTH(x1, x2, x3)** — `PRINT TRADING.STOCKVALUECONSTGROWTH(0, 0, 0)` (shared/minitests/trading/gen_TRADING_StockValueConstGrowth.bas:4)
- **TRADING.STOCKVALUEZEROGROWTH(x1, x2)** — `PRINT "StockValueZeroGrowth(5,0.08)="; TRADING.STOCKVALUEZEROGROWTH(5, 0.08)` (shared/samples/Trading/TradingPortfolioCalculations.bas:99)
- **TRADING.TAXEQUIVALENTYIELD(x1, x2)** — `PRINT "TAXEQUIVALENTYIELD: "; TRADING.TaxEquivalentYield(0.05, 0.30)` (shared/samples/Trading/TradingFullCoverage.bas:55)
- **TRADING.TOTALSTOCKRETURN(x1, x2, x3)** — `ret = TRADING.TOTALSTOCKRETURN(100.0, 120.0, 5.0)` (shared/samples/Trading/TradingCoreUncoveredCoverage.bas:24)
- **TRADING.BACKTESTS.RUN(x1, x2)** — `ag = AGENT.RUN("test", 1)` (shared/samples/cua/CuaDiagnostics.bas:50)
- **TRADING.BONDS.BONDEQUIVALENTYIELD(x1, x2, x3)** — `PRINT "BondEquivalentYield:  "; MATH.ROUND(BND.BONDEQUIVALENTYIELD(100, 95, 180), 4)` (shared/samples/Trading/TradingOptionsAndBonds.bas:104)
- **TRADING.BONDS.BONDPRICE(x1, x2, x3, x4, x5, x6)** — `DIM BondPrice = BND.BONDPRICE(Settlement, Maturity, CouponRate, MarketYield, Redemption, Frequency, …` (shared/samples/Finance/FinanceAdvanced.bas:42)
- **TRADING.BONDS.BONDYIELD(x1, x2, x3, x4, x5, x6)** — `Y = BND.BONDYIELD(CTOD("01/01/2024"), CTOD("01/01/2029"), 0.05, 98, 100, 1, 1)` (shared/samples/Trading/FinanceExtendedFunctions.bas:46)
- **TRADING.BONDS.CURRENTYIELD(x1, x2)** — `PRINT "CurrentYield:          "; MATH.ROUND(BND.CURRENTYIELD(5, 95), 4)` (shared/samples/Trading/TradingOptionsAndBonds.bas:105)
- **TRADING.BONDS.DURATION(x1, x2, x3, x4, x5, x6, x7)** — `PRINT "Duration (Macaulay): "; "(sospeso)"` (shared/samples/Finance/FinanceAdvanced.bas:48)
- **TRADING.BONDS.PRICESIMPLE(x1, x2, x3, x4, x5, x6)** — `P = BND.PRICESIMPLE(CTOD("01/01/2024"), CTOD("01/01/2025"), 0.05, 0.04, 100, 1)` (shared/samples/Trading/FinanceExtendedFunctions.bas:52)
- **TRADING.BONDS.YIELDTOMATURITY(x1, x2, x3, x4)** — `PRINT "YieldToMaturity:       "; MATH.ROUND(BND.YIELDTOMATURITY(5, 100, 95, 5), 4)` (shared/samples/Trading/TradingOptionsAndBonds.bas:106)
- **TRADING.BONDS.ZEROCOUPONBONDVALUE(x1, x2, x3)** — `PRINT "ZeroCouponBondValue:   "; MATH.ROUND(BND.ZEROCOUPONBONDVALUE(100, 0.05, 5), 4)` (shared/samples/Trading/TradingOptionsAndBonds.bas:107)
- **TRADING.BONDS.ZEROCOUPONBONDYIELD(x1, x2, x3)** — `PRINT "ZeroCouponBondYield:   "; MATH.ROUND(BND.ZEROCOUPONBONDYIELD(100, 78, 5), 4)` (shared/samples/Trading/TradingOptionsAndBonds.bas:108)
- **TRADING.METRICS.ALPHA(x1, x2, x3, x4)** — `PRINT "--- 5. Test con Canale Alpha (Trasparenza) ---"` (shared/samples/graphics/GraphicsColors.bas:114)
- **TRADING.METRICS.ANNUALIZEDDOWNSIDEDEVIATION(x1, x2, x3)** — `PRINT "ANNUALIZEDDOWNSIDEDEVIATION(R,0,12)= "; TM.ANNUALIZEDDOWNSIDEDEVIATION(R, 0, 12)` (shared/samples/Trading/TradingMetricsUncoveredCoverage.bas:60)
- **TRADING.METRICS.ANNUALIZEDRETURN(x1, x2)** — `PRINT "Annualized Return:  "; FORMAT(TM.AnnualizedReturn(Rendimenti, PeriodiAnno), "0.00%")` (shared/samples/Trading/TradingMetrics.bas:37)
- **TRADING.METRICS.ANNUALIZEDTRACKINGERROR(x1, x2, x3)** — `PRINT "ANNUALIZEDTRACKINGERROR(R,B,12)  = "; TM.ANNUALIZEDTRACKINGERROR(R, B, 12)` (shared/samples/Trading/TradingMetricsUncoveredCoverage.bas:59)
- **TRADING.METRICS.ANNUALIZEDVOLATILITY(x1, x2)** — `PRINT "Annualized Volat.:  "; FORMAT(TM.AnnualizedVolatility(Rendimenti, PeriodiAnno), "0.00%")` (shared/samples/Trading/TradingMetrics.bas:42)
- **TRADING.METRICS.BETA(x1, x2)** — `PRINT "BETA(2,3)      = "; SPECIAL.BETA(2.0, 3.0)    ' 1/12` (shared/samples/Math/SpecialFunctions.bas:22)
- **TRADING.METRICS.BOOKVALUEPERSHARE(x1, x2)** — `PRINT "TM.BookValuePerShare(1000,100)="; TM.BookValuePerShare(1000, 100)` (shared/samples/Trading/TradingMetrics.bas:69)
- **TRADING.METRICS.CALMARRATIO(x1, x2)** — `PRINT "CALMARRATIO(R, 12)               = "; TM.CALMARRATIO(R, 12)` (shared/samples/Trading/TradingMetricsUncoveredCoverage.bas:45)
- **TRADING.METRICS.CONDITIONALVALUEATRISK(x1, x2)** — `PRINT "CONDITIONALVALUEATRISK(R, 0.05)  = "; TM.CONDITIONALVALUEATRISK(R, 0.05)` (shared/samples/Trading/TradingMetricsUncoveredCoverage.bas:48)
- **TRADING.METRICS.DPS(x1, x2)** — `PRINT "TM.DPS(100,50)="; TM.DPS(100, 50)` (shared/samples/Trading/TradingMetrics.bas:75)
- **TRADING.METRICS.DILUTEDEPS(x1, x2, x3)** — `PRINT "DILUTEDEPS(1000000, 500000, 0)   = "; TM.DILUTEDEPS(1000000, 500000, 0)` (shared/samples/Trading/TradingMetricsUncoveredCoverage.bas:36)
- **TRADING.METRICS.DIVIDENDPAYOUTRATIO(x1, x2)** — `DIM dpr = TM.DIVIDENDPAYOUTRATIO(2.5, 5.0)` (shared/samples/Trading/TradingMetricsExtended.bas:15)
- **TRADING.METRICS.DIVIDENDYIELD(x1, x2)** — `PRINT "Dividend Yield (5 su 100)    = "; MATH.ROUND(TM.DividendYield(5, 100), 4)` (shared/samples/Trading/TradingEquityMetrics.bas:15)
- **TRADING.METRICS.DOWNSIDEDEVIATION(x1, x2)** — `PRINT "Downside Deviation: "; FORMAT(TM.DownsideDeviation(Rendimenti, 0), "0.00%")` (shared/samples/Trading/TradingMetrics.bas:45)
- **TRADING.METRICS.EPS(x1, x2)** — `PRINT "TM.EPS(500,100)="; TM.EPS(500, 100)` (shared/samples/Trading/TradingMetrics.bas:74)
- **TRADING.METRICS.EQUITYMULTIPLIER(x1, x2)** — `PRINT "TM.EquityMultiplier(2000,500)="; TM.EquityMultiplier(2000, 500)` (shared/samples/Trading/TradingMetrics.bas:70)
- **TRADING.METRICS.INFORMATIONRATIO(x1, x2, x3)** — `PRINT "INFORMATIONRATIO(R, B, 12)       = "; TM.INFORMATIONRATIO(R, B, 12)` (shared/samples/Trading/TradingMetricsUncoveredCoverage.bas:46)
- **TRADING.METRICS.JARQUEBERATEST(x1)** — `JB_Result = TM.JarqueBeraTest(Rendimenti)` (shared/samples/Trading/TradingMetrics.bas:62)
- **TRADING.METRICS.MAXDRAWDOWN(x1)** — `PRINT "Max Drawdown:       "; FORMAT(TM.MaxDrawdown(Rendimenti), "0.00%")` (shared/samples/Trading/TradingMetrics.bas:43)
- **TRADING.METRICS.NAV(x1, x2, x3)** — `PRINT "TM.NAV(5000,2000,100)="; TM.NAV(5000, 2000, 100)` (shared/samples/Trading/TradingMetrics.bas:71)
- **TRADING.METRICS.OMEGARATIO(x1, x2)** — `PRINT "OMEGARATIO(R, 0.0)               = "; TM.OMEGARATIO(R, 0.0)` (shared/samples/Trading/TradingMetricsUncoveredCoverage.bas:61)
- **TRADING.METRICS.PBRATIO(x1, x2)** — `DIM pb = TM.PBRATIO(50.0, 20.0)` (shared/samples/Trading/TradingMetricsExtended.bas:22)
- **TRADING.METRICS.PERATIO(x1, x2)** — `PRINT "P/E  (prezzo 10, EPS 2)      = "; MATH.ROUND(TM.PERatio(10, 2), 4)` (shared/samples/Trading/TradingEquityMetrics.bas:14)
- **TRADING.METRICS.PSRATIO(x1, x2)** — `DIM ps = TM.PSRATIO(30.0, 15.0)` (shared/samples/Trading/TradingMetricsExtended.bas:29)
- **TRADING.METRICS.RETURNSKURTOSIS(x1)** — `PRINT "TM.RETURNSKURTOSIS test:"; TM.RETURNSKURTOSIS(Rendimenti)` (shared/samples/Trading/TradingMetrics.bas:77)
- **TRADING.METRICS.RETURNSSKEWNESS(x1)** — `PRINT "RETURNSSKEWNESS(R)               = "; TM.RETURNSSKEWNESS(R)` (shared/samples/Trading/TradingMetricsUncoveredCoverage.bas:53)
- **TRADING.METRICS.SHARPERATIO(x1)** — `Sharpe = TP.SHARPERATIO(ExpRet, Risk, TassoRiskFree)` (shared/samples/Trading/TradingPortfolioCalculations.bas:60)
- **TRADING.METRICS.SORTINORATIO(x1, x2, x3)** — `PRINT "Sortino Ratio:      "; FORMAT(TM.SortinoRatio(Rendimenti, RiskFree, PeriodiAnno), "0.00")` (shared/samples/Trading/TradingMetrics.bas:50)
- **TRADING.METRICS.TOTALRETURN(x1)** — `PRINT "Total Return:       "; FORMAT(TM.TotalReturn(Rendimenti), "0.00%")` (shared/samples/Trading/TradingMetrics.bas:36)
- **TRADING.METRICS.TREYNORRATIO(x1, x2, x3, x4)** — `PRINT "TREYNORRATIO(R, B, riskFree, 12) = "; TM.TREYNORRATIO(R, B, riskFree, 12)` (shared/samples/Trading/TradingMetricsUncoveredCoverage.bas:47)
- **TRADING.METRICS.ULCERINDEX(x1)** — `PRINT "ULCERINDEX(R)                    = "; TM.ULCERINDEX(R)` (shared/samples/Trading/TradingMetricsUncoveredCoverage.bas:54)
- **TRADING.METRICS.VALUEATRISK(x1, x2)** — `PRINT "Value At Risk (95%):"; FORMAT(TM.ValueAtRisk(Rendimenti, 0.95), "0.00%")` (shared/samples/Trading/TradingMetrics.bas:44)
- **TRADING.OPTIONS.BINOMIALOPTION(x1, x2, x3, x4, x5, x6, x7)** — `PRINT "BinomialOption: "; MATH.ROUND(OPT.BINOMIALOPTION(S, K, T, r, v, 100, "CALL"), 4)` (shared/samples/Trading/TradingOptionsAndBonds.bas:82)
- **TRADING.OPTIONS.BSCALL(x1, x2, x3, x4, x5)** — `DIM CallPrice = OPT.BSCALL(S, K, T, r, v)  ' Standard Black-Scholes (5 args)` (shared/samples/Trading/TradingOptionsAndBonds.bas:48)
- **TRADING.OPTIONS.BSPUT(x1, x2, x3, x4, x5)** — `DIM PutPrice  = OPT.BSPUT(S, K, T, r, v)   ' Standard Black-Scholes (5 args)` (shared/samples/Trading/TradingOptionsAndBonds.bas:49)
- **TRADING.OPTIONS.CHARM(x1, x2, x3, x4, x5, x6)** — `PRINT "Charm (Call): "; MATH.ROUND(OPT.CHARM(S, K, T, r, v, FALSE), 4)` (shared/samples/Trading/TradingOptionsAndBonds.bas:74)
- **TRADING.OPTIONS.DELTA(x1, x2, x3, x4, x5, x6)** — `DIM d = OPT.DELTA(S, K, T, r, v, FALSE)   ' FALSE = Call option` (shared/samples/Trading/TradingOptionsAndBonds.bas:59)
- **TRADING.OPTIONS.DELTASPEED(x1, x2, x3, x4, x5)** — `PRINT "DeltaSpeed:   "; MATH.ROUND(OPT.DELTASPEED(S, K, T, r, v), 4)` (shared/samples/Trading/TradingOptionsAndBonds.bas:76)
- **TRADING.OPTIONS.GAMMA(x1, x2, x3, x4, x5)** — `PRINT "GAMMA(5)       = "; SPECIAL.GAMMA(5.0)       ' 24` (shared/samples/Math/SpecialFunctions.bas:16)
- **TRADING.OPTIONS.GAMMACOLOR(x1, x2, x3, x4, x5)** — `PRINT "GammaColor:   "; MATH.ROUND(OPT.GAMMACOLOR(S, K, T, r, v), 4)` (shared/samples/Trading/TradingOptionsAndBonds.bas:77)
- **TRADING.OPTIONS.MERTONCALL(x1, x2, x3, x4, x5)** — `DIM MertonCall = OPT.MERTONCALL(S, K, T, r, v, q)  ' Con dividend yield` (shared/samples/Trading/TradingOptionsAndBonds.bas:50)
- **TRADING.OPTIONS.MERTONPUT(x1, x2, x3, x4, x5)** — `PRINT "MertonPut:    "; MATH.ROUND(OPT.MERTONPUT(S, K, T, r, v, 0), 4)` (shared/samples/Trading/TradingOptionsAndBonds.bas:81)
- **TRADING.OPTIONS.RHO(x1, x2, x3, x4, x5, x6)** — `DIM rh = OPT.RHO(S, K, T, r, v, FALSE)    ' FALSE = Call option` (shared/samples/Trading/TradingOptionsAndBonds.bas:63)
- **TRADING.OPTIONS.THETA(x1, x2, x3, x4, x5, x6)** — `DIM th = OPT.THETA(S, K, T, r, v, FALSE)  ' FALSE = Call option` (shared/samples/Trading/TradingOptionsAndBonds.bas:61)
- **TRADING.OPTIONS.VANNA(x1, x2, x3, x4, x5)** — `PRINT "Vanna:        "; MATH.ROUND(OPT.VANNA(S, K, T, r, v), 4)` (shared/samples/Trading/TradingOptionsAndBonds.bas:72)
- **TRADING.OPTIONS.VEGA(x1, x2, x3, x4, x5)** — `DIM ve = OPT.VEGA(S, K, T, r, v)` (shared/samples/Trading/TradingOptionsAndBonds.bas:62)
- **TRADING.OPTIONS.VOLGA(x1, x2, x3, x4, x5)** — `PRINT "Volga:        "; MATH.ROUND(OPT.VOLGA(S, K, T, r, v), 4)` (shared/samples/Trading/TradingOptionsAndBonds.bas:73)
- **TRADING.OPTIONS.ZOMMA(x1, x2, x3, x4, x5)** — `PRINT "Zomma:        "; MATH.ROUND(OPT.ZOMMA(S, K, T, r, v), 4)` (shared/samples/Trading/TradingOptionsAndBonds.bas:75)
- **TRADING.PORTFOLIO.BLACKLITTERMAN(x1, x2, x3, x4, x5, x6, x7, x8)** — `PRINT "--- 6. BLACKLITTERMAN (richiede P/Q/omega: input 8x8) ---"` (shared/samples/Trading/TradingPortfolioOptimization.bas:55)
- **TRADING.PORTFOLIO.CVAR(x1, x2, x3)** — `PRINT "--- 5. PVAR e CVAR (Parametrici) ---"` (shared/samples/Trading/TradingPortfolioCalculations.bas:76)
- **TRADING.PORTFOLIO.EQUALLYWEIGHTED(x1)** — `Ottimo = OPT.EQUALLYWEIGHTED(4)` (shared/samples/Trading/TradingPortfolioOptimization.bas:53)
- **TRADING.PORTFOLIO.EVOLVE(x1, x2, x3)** — `PRINT "TRADING.PORTFOLIO — EVOLVE (Differential Evolution)"` (shared/samples/Trading/TradingPortfolioEvolve.bas:11)
- **TRADING.PORTFOLIO.EXPECTEDRETURN(x1, x2)** — `ExpRet = TP.EXPECTEDRETURN(Pesi, RendimentiAttesi)` (shared/samples/Trading/TradingPortfolioCalculations.bas:46)
- **TRADING.PORTFOLIO.ISLONGONLY(x1)** — `IsLong = TP.ISLONGONLY(Pesi)` (shared/samples/Trading/TradingPortfolioCalculations.bas:67)
- **TRADING.PORTFOLIO.KELLYCRITERION(x1, x2)** — `kc = TP.KELLYCRITERION(rets, 0.02)` (shared/samples/Trading/TradingFullCoverage.bas:29)
- **TRADING.PORTFOLIO.MAXRETURN(x1)** — `mr = TP.MAXRETURN(rets)` (shared/samples/Trading/TradingFullCoverage.bas:22)
- **TRADING.PORTFOLIO.MAXSHARPE(x1, x2, x3)** — `Ottimo = OPT.MAXSHARPE(Rendimenti, Covarianza, RiskFree)` (shared/samples/Trading/TradingPortfolioOptimization.bas:46)
- **TRADING.PORTFOLIO.MEANVARIANCE(x1, x2, x3)** — `Ottimo = OPT.MEANVARIANCE(Rendimenti, Covarianza, NULL)` (shared/samples/Trading/TradingPortfolioOptimization.bas:48)
- **TRADING.PORTFOLIO.MINCDAR(x1, x2, x3)** — `cd = TP.MINCDAR(rets, 0.95, 0.02)` (shared/samples/Trading/TradingFullCoverage.bas:43)
- **TRADING.PORTFOLIO.MINCVAR(x1, x2, x3)** — `cv = TP.MINCVAR(rets, 0.95, 0.02)` (shared/samples/Trading/TradingFullCoverage.bas:36)
- **TRADING.PORTFOLIO.MINVOLATILITY(x1)** — `Ottimo = OPT.MINVOLATILITY(Covarianza)` (shared/samples/Trading/TradingPortfolioOptimization.bas:44)
- **TRADING.PORTFOLIO.PORTFOLIOMAD(x1)** — `PRINT "PORTFOLIOMAD(R)          = "; TP.PORTFOLIOMAD(R)` (shared/samples/Trading/TradingPortfolioUncoveredCoverage.bas:30)
- **TRADING.PORTFOLIO.PORTFOLIOSEMIMAD(x1)** — `PRINT "PORTFOLIOSEMIMAD(R)      = "; TP.PORTFOLIOSEMIMAD(R)` (shared/samples/Trading/TradingPortfolioUncoveredCoverage.bas:31)
- **TRADING.PORTFOLIO.PVAR(x1, x2, x3)** — `PortVar = TP.PVAR(Storico, Pesi, 0.95)` (shared/samples/Trading/TradingPortfolioCalculations.bas:90)
- **TRADING.PORTFOLIO.RISKPARITY(x1)** — `Ottimo = OPT.RISKPARITY(Covarianza)` (shared/samples/Trading/TradingPortfolioOptimization.bas:50)
- **TRADING.PORTFOLIO.SHARPERATIO(x1, x2, x3)** — `Sharpe = TP.SHARPERATIO(ExpRet, Risk, TassoRiskFree)` (shared/samples/Trading/TradingPortfolioCalculations.bas:60)
- **TRADING.PORTFOLIO.STDEV(x1, x2)** — `PRINT "--- 2. STDEV (Volatilità) ---"` (shared/samples/Trading/TradingPortfolioCalculations.bas:51)
<!-- END DOMAIN TABLE -->

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
