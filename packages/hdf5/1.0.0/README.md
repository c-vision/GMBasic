# gmblib-hdf5 — HDF5 per BASIC (namespace `HDF5`)

**Pacchetto** (dipendenza esterna: libhdf5), non libreria interna: HDF5 non è codice nostro e
va versionato/distribuito separatamente dai sorgenti di gmbsuite.

- wrapper: `gmbsuite/gmbhdf5` (`gmb_h5_*`, 17 funzioni) copiato in `src/`;
- shim FFI: `src/hdf5_ffi.c` (`ofx_h5_*`), handle = `LONG` (puntatore), stringhe come `STRING`;
- superficie BASIC: `hdf5.inc` → namespace **`HDF5`** (19 funzioni: le 17 + `H5SIZE` per
  dimensionare `out()` e `H5READDBL` type-agnostica);
- **relocabile e allineato al runtime**: `lib/osx-arm64/` contiene `libgmbhdf5.dylib` **e**
  `libhdf5.dylib` (install-name `@rpath/libhdf5.dylib`, rpath `@loader_path`). La dylib HDF5 è
  **la stessa build 1.14.6 che GMBasic pubblica** in `~/.gmbasic/libs` (`libhdf5.310.dylib`):
  la VM/C precarica le lib del runtime, quindi una build diversa (es. brew 2.2) risponderebbe solo
  sui backend che caricano la nostra copia → output divergenti (verificato: `H5VERSION` dava
  2.2.0 su IL/PCode/C99 e 1.14.6 su VM/C). Con la build allineata, `H5VERSION` è identica sui
  4 backend.

## Build

```bash
brew install hdf5          # header HDF5 per compilare (la lib linkata NON è quella di brew)
gmb libs ensure            # popola ~/.gmbasic/libs con la build 1.14.6 del runtime
bash build.sh              # -> lib/osx-arm64/libgmbhdf5.dylib + libhdf5.dylib (firmate)
```

`build.sh` copia `~/.gmbasic/libs/<rid>/libhdf5.310.dylib` nel pacchetto (override:
`HDF5_DYLIB=/percorso/libhdf5.dylib`) e le ri-firma entrambe: `install_name_tool` invalida la
firma e macOS uccide il caricamento di una dylib non firmata (SIGKILL). I `.dylib` non sono in
git (`.gitignore`): si rigenerano con `bash build.sh`.

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

- `libhdf5.dylib` vive nella **stessa cartella** di `libgmbhdf5.dylib` e viene risolta da dyld
  con `@rpath` + `@loader_path`: non serve dichiararla in `ffi.libs` (che indica la sola dylib
  dello shim), ma deve essere copiata insieme al pacchetto — cosa che `gmb pkg install` fa
  perché copia l'intera cartella.

- `out()` di `H5READF64`/`H5READI64`/`H5READDBL` va dimensionato ad almeno `H5SIZE(h, nome)`;
- `H5WRITEF64C` comprime con deflate+shuffle (`compress` 0..9);
- errori: `-1` argomenti/handle, dataset assente o tipo di dataset inatteso
  (es. `H5READF64` su un I64; per qualunque tipo numerico usare `H5READDBL`);
- i diagnostici testuali di HDF5 sono disattivati nello shim (`H5Eset_auto2`): l'esito si legge
  dal codice di ritorno (0 = ok, negativo = errore), l'output resta pulito;
