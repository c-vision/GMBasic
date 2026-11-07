# gmblib-hdf5 — HDF5 per BASIC (namespace `HDF5`)

**Pacchetto** (dipendenza esterna: libhdf5), non libreria interna: HDF5 non è codice nostro e
va versionato/distribuito separatamente dai sorgenti di gmbsuite.

- wrapper: `gmbsuite/gmbhdf5` (`gmb_h5_*`, 17 funzioni) copiato in `src/`;
- shim FFI: `src/hdf5_ffi.c` (`ofx_h5_*`), handle = `LONG` (puntatore), stringhe come `STRING`;
- superficie BASIC: `hdf5.inc` → namespace **`HDF5`** (19 funzioni: le 17 + `H5SIZE` per
  dimensionare `out()` e `H5READDBL` type-agnostica);
- **HDF5 2.2.0 (l'ultima), isolata dal runtime**: `lib/osx-arm64/` contiene `libgmbhdf5.dylib`,
  `libhdf5.320.dylib` (HDF5 2.2.0, soname 2.x), `libsz.2.dylib` e `libaec.0.dylib`
  (dipendenze szip/aec), tutte con
  install-name `@rpath/...` e rpath `@loader_path`. La dylib HDF5 **non** usa il nome standard
  `libhdf5.dylib` di proposito: il runtime GMBasic pubblica in `~/.gmbasic/libs` una sua
  libhdf5 1.14.6 con install-name `@rpath/libhdf5.dylib` e la VM/C la precarica; dyld
  deduplica per install-name, quindi con il nome standard la VM/C riusava la 1.14.6
  (`H5VERSION` = 2.2.0 su IL/PCode/C99 e 1.14.6 su VM/C). Con `@rpath/libhdf5.320.dylib` e il
  binding a due livelli le chiamate dello shim restano legate alla **nostra** copia:
  `H5VERSION` = **2.2.0 su tutti e 4 i backend**.

## FFI: firma C fissa (`DECLARE NATIVE`)

Le funzioni di questa libreria hanno un'ABI fissa: tutte le `DECLARE` con array sono
`DECLARE NATIVE`, quindi il compilatore passa l'array come **solo puntatore** e le
dimensioni restano parametri espliciti (`n`, `rank`, `dims`). Non aggiungete le
auto-extents (`d0..dN`): il C della libreria non le riceve. Regola completa nella
guida FFI e nel manuale III.16.

## Build

```bash
brew install hdf5 libaec   # HDF5 2.2.0 + libsz (szip)
bash build.sh              # -> lib/osx-arm64/{libgmbhdf5,libhdf5.320,libsz.2,libaec.0}.dylib
```

`build.sh` copia `libhdf5.320.dylib`, `libsz.2.dylib` e `libaec.0.dylib` da brew, riscrive gli install-name in
`@rpath/...` (e i riferimenti a szip/aec dentro libhdf5), linka lo shim a `-lhdf5.320` con rpath
`@loader_path` e ri-firma le tre dylib: `install_name_tool` invalida la firma e macOS uccide il
caricamento di una dylib non firmata (SIGKILL). I `.dylib` non sono in git (`.gitignore`): si
rigenerano con `bash build.sh`.

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

- `libhdf5.320.dylib`, `libsz.2.dylib` e `libaec.0.dylib` vivono nella **stessa cartella** di
  `libgmbhdf5.dylib` e sono risolte da dyld con `@rpath` + `@loader_path`: non vanno dichiarate
  in `ffi.libs` (che indica la sola dylib dello shim), ma devono essere copiate insieme al
  pacchetto — cosa che `gmb pkg install` fa perché copia l'intera cartella.

- `out()` di `H5READF64`/`H5READI64`/`H5READDBL` va dimensionato ad almeno `H5SIZE(h, nome)`;
- `H5WRITEF64C` comprime con deflate+shuffle (`compress` 0..9);
- errori: `-1` argomenti/handle, dataset assente o tipo di dataset inatteso
  (es. `H5READF64` su un I64; per qualunque tipo numerico usare `H5READDBL`);
- i diagnostici testuali di HDF5 sono disattivati nello shim (`H5Eset_auto2`): l'esito si legge
  dal codice di ritorno (0 = ok, negativo = errore), l'output resta pulito;
