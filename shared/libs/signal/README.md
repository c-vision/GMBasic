# signal — lib FFI del namespace SIGNAL

Elaborazione dei segnali (`scipy.signal`) usata da IL, PCodeVM, VM/C e C99Native
tramite `IMPORT SIGNAL`: finestre, convoluzione/correlazione, design e
applicazione di filtri IIR (`butter`/`cheby1`/`cheby2`/`ellip` +
`lfilter`/`filtfilt`/`freqz`), FIR (`firwin`), picchi (`findpeaks`),
ricampionamento (`resample`/`decimate`), smoothing Savitzky-Golay.

- `src/signal_core.c` — FFT (radix-2 + Bluestein), finestre, convoluzione/
  correlazione, `welch`, `resample`.
- `src/signal_filters.c` — registro degli handle, design ZPK→(b,a), `lfilter`/
  `filtfilt`/`freqz`.
- `src/signal_fir.c` — `firwin`, `find_peaks`, `savgol_filter`, `decimate`.
- `src/signal_ffi.c` — wrapper `ofx_signal_*` (ABI C per la FFI di BASIC).
- `include/gmbsignal.h` — API pubblica.
- `build.sh` — `gmb_lib.sh build signal` → `libgmbsignal.<ext>`.

Nessuna dipendenza esterna: solo `libm` (self-contained, `§0octies`).

## Cosa NON è in questa lib

- `WELCH` resta **builtin**: ritorna un handle di matrice
  (`gmb_welch_c99` + `gmb_matrix_from_flat`) e il registro delle matrici vive
  nel runtime. La lib espone `ofx_signal_welch` (buffer piatto `rows*2`) per
  quando il ponte sarà disponibile.
- `FILTER.LOWPASS`/`HIGHPASS`/`BANDPASS`/`BANDSTOP` — dominio `FILTER.*`, restano
  nel runtime (`gmb_filter_*`).
- il comando `SIGNAL` e l'evento `ON SIGNAL` — bus con dispatch GPM
  (`gmb_signal.c`, `gmb_vm_signal.c`), non è calcolo.

## Provenienza

Sorgente canonico: **`gmbsuite/gmbsignal`** (stessa copia in
`packages/signal/0.1.0`). Formule spostate dal runtime C di GMBasic
(`gmb_filters.c`, `gmb_math.c`) con il solo cambio di prefisso `gmb_` → `gs_`:
la parità numerica con le gambe è per costruzione, verificata da 93 self-check.

## Test e documentazione

- MiniTest: `shared/minitests/signal/` (17 + `signal_all.bas`, `welch.bas` resta builtin).
- Sample: `shared/samples/Signal/SignalFiltering.bas`, `shared/samples/Signals/SignalDemo.bas`.
- Export BASIC: `shared/include/signal.inc` (namespace SIGNAL, 17 funzioni).
- Guida utente: `Guide/utente/SIGNAL-Guida-Utente.md` (vault).
