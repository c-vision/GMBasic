' FILE: samples/FFI/FfiKitchenSink.bas
' ======================================================================================================================
' FFI — KITCHEN SINK: ogni forma del contratto, su tutte e 5 le gambe
' ======================================================================================================================
' Copre: NAMESPACE + PUBLIC DECLARE (.inc accanto al programma), IMPORT / IMPORT AS,
' DECLARE FUNCTION / DECLARE SUB, ALIAS, tipi DOUBLE/LONG/INTEGER, arità 0/1/2/4/8,
' array BYREF (output/input/multiplo — §4bis), STRING param+ritorno (§4bis), e la
' risoluzione drop-in `libs/` (la dylib NON è accanto al programma).
'
' Prima:  make   (in samples/FFI)  -> libs/libvec_module.dylib
' Poi:    esegui questo .bas.
' Guida:  Guide/FFI-Guida-Utente.md (§1-§9).
' ======================================================================================================================

IMPORT FFIKITCHEN
IMPORT FFIKITCHEN AS K

SUB Main()
    PRINT "=== FFI kitchen sink (4 backend) ==="

    ' --- FUNCTION DOUBLE, 2 argomenti (contratto all-double) ---
    PRINT "k_scale(3, 4)      = "; k_scale(3, 4)                 ' 12
    PRINT "K.k_scale(1.5, 4)  = "; K.k_scale(1.5, 4)             ' 6

    ' --- FUNCTION LONG con ALIAS, 2 argomenti (contratto int64) ---
    PRINT "K.k_add(21, 21)    = "; K.k_add(21, 21)               ' 42

    ' --- FUNCTION INTEGER (marshallato int64) ---
    PRINT "k_prime(17)        = "; k_prime(17)                   ' 1

    ' --- SUB zero-arg + SUB 1-arg + FUNCTION zero-arg (stato condiviso) ---
    k_reset()
    k_bump(1)
    k_bump(2)
    PRINT "k_tick()           = "; k_tick()                      ' 2

    ' --- Multi-argomento: 4 DOUBLE e 8 LONG ---
    PRINT "k_avg4(2,4,6,8)    = "; k_avg4(2, 4, 6, 8)            ' 5
    PRINT "k_sum8(1..8)       = "; k_sum8(1, 2, 3, 4, 5, 6, 7, 8) ' 36

    PRINT
    PRINT "--- Array BYREF: pipeline buffer (output / scalare-per-elemento / multiplo / input) ---"

    ' 1) Array BYREF OUTPUT (CopyBuffer-style): il nativo riempie il buffer del chiamante.
    ' Nessun bound auto-iniettato — n e' un parametro AS LONG separato passato esplicitamente.
    DIM raw(5) AS DOUBLE
    k_fill(raw)                              ' raw = 0, 10, 20, 30, 40

    ' 2) Costruisce due buffer derivati chiamando una FUNCTION scalare elemento-per-elemento
    ' (combina il contratto scalare gia' esistente con l'array appena riempito).
    DIM scaledA(5) AS DOUBLE
    DIM scaledB(5) AS DOUBLE
    DIM i AS LONG
    FOR i = 0 TO 4
        scaledA(i) = k_scale(raw(i), 0.1)       ' 0, 1, 2, 3, 4
        scaledB(i) = k_avg4(raw(i), raw(i), raw(i), raw(i) + 10) ' media (raw,raw,raw,raw+10)
    NEXT i

    ' 3) Array BYREF MULTIPLO: 3 buffer nella stessa chiamata (n e' il primo parametro,
    ' scalare) — combined(i) = scaledA(i) + scaledB(i).
    DIM combined(5) AS DOUBLE
    k_combine(scaledA, scaledB, combined)

    PRINT "raw       = "; raw(0); raw(1); raw(2); raw(3); raw(4)           ' 0 10 20 30 40
    PRINT "scaledA   = "; scaledA(0); scaledA(1); scaledA(2); scaledA(3); scaledA(4) ' 0 1 2 3 4
    PRINT "scaledB   = "; scaledB(0); scaledB(1); scaledB(2); scaledB(3); scaledB(4) ' 2.5 12.5 22.5 32.5 42.5
    PRINT "combined  = "; combined(0); combined(1); combined(2); combined(3); combined(4) ' 2.5 13.5 24.5 35.5 46.5

    ' 4) Array BYREF INPUT, elemento LONG, dentro una FUNCTION con ritorno: somma i valori
    ' interi (CLNG half-to-even) del buffer combinato — chiude la pipeline in un unico scalare.
    DIM totals(5) AS LONG
    FOR i = 0 TO 4
        totals(i) = CLNG(combined(i))
    NEXT i
    DIM total AS LONG
    total = k_isum(totals)
    PRINT "k_isum(totals)  = "; total          ' 122 (CLNG half-to-even: 2+14+24+36+46)

    PRINT
    PRINT "--- STRING: parametro + ritorno (puntatore, firma INT64 come gli array) ---"

    ' 5) STRING: chiude la pipeline etichettando il risultato — id (LONG) + name (STRING) in
    ' ingresso, STRING in ritorno (copy-out: il modulo riusa un buffer statico interno).
    DIM label1$ = k_label(total, "kitchen-sink")
    PRINT "k_label(total, kitchen-sink) = "; label1$        ' "122:kitchen-sink"
    ' Verifica copy-out: la chiamata successiva non deve alterare label1$.
    DIM label2$ = k_label(0, "altro")
    PRINT "Dopo k_label(0, altro): label1$ ancora = "; label1$   ' invariata

    PRINT
    PRINT "--- BYREF scalare: puntatore a un singolo valore (non un buffer) ---"

    ' 6) BYREF scalare elemento LONG: scompone il totale in quoziente (ritorno) e resto
    ' (OUT) — pattern C classico div/mod.
    DIM q AS LONG
    DIM remainder AS LONG
    q = k_divmod(total, 7, remainder)
    PRINT "k_divmod(total, 7) = "; q; "resto"; remainder      ' 122/7 = 17 resto 3

    ' 7) BYREF scalare elemento DOUBLE, IN/OUT: scala un valore del chiamante in place
    ' (nessun ritorno — il risultato è SOLO nel parametro BYREF).
    DIM scaledTotal AS DOUBLE
    scaledTotal = total
    k_scale_ref(q, scaledTotal)
    PRINT "k_scale_ref(q, total) = "; scaledTotal             ' 122 * 17 = 2074

    PRINT
    PRINT "FfiKitchenSink completato."
END SUB
