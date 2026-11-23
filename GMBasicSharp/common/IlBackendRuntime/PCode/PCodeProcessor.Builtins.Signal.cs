// FILE: Basic.IlBackend.PCode.PCodeProcessor.Builtins.Signal.cs
// P-CODE VM: dispatch del dominio SIGNAL — VUOTO dal 2026-11-23.
//
// Tutte le funzioni del dominio (finestre, convoluzione/correlazione, filtri IIR, FIR, picchi,
// ricampionamento, savgol, WELCH) sono nel pacchetto FFI `signal` (`shared/libs/signal` +
// `shared/include/signal.inc`): non c'è più un builtin SIGNAL.* da dispatchare qui.
//
// Il file resta come punto di aggancio del dominio (stessa partizione per dominio degli altri
// `PCodeProcessor.Builtins.<Dominio>.cs`) se in futuro servirà un builtin SIGNAL nativo.
