# libgmbmath (FFI)

Sorgenti venduti da `gmbsuite` per ricompilare la libreria senza dipendenze esterne:

- `gmbmath-core`, `gmbmath-special` (+ AMOS), `gmbmath-linalg`,
  `gmbmath-numerical`, `gmbmath-vector`
- `gmbmath/src` legacy (`gmi_*`, `gm_*` residui: ellittiche, combinatoria, ecc.)

Il layer `gmbcompute` (superficie `gc_*`) **non** è più copiato qui: vive una volta sola in
`shared/libs/compute` ed è compilato dentro questa dylib tramite `shared-src.txt`
(`compute`). La dylib resta self-contained, ma il sorgente esiste in un posto solo.

Build:

```
bash shared/libs/math/build.sh
```

Produce `libgmbmath.dylib` (o `.so`). Export BASIC:

- `shared/include/special.inc` (namespace `SPECIAL`). Le funzioni `gm_special_*`,
  `gm_zeta*`, `gm_hypergeom_*`, `gm_lambert_w`, `gm_mittag_leffler`, `gm_struve_h`,
  `gm_kelvin_*`, `gm_parabolic_cylinder` sono quelle usate da `SPECIAL`.
- `shared/include/quat.inc` (namespace `QUAT`, 2026-11-06). I quaternioni di
  `gmbmath-vector` (`gm_quat_*`, 19 funzioni: costruzione, algebra, LERP/SLERP,
  conversione in matrice 3×3 e rotazione di un vettore) via shim `src/quat_ffi.c`
  (`ofx_quat_*`). Il quaternione viaggia per valore come `TYPE Quat`, e la matrice
  viene scambiata row-major.
- `shared/include/complex.inc` (namespace `COMPLEX`, 2026-11-06). I numeri complessi di
  `gmbmath-core` (`gm_cpx_*`, 28 funzioni: aritmetica, misure, esponenziale/log/potenza,
  trigonometriche, iperboliche e inverse) via shim `src/complex_ffi.c` (`ofx_cpx_*`).
  Il numero viaggia per valore come `TYPE Complex { RE, IM }`; le funzioni che hanno già
  un nome scalare nel linguaggio usano il prefisso `C` (CABS, CEXP, CSIN, …).
- `shared/include/xform.inc` (namespace `XFORM`, 2026-11-07). Trasformazioni 3D di
  `gmbmath-vector` (`gm_mat3_*`/`gm_mat4_*`, 18 funzioni: rotazioni, traslazione, scala,
  composizione, punto/direzione, inversa affine, matrice normale, conversioni 3x3<->4x4)
  via shim `src/xform_ffi.c` (`ofx_xf_*`). Matrici piatte row-major (9 o 16 double).
- `shared/include/spectral.inc` (namespace `SPECTRAL`, 2026-11-07). Trasformate di
  `gmbmath-numerical` (DCT/DST e inverse, DCT2, Haar, Hilbert, Z-trasformata, STFT/iSTFT)
  via shim `src/spectral_ffi.c` (`ofx_sp_*`). I filtri restano in `SIGNAL`.
- `shared/include/vec.inc` (namespace `VEC`, 2026-11-07). Le operazioni vettoriali che
  `VECTOR` non ha (`gm_vec3_*`: proiezione, componente perpendicolare, Fresnel di Schlick,
  bias, clamp/clamp01/clamp_mag, abs/sqrt/pow per componente, perpendicolare 2D) via
  shim `src/vec_ffi.c` (`ofx_vec_*`).
- `shared/include/sparse.inc` (namespace `SPARSE`, 2026-11-07). Matrici sparse di
  `gmbcompute/gc_sparse` (COO/CSR/CSC, conversioni, trasposizione) e solutori diretti di
  `gmbmath-linalg` (Cholesky e LU sparsi) via shim `src/sparse_ffi.c` (`ofx_sp_*`).
  Superficie a handle (64 slot), `FREE` per liberare.
- `shared/include/quad.inc` (namespace `QUAD`, 2026-11-07). Quadratura di
  `gmbmath-numerical` (Gauss-Kronrod con errore, Gauss-Legendre, Romberg, tanh-sinh e
  variante complement, nodi Legendre/Hermite) via shim `src/quad_ffi.c` (`ofx_quad_*`).
  Callback BASIC `(x AS DOUBLE) AS DOUBLE`.
- `shared/include/ode.inc` (namespace `ODE`, 2026-11-07). Metodi avanzati di
  `gmbmath-numerical` (Adams-Bashforth/Moulton 4, BDF2 e midpoint impliciti, Verlet,
  BVP shooting/differenze finite) via shim `src/ode_ffi.c` (`ofx_ode_*`). Callback
  `(y() AS DOUBLE, t AS DOUBLE) AS ARRAY` — array prima, tempo dopo (descrittore 148).
- `shared/include/veccalc.inc` (namespace `VECCALC`, 2026-11-07). Calcolo vettoriale di
  `gmbmath-numerical` (gradiente, divergenza, rotore, laplaciano, triedro di Frenet,
  lunghezza d'arco, integrali di linea, flusso) via shim `src/veccalc_ffi.c`
  (`ofx_vc_*`). Callback: campo scalare/vettoriale, curva, patch (descrittori 192+N).


Guide utente: `QUAT-Guida-Utente`, `COMPLEX-Guida-Utente`, `XFORM-Guida-Utente`, `SPECTRAL-Guida-Utente`, `VEC-Guida-Utente`, `SPARSE-Guida-Utente`, `QUAD-Guida-Utente`, `ODE-Guida-Utente`, `VECCALC-Guida-Utente` (vault) e capitolo V.27 del
manuale.
