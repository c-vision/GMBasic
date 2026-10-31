NAMESPACE FFIDROPIN_DEMO

' Drop-in FFI: questo `.inc` sta ACCANTO al programma (il driver C99Native lo risolve
' dalla dir del programma) e dichiara una libreria che vive SOLO in `libs/`
' (`libvec_module` non è nella CWD) → la risoluzione passa dall'auto-discovery `libs/`.
' Nessun pacchetto REQUIRE, nessun manifest. Vedi Guide/FFI-Guida-Utente.md §9.
PUBLIC DECLARE FUNCTION demo_scale LIB "libvec_module" ALIAS "vec_scale" (a AS DOUBLE, b AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION demo_add LIB "libvec_module" ALIAS "vec_add" (a AS LONG, b AS LONG) AS LONG
PUBLIC DECLARE FUNCTION demo_prime LIB "libvec_module" ALIAS "vec_isprime" (n AS LONG) AS LONG
