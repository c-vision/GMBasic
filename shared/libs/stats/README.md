# libgmbstats (FFI)

Sorgenti venduti da `gmbsuite` per ricompilare la libreria senza dipendenze esterne:

- `gmbstats-base`, `gmbstats-stats`, `gmbstats-regression`, `gmbstats-fitting`,
  `gmbstats-simulation`, `gmbstats-timeseries`, `gmbstats-multivariate`,
  `gmbstats-finance` (+ header legacy `gmbstats/include`)
- `gmbcompute`, `gmbmath-core`, `gmbmath-special`, `gmbmath-linalg`,
  `gmbmath-vector`, `gmbmath-numerical`, `gmbmath/src` (dipendenze vendute)

Build:

```
bash shared/libs/stats/build.sh
```

Produce `libgmbstats.dylib` (o `.so`). Export BASIC: `shared/include/statsx.inc`
(namespace `STATSX`). Gli altri export (`timeseries.inc`, `multivariate.inc`,
`fitting.inc`, `simulation.inc`) usano la stessa lib.
