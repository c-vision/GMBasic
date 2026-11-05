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

## Convenzione FFI (obbligatoria per nuove dichiarazioni)

- Gli array viaggiano come **puntatore**: la dimensione va passata **una volta**,
  subito dopo l'array che descrive (`a() AS DOUBLE, n AS LONG`). Mai una dimensione
  per ogni array quando condividono la stessa lunghezza.
- Gli array di uscita usano `out()` **piatti** con layout documentato
  (es. `PARTICLEFILTER`: `out(2*T)`, prima metà media, seconda varianza).
  Niente buffer multipli quando un unico `out()` può esprimerli.
- Niente pacchetti sintetici di parametri (`par()`) solo per aggirare il limite di
  arità: si riduce il numero di parametri o si usa un `out()` documentato.
- Le dimensioni ridondanti si eliminano nello shim quando sono deducibili
  (es. `nb` di Padé = `len(b)-1`), non nella firma BASIC.
