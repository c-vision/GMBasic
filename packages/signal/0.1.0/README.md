# signal (0.1.0)

Dominio **SIGNAL** (elaborazione dei segnali, `scipy.signal`) come pacchetto FFI:
le funzioni stanno in una dylib (`libgmbsignal`) invece che nel compilatore.

| gruppo | funzioni |
|---|---|
| finestre | `HANN`, `HAMMING`, `BLACKMAN` |
| convoluzione | `CONVOLVE`, `CORRELATE` (risultato "full", `nx+nk-1`) |
| filtri IIR | `BUTTER`, `CHEBY1`, `CHEBY2`, `ELLIP` → handle; `LFILTER`, `FILTFILT`, `FREQZ` |
| FIR e picchi | `FIRWIN`, `FINDPEAKS` |
| ricampionamento | `RESAMPLE` (FFT), `DECIMATE` (filtro + decimazione) |
| smoothing | `SAVGOLFILTER` |
| spettro | `WELCH` (densità spettrale di potenza) |

Nessuna dipendenza esterna: solo `libm`. Il dominio è puro calcolo, quindi la lib
è self-contained e il pacchetto non richiede componenti di sistema.

## Copia canonica

I sorgenti canonici vivono in **`gmbsuite/gmbsignal`**; qui c'è la copia
self-contained (`src/`, `include/`) usata dal pacchetto. Le due copie devono
restare **identiche** (confronto byte per byte): il canonico è uno solo.
Self-test dei vettori noti (nel canonico): `cd ~/Documents/Projects/gmbsuite/gmbsignal && make test`
(93 check).

## Build

```bash
bash build.sh          # produce lib/<rid>/libgmbsignal.dylib (o .so su Linux)
```

## `signal.inc` — firme pubbliche preservate

Le `DECLARE` non possono ritornare un array, quindi le primitive `SXF_*`
scrivono in un buffer del chiamante e ritornano il numero di elementi
(`<0` = errore, `-2` = buffer piccolo). I wrapper `PUBLIC` allocano il buffer
della dimensione giusta e ritornano `AS ARRAY OF DOUBLE`: la firma pubblica
resta quella storica, così i MiniTest del dominio non vanno riscritti (basta
aggiungere `REQUIRE "signal"`).

```basic
REQUIRE "signal"
IMPORT SIGNAL

DIM wn(0) AS DOUBLE = {0.3}
DIM x(4) AS DOUBLE = {1, 2, 3, 4, 5}
DIM h = SIGNAL.BUTTER(2, wn, "low")
DIM y() = SIGNAL.LFILTER(h, x)
```

| funzione | firma |
|---|---|
| `HANN` / `HAMMING` / `BLACKMAN` | `(n AS DOUBLE) AS ARRAY OF DOUBLE` |
| `CONVOLVE` / `CORRELATE` | `(x() AS DOUBLE, k() AS DOUBLE) AS ARRAY OF DOUBLE` |
| `RESAMPLE` | `(x() AS DOUBLE, num AS LONG) AS ARRAY OF DOUBLE` |
| `DECIMATE` | `(x() AS DOUBLE, q AS LONG) AS ARRAY OF DOUBLE` |
| `BUTTER` | `(n AS DOUBLE, wn() AS DOUBLE, btype AS STRING) AS LONG` |
| `CHEBY1` / `CHEBY2` | `(n AS DOUBLE, rp|rs AS DOUBLE, wn() AS DOUBLE, btype AS STRING) AS LONG` |
| `ELLIP` | `(n AS DOUBLE, rp AS DOUBLE, rs AS DOUBLE, wn() AS DOUBLE, btype AS STRING) AS LONG` |
| `LFILTER` / `FILTFILT` | `(handle AS LONG, x() AS DOUBLE) AS ARRAY OF DOUBLE` |
| `FREQZ` | `(handle AS LONG, n AS DOUBLE) AS ARRAY OF DOUBLE` |
| `FIRWIN` | `(numtaps AS LONG, cutoff() AS DOUBLE, btype AS STRING, window AS STRING) AS ARRAY OF DOUBLE` |
| `FINDPEAKS` | `(x() AS DOUBLE, distance AS DOUBLE) AS ARRAY OF DOUBLE` |
| `SAVGOLFILTER` | `(x() AS DOUBLE, window_length AS LONG, polyorder AS LONG) AS ARRAY OF DOUBLE` |
| `WELCH` | `(x() AS DOUBLE, nperseg AS LONG, fs AS DOUBLE) AS ARRAY OF DOUBLE` |
| `FILTERCOUNT` | `() AS LONG` (diagnostica) |

**Unica differenza dichiarata — `WELCH`.** Il builtin ritorna un *handle di
matrice* (`nfreq` righe × 2 colonne: frequenza, PSD); la lib produce il buffer
piatto e il wrapper ritorna l'array `[freq0, psd0, freq1, psd1, ...]` di
`2*nfreq` valori (`nfreq = nperseg/2 + 1`, con `nperseg` ridotto a `n` se più
grande). Trasformarlo in handle matrice richiede il registro del runtime, che
vive nel compilatore.

## Stato: `signal.inc` pronto, attivazione bloccata dal builtin

`signal.inc` dichiara il namespace **SIGNAL** con le 18 firme storiche. Il file
viene letto e parsato, ma `REQUIRE "signal"` è respinto dal linker:

```
la procedura 'HANN' nel namespace 'SIGNAL' ridefinisce il builtin esistente
```

Finché le 18 procedure sono builtin (TokenKind `Signal*`, `StdLib.Signal.cs`,
`IlRuntime.Signal.cs`, `PCodeProcessor.Builtins.Signal.cs`,
`gmb_vm_c/gmb_vm_signal.c`, `gmb_c99_emit*.c`), una `DECLARE` con lo stesso nome
e la stessa arità non può esistere. L'attivazione richiede la rimozione della
superficie builtin (F3): è una modifica a `GMBasicSharp/` e `GMBasic/native/`
che va autorizzata. Stessa condizione del pacchetto `crypto`.

Superficie verificata a livello C: **93 self-check** nel canonico (oracoli dei
MiniTest del dominio). Il sample `samples/signal_suite.bas` usa tutte le 18
funzioni e porta gli stessi valori attesi (`EXPECT:`), pronto per la prima
esecuzione dopo l'attivazione.

## Non sono in questo pacchetto

- `FILTER.LOWPASS`/`HIGHPASS`/`BANDPASS`/`BANDSTOP` — dominio `FILTER.*`, restano
  nel runtime C (`gmb_filter_*`);
- il comando `SIGNAL` e l'evento `ON SIGNAL` — bus eventi con dispatch GPM, non
  è calcolo e resta builtin;
- i ponti verso gli handle di matrice (`gmb_welch_c99`,
  `gmb_matrix_from_flat`) — agganci del backend.
