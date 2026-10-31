' FILE: FFI/NativeVec/NativeVec.bas
' =================================================================
' NativeVec — esempio FFI livello 1 (c-abi) con modulo nativo gmb_ffi.h
'
' Prima:  make                    (in samples/Community/FFI/NativeVec)
'         produce libvec_module.dylib (.so su Linux)
'
' Poi:    esegui questo .bas. Il runtime trova la libreria accanto
'         al sorgente (DECLARE ... LIB "./libvec_module").
'
' Contratto FFI (DECLARE ... LIB, copy-in/copy-out — Guide/FFI-Guida-Utente.md):
'   - le STRING ritornate dal modulo vengono COPIE dal runtime (mai UAF), §4bis;
'   - il BYREF SCALARE (es. `vec_divmod(a, b, BYREF r)`) è ammesso dal 2026-09-28
'     per DOUBLE/LONG/QUAD/ULONG/UQUAD verso una variabile semplice — viaggia come
'     puntatore a un singolo valore, §1bis. `vec_split` (multi-out DOUBLE + scalare
'     DOUBLE puro nella stessa firma) resta NON chiamabile: non per il BYREF in sé,
'     ma perché mescola un DOUBLE scalare con un puntatore, vietato dalla regola di
'     uniformità del kind (§1/§4bis) — un multi-out di questo tipo va comunque
'     espresso come 2 FUNCTION separate o un handle (vedi bug doc, punto 2).
'   - Struct/UDT per VALORE (§2bis, 2026-09-29): un TYPE semplice (nessun operatore/
'     costruttore/metodo) con 1-2 campi dello stesso kind (tutti AS DOUBLE, o tutti
'     dalla famiglia intera a 8 byte) viaggia per valore come parametro E ritorno.
'     Un UDT-RITORNO può avere campi di kind MISTO (un DOUBLE e un intero, in
'     qualunque ordine) — un UDT-PARAMETRO no (limite del dispatch nativo VM/C).
' =================================================================

TYPE Point
    X AS DOUBLE
    Y AS DOUBLE
END TYPE

TYPE Int2
    A AS LONG
    B AS LONG
END TYPE

TYPE Labeled
    V AS DOUBLE
    N AS LONG
END TYPE

DECLARE FUNCTION vec_add LIB "./libvec_module" (a AS LONG, b AS LONG) AS LONG
DECLARE FUNCTION vec_scale LIB "./libvec_module" (a AS DOUBLE, b AS DOUBLE) AS DOUBLE
DECLARE FUNCTION vec_isprime LIB "./libvec_module" (n AS LONG) AS LONG
DECLARE FUNCTION vec_label LIB "./libvec_module" (id AS LONG, name AS STRING) AS STRING
DECLARE FUNCTION vec_divmod LIB "./libvec_module" (a AS LONG, b AS LONG, BYREF r AS LONG) AS LONG
DECLARE SUB vec_scale_ref LIB "./libvec_module" (n AS LONG, BYREF pv AS DOUBLE)
DECLARE FUNCTION vec_make_point LIB "./libvec_module" (x AS DOUBLE, y AS DOUBLE) AS Point
DECLARE FUNCTION vec_point_len LIB "./libvec_module" (p AS Point) AS DOUBLE
DECLARE FUNCTION vec_point_add LIB "./libvec_module" (a AS Point, b AS Point) AS Point
DECLARE FUNCTION vec_int2_sum LIB "./libvec_module" (a AS Int2, b AS Int2) AS Int2
DECLARE FUNCTION vec_make_labeled_dn LIB "./libvec_module" (v AS DOUBLE, n AS DOUBLE) AS Labeled
' Callback come parametro (§3bis): un puntatore a funzione nativo REALE — il nativo richiama
' indietro nel chiamante BASIC durante l'esecuzione (comparator/apply-style). v1: arita' 0-4,
' firma interna del callback omogenea per kind (DOUBLE o intero), BYVAL, un solo parametro
' callback per dichiarazione. Un parametro callback forza SEMPRE Int64 sulla firma ESTERNA
' della dichiarazione (e' un puntatore) — indipendentemente dal kind INTERNO del callback.
DECLARE FUNCTION vec_cb_apply LIB "./libvec_module" (cb AS FUNCTION(x AS LONG) AS LONG, x AS LONG) AS LONG
DECLARE FUNCTION vec_cb_combine2 LIB "./libvec_module" (cb AS FUNCTION(a AS LONG, b AS LONG) AS LONG, a AS LONG, b AS LONG) AS LONG
DECLARE FUNCTION vec_cb_sum_transformed LIB "./libvec_module" (buf() AS LONG, n AS LONG, cb AS FUNCTION(x AS LONG) AS LONG) AS LONG
DECLARE FUNCTION vec_cb_apply_i2d LIB "./libvec_module" (cb AS FUNCTION(x AS DOUBLE) AS DOUBLE, unused AS LONG) AS LONG
SUB Main()
    PRINT "╔══════════════════════════════════════════════════════════╗"
    PRINT "║  NativeVec — FFI modulo C con gmb_ffi.h                  ║"
    PRINT "╚══════════════════════════════════════════════════════════╝"
    PRINT

    ' --- 1. Funzioni scalari (LONG/DOUBLE) ---

    PRINT "vec_add(21, 21)   = "; vec_add(21, 21)      ' 42
    PRINT "vec_scale(1.5, 4) = "; vec_scale(1.5, 4.0)  ' 6
    PRINT "vec_isprime(17)   = "; vec_isprime(17)      ' 1
    PRINT "vec_isprime(21)   = "; vec_isprime(21)      ' 0
    PRINT

    ' --- 2. Stringa (copy-out: il modulo riusa un buffer statico) ---
    DIM lbl$ = vec_label(7, "alpha")
    PRINT "vec_label(7, alpha) = "; lbl$
    ' Verifica che la STRING BASIC sia una COPIA (seconda chiamata non la altera)
    DIM lbl2$ = vec_label(9, "beta")
    PRINT "Dopo vec_label(9,beta): lbl$ ancora = "; lbl$   ' "7:alpha" (non "9:beta")
    PRINT

    ' --- 2bis. BYREF scalare (§1bis): puntatore a un singolo valore, non un buffer ---
    DIM q AS LONG
    DIM remainder AS LONG
    q = vec_divmod(17, 5, remainder)
    PRINT "vec_divmod(17, 5)   = "; q; "resto"; remainder      ' 3 resto 2

    DIM pv AS DOUBLE
    pv = 2.5
    vec_scale_ref(4, pv)
    PRINT "vec_scale_ref(4, 2.5) = "; pv                       ' 10
    PRINT

    ' --- 2ter. Struct/UDT per valore (§2bis): TYPE semplice passato/ritornato per VALORE ---
    DIM p1 AS Point
    p1 = vec_make_point(3, 4)
    PRINT "vec_make_point(3,4) = ("; p1.X; ","; p1.Y; ")"        ' (3, 4)
    PRINT "vec_point_len(p1)   = "; vec_point_len(p1)            ' 5

    DIM p2 AS Point
    p2 = vec_make_point(1, 1)
    DIM p3 AS Point
    p3 = vec_point_add(p1, p2)
    PRINT "vec_point_add(p1,p2) = ("; p3.X; ","; p3.Y; ")"       ' (4, 5)

    DIM i1 AS Int2
    i1.A = 10 : i1.B = 20
    DIM i2 AS Int2
    i2.A = 1 : i2.B = 2
    DIM i3 AS Int2
    i3 = vec_int2_sum(i1, i2)
    PRINT "vec_int2_sum(i1,i2) = ("; i3.A; ","; i3.B; ")"        ' (11, 22)

    ' Kind misto sul RITORNO (un DOUBLE e un intero nello stesso UDT): ammesso solo
    ' come tipo di ritorno, mai come parametro (limite del dispatch nativo VM/C).
    DIM lab AS Labeled
    lab = vec_make_labeled_dn(3.5, 7)
    PRINT "vec_make_labeled_dn(3.5,7) = V="; lab.V; " N="; lab.N ' V=3.5 N=7
    PRINT

    ' --- 2quater. Callback come parametro (§3bis): il nativo richiama indietro nel chiamante ---
    ' Callback a 1 argomento: una lambda BASIC vera, invocata DENTRO vec_cb_apply.
    DIM cbr1 AS LONG
    cbr1 = vec_cb_apply(FN(x AS LONG) AS LONG => x * 2, 21)
    PRINT "vec_cb_apply(FN x=>x*2, 21) = "; cbr1                ' 42

    ' Callback a 2 argomenti.
    DIM cbr2 AS LONG
    cbr2 = vec_cb_combine2(FN(a AS LONG, b AS LONG) AS LONG => a + b * 10, 3, 4)
    PRINT "vec_cb_combine2(FN a,b=>a+b*10, 3, 4) = "; cbr2      ' 43

    ' Callback COMBINATO con un parametro array nella stessa firma: il nativo somma il buffer
    ' DOPO averlo trasformato elemento per elemento tramite la lambda.
    DIM cbbuf(3) AS LONG
    cbbuf(0) = 1 : cbbuf(1) = 2 : cbbuf(2) = 3 : cbbuf(3) = 4
    DIM cbr3 AS LONG
    cbr3 = vec_cb_sum_transformed(cbbuf, 4, FN(x AS LONG) AS LONG => x * x)
    PRINT "vec_cb_sum_transformed({1,2,3,4}, FN x=>x*x) = "; cbr3  ' 30 (1+4+9+16)

    ' Il kind INTERNO del callback (qui DOUBLE) e' indipendente dal kind ESTERNO forzato
    ' (LONG) della dichiarazione — il callback stesso lavora in virgola mobile.
    DIM cbr4 AS LONG
    cbr4 = vec_cb_apply_i2d(FN(x AS DOUBLE) AS DOUBLE => x * 2.0, 0)
    PRINT "vec_cb_apply_i2d(FN x AS DOUBLE=>x*2.0) = "; cbr4    ' 7 (cb(3.5)*2.0 = 7.0)
    PRINT

    ' --- 3. gmb-native (livello 2) — nota: richiede il dispatcher futuro ---
    ' Il modulo espone anche gmb_ffi_module_entry() (tabella vec_add/vec_scale/
    ' vec_label/VecBoom). Quando il backend IL/C99/VM integrerà gmb_ffi.h, si
    ' potrà chiamare per NOME tramite la tabella; oggi si usa il livello 1 sopra.
    PRINT "Test NativeVec completato."
END SUB
