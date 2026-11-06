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

Produce `libgmbmath.dylib` (o `.so`). Export BASIC: `shared/include/special.inc`
(namespace `SPECIAL`). Le funzioni `gm_special_*`, `gm_zeta*`, `gm_hypergeom_*`,
`gm_lambert_w`, `gm_mittag_leffler`, `gm_struve_h`, `gm_kelvin_*`,
`gm_parabolic_cylinder` sono quelle usate da `SPECIAL`.
