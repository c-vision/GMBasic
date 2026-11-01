' FILE: samples/FFI/FfiDropinLibs.bas
' ======================================================================================================================
' FFI DROP-IN — `include/` + `libs/` senza pacchetto REQUIRE
' ======================================================================================================================
' Mostra l'UX drop-in: `FfiDropin.inc` (accanto al programma) dichiara `LIB "libvec_module"`,
' ma la dylib NON sta accanto al programma: vive in `libs/libvec_module.dylib` e viene
' risolta dall'auto-discovery `libs/` (walk-up dal CWD) su tutte e 5 le gambe.
' Nessun manifest, nessun REQUIRE.
'
' Prima:  make   (in samples/FFI)  -> libs/libvec_module.dylib
' Poi:    esegui questo .bas.
' Guida:  Guide/FFI-Guida-Utente.md §9 (Ordine di ricerca delle librerie).
' ======================================================================================================================

IMPORT FFIDROPIN_DEMO

SUB Main()
    PRINT "║ FFI drop-in: include/ + libs/ (auto-discovery) ║"
    PRINT
    PRINT "demo_scale(3.0, 4.0) = "; demo_scale(3.0, 4.0)   ' 12
    PRINT "demo_add(21, 21)     = "; demo_add(21, 21)       ' 42
    PRINT "demo_prime(17)       = "; demo_prime(17)         ' 1
    PRINT
    PRINT "Test FfiDropinLibs completato."
END SUB
