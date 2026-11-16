# libffi — copia vendorizzata

- **Versione**: 3.5.2 (release ufficiale, tarball `libffi-3.5.2.tar.gz`).
- **Data**: 2026-11-06.
- **Licenza**: MIT-like (vedi `LICENSE`), compatibile con l'uso nel progetto.
- **Perché vendorizzata**: il motore FFI della VM/C usa `ffi_prep_cif`/`ffi_call`; tenerla nel
  repo evita dipendenze di sistema e differenze fra piattaforme. La 3.5.2 è la prima che
  compila con il clang recente su macOS arm64 (la 3.4.6 falliva con "invalid CFI advance_loc").
- **Build**: `bash shared/libs/ffi/build.sh` → `out/lib/libffi.a` + `out/include/ffi.h`.
- **Uso**: `gmb_vm_c` preferisce `out/lib/libffi.a`; se assente ricade sulla lib di sistema
  (`find_library(ffi)`).
- **Aggiornamento**: scaricare il tarball della nuova release, sostituire i sorgenti (tranne
  `build.sh`, `VENDOR.md`, `out/`), rieseguire il build e la suite FFI.

## Cosa NON è stato copiato (2026-11-16)

- **`testsuite/libffi.bhaible/`** (7 file) è stato **rimosso**: è la suite di test upstream di
  Bruno Haible (© 1993 Bill Triggs, © 1995-2017 Bruno Haible) distribuita sotto **GPLv3+**. Non
  entra in `libffi.a` (la libreria si compila da `src/`) e non viene mai eseguita dal nostro build:
  `build.sh` fa `configure --disable-shared --enable-static` + `make` + `make install`, **non**
  `make check`. Tenerla significava distribuire codice GPL dentro un repository PUL senza
  dichiararlo; tolta, il repo è coerente con `NOTICE`.
  - **Verificato**: senza quella cartella `build.sh` produce `out/lib/libffi.a` (55.904 B) con
    `ffi_call` esportato — 8 secondi, nessun errore. Restano in `testsuite/` le altre suite (MIT),
    ma `make check` per il sottoinsieme bhaible non è più disponibile (non lo usiamo).
  - Se un giorno servisse quella suite: si ri-estrae dal tarball upstream `libffi-3.5.2.tar.gz` e si
    tratta come materiale **GPLv3+** a sé (non ricopiarla nel repo senza dichiararla).
