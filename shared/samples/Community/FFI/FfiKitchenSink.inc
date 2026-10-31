NAMESPACE FFIKITCHEN

' FFI "kitchen sink": dichiarazioni che coprono OGNI forma del contratto FFI
' (FUNCTION/SUB, LIB, ALIAS, tipi DOUBLE/LONG/INTEGER, arità 0/1/2/4/8, array
' BYREF, STRING param+ritorno).
' La libreria vive in `libs/libvec_module.dylib` (auto-discovery libs/), NON accanto
' al programma → esercita anche la risoluzione drop-in. Vedi Guide/FFI-Guida-Utente.md.
PUBLIC DECLARE FUNCTION k_scale LIB "libvec_module" ALIAS "vec_scale" (a AS DOUBLE, b AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION k_add LIB "libvec_module" ALIAS "vec_add" (a AS LONG, b AS LONG) AS LONG
PUBLIC DECLARE FUNCTION k_prime LIB "libvec_module" ALIAS "vec_isprime" (n AS INTEGER) AS INTEGER
PUBLIC DECLARE SUB k_reset LIB "libvec_module" ALIAS "vec_reset" ()
PUBLIC DECLARE SUB k_bump LIB "libvec_module" ALIAS "vec_bump" (v AS DOUBLE)
PUBLIC DECLARE FUNCTION k_tick LIB "libvec_module" ALIAS "vec_tick" () AS LONG
PUBLIC DECLARE FUNCTION k_avg4 LIB "libvec_module" ALIAS "vec_avg4" (a AS DOUBLE, b AS DOUBLE, c AS DOUBLE, d AS DOUBLE) AS DOUBLE
PUBLIC DECLARE FUNCTION k_sum8 LIB "libvec_module" ALIAS "vec_sum8" (a AS LONG, b AS LONG, c AS LONG, d AS LONG, e AS LONG, f AS LONG, g AS LONG, h AS LONG) AS LONG

' --- Array BYREF (2026-09-28): il parametro array viaggia come puntatore nativo, nessun
' bound auto-iniettato (il chiamante passa la lunghezza come parametro AS LONG separato,
' come farebbe con l'API C reale) — vedi Guide/FFI-Guida-Utente.md §4bis.
PUBLIC DECLARE SUB k_fill LIB "libvec_module" ALIAS "vec_fill" (buf() AS DOUBLE, n AS LONG)
PUBLIC DECLARE FUNCTION k_isum LIB "libvec_module" ALIAS "vec_isum" (buf() AS LONG, n AS LONG) AS LONG
PUBLIC DECLARE SUB k_combine LIB "libvec_module" ALIAS "vec_combine" (n AS LONG, a() AS DOUBLE, b() AS DOUBLE, c() AS DOUBLE)

' --- STRING (2026-09-28): parametro + ritorno, entrambi puntatori (const char*) — firma
' INT64 come gli array. Vedi Guide/FFI-Guida-Utente.md §4bis.
PUBLIC DECLARE FUNCTION k_label LIB "libvec_module" ALIAS "vec_label" (id AS LONG, name AS STRING) AS STRING

' --- BYREF scalare (2026-09-28): puntatore a un SINGOLO valore ("array di lunghezza 1"),
' non un buffer — pattern C classico div/mod (elemento LONG) e scala-in-place (elemento
' DOUBLE, IN/OUT). Vedi Guide/FFI-Guida-Utente.md §1bis.
PUBLIC DECLARE FUNCTION k_divmod LIB "libvec_module" ALIAS "vec_divmod" (a AS LONG, b AS LONG, BYREF r AS LONG) AS LONG
PUBLIC DECLARE SUB k_scale_ref LIB "libvec_module" ALIAS "vec_scale_ref" (n AS LONG, BYREF pv AS DOUBLE)
