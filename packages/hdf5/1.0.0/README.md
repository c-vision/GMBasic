# gmblib-hdf5 — HDF5 per BASIC (namespace `HDF5`)

**Pacchetto** (dipendenza esterna: libhdf5), non libreria interna: HDF5 non è codice nostro e
va versionato/distribuito separatamente dai sorgenti di gmbsuite.

- wrapper: `gmbsuite/gmbhdf5` (`gmb_h5_*`, 17 funzioni) copiato in `src/`;
- shim FFI: `src/hdf5_ffi.c` (`ofx_h5_*`), handle = `LONG` (puntatore), stringhe come `STRING`;
- superficie BASIC: `hdf5.inc` → namespace **`HDF5`** (19 funzioni: le 17 + `H5SIZE` per
  dimensionare `out()` e `H5READDBL` type-agnostica);
- **relocabile**: `lib/osx-arm64/` contiene `libgmbhdf5.dylib` **e** `libhdf5.dylib` (copiata da
  brew con install-name `@rpath/libhdf5.dylib`, rpath `@loader_path`): il pacchetto non dipende
  dal percorso di brew a runtime.

## Build

```bash
brew install hdf5          # prerequisito: header + libhdf5
bash build.sh              # -> lib/osx-arm64/libgmbhdf5.dylib (+ libhdf5.dylib)
```

## Installazione

```bash
gmb pkg install hdf5       # dal registry, oppure copia della cartella in ~/.gmbasic/packages/hdf5/1.0.0
```

```basic
REQUIRE "hdf5"
IMPORT HDF5

h = HDF5.H5CREATE("serie.h5")
DIM d(1) AS LONG : d(0)=2 : d(1)=3
DIM v(5) AS DOUBLE
FOR i = 0 TO 5 : v(i) = i : NEXT i
ok = HDF5.H5WRITEF64(h, "serie", 2, d, v)
HDF5.H5CLOSE(h)
```

## Note

- `out()` di `H5READF64`/`H5READI64`/`H5READDBL` va dimensionato ad almeno `H5SIZE(h, nome)`;
- `H5WRITEF64C` comprime con deflate+shuffle (`compress` 0..9);
- errori: `-1` argomenti/handle, `-2` tipo di dataset inatteso (es. `H5READF64` su un I64).
