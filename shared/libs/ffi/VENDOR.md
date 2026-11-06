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
