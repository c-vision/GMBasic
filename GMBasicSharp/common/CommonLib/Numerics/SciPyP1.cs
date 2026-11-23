// FILE: CommonLib.Numerics.SciPyP1.cs
// P1 scipy — VUOTO dal 2026-11-23.
//
// Tutte le funzioni del livello P1 sono state spostate nei pacchetti FFI:
//   - scipy.signal / scipy.fft  → SIGNAL (pacchetto `signal`: welch, savgol, resample, decimate,
//                                 rfft, irfft);
//   - scipy.interpolate         → INTERP (pacchetto `math`: splrep, splev con Thomas O(n));
//   - spatial.distance/ConvexHull → SPATIAL (pacchetto `spatial`).
// Il kernel C resta l'unica fonte (`gmb_welch`, `gs_rfft`, `gmi_splrep`, …): qui non c'è più
// una copia managed da tenere allineata.
//
// Il file resta come punto di aggancio del livello P1 nella cartella Numerics.
