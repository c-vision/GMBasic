# SIGNAL — implementazione C# archiviata

Il namespace `SIGNAL` è migrato alla **lib FFI** `shared/libs/signal`
(lib `libgmbsignal`, C puro, self-contained): finestre, convoluzione/correlazione,
filtri IIR (`butter`/`cheby1`/`cheby2`/`ellip` + `lfilter`/`filtfilt`/`freqz`),
`firwin`, `findpeaks`, `resample`/`decimate`, `savgolfilter`.

Qui è conservata l'implementazione C# storica, NON più compilata:

- `CommonLib-Numerics-Signal.cs` — era `CommonLib/Numerics/Signal.cs`
  (convoluzione/correlazione 1D, finestre hann/hamming/blackman, `Firwin`,
  `FindPeaks`). Era il nucleo managed chiamato da IL/PCode e dai test
  `SciPySignalTests`.

Restano invece **nel runtime**, perché servono ancora a superfici vive:

- `CommonLib/Numerics/SciPyP1.cs` — `Welch`: `SIGNAL.WELCH` resta **builtin**
  perché ritorna un handle di matrice (registro del runtime);
- `CommonLib/Numerics/SignalFilters.cs` — `Butter`/`Cheby1`/`Cheby2`/`Ellip`/
  `LFilter`/`FiltFilt`/`Freqz`: usati dai wrapper **TIMESERIES** su IL
  (`IlRuntime.TimeSeriesLowPass` e soci).

Il comando `SIGNAL` e l'evento `ON SIGNAL` (bus con dispatch GPM) restano
builtin: `Parser.Stmt.Signal.cs`, `Ast/Signal.cs`, `IlRuntime.Signal.cs`,
`PCodeProcessor.Signal.cs`, `gmb_runtime_c/src/gmb_signal.c`,
`gmb_vm_c/gmb_vm_signal.c`.

I test di parità sono i MiniTest del dominio (`shared/minitests/signal/`, che
usano `IMPORT SIGNAL` e quindi la lib) e il self-test C del canonico
(`gmbsuite/gmbsignal`, 93 check sugli stessi oracoli).
