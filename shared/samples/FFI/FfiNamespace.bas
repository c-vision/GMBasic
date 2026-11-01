' FILE: samples/FFI/FfiNamespace.bas
' ======================================================================================================================
' FFI DENTRO NAMESPACE — DECLARE ... LIB importabile via IMPORT
' ======================================================================================================================
' UNITED DELLE DICHIARAZIONI FFI in un NAMESPACE (FfiNamespaceLib.inc), importate
' qui in ENTRAMBE le modalità:
'   1. IMPORT completo      : IMPORT FfiNamespaceLib            -> simboli non qualificati
'   2. IMPORT con ALIAS     : IMPORT FfiNamespaceLib AS FFL      -> simboli qualificati FFL.
' Mostra: DOUBLE (all-double), LONG omogeneo (vec_add), ALIAS (nome BASIC != simbolo C),
' loop sui primi (vec_isprime), SUB void (vec_bump) + FUNCTION long (vec_tick).
'
' Prima:  make   (in samples/FFI/NativeVec)  -> libvec_module.dylib
' Poi:    esegui questo .bas.
' ======================================================================================================================

IMPORT FfiNamespaceLib AS FFL

' Seconda modalità IMPORT: FfiNamespaceLib (non qualificato) per accedere a vec_scale /
' vec_bump / vec_tick senza prefisso.
IMPORT FfiNamespaceLib

SUB Main()
    PRINT "║ FfiNamespace - FFI in NAMESPACE (IMPORT / IMPORT AS) ║"
    PRINT

    ' --- DOUBLE (contratto all-double) prefix + qualificato ---
    PRINT "vec_scale(3.0, 4.0)          = "; vec_scale(3.0, 4.0)      ' 12
    PRINT "FFL.vec_scale(1.5, 4.0)      = "; FFL.vec_scale(1.5, 4.0)   ' 6

    ' --- LONG omogeneo (OP_CALL_FFI_T / wrapper int64) + ALIAS ---
    PRINT "FFL.ns_add(21, 21)           = "; FFL.ns_add(21, 21)        ' 42  (ALIAS vec_add)

    ' --- Loop sui primi (FUNCTION long) ---
    PRINT "Primi tra 2 e 20:"
    FOR i = 2 TO 20
        IF FFL.ns_prime(i) THEN PRINT i
    NEXT i

    ' --- SUB void + FUNCTION long (stato condiviso: hit counter) ---
    PRINT "vec_bump x3 ..."
    vec_bump(1)
    vec_bump(2)
    vec_bump(3)
    PRINT "vec_tick()                    = "; vec_tick()               ' 3

    PRINT
    PRINT "Test FfiNamespace completato."
END SUB
