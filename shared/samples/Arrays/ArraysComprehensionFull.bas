' FILE: Arrays/ArraysComprehensionFull.bas
' =================================================================================================
'        GUIDA TEORICA: COMPRENSIONI (LIST, DICTIONARY, SET)
' =================================================================================================
'
' Le "Comprensioni" sono costrutti sintattici potenti che permettono di creare
' nuove collezioni (Array, Liste, Dizionari, Set) applicando trasformazioni e filtri
' a una o più collezioni esistenti, tutto in una singola espressione compatta.
'
' --- SINTASSI ---
'
' 1. LIST/ARRAY: [ <espressione_output> FOR <var> IN <coll> [IF <cond>] ]
'    - Produce un array. È il tipo più comune e versatile.
'
' 2. DICTIONARY: { <chiave>:<valore> FOR <var> IN <coll> [IF <cond>] }
'    - Produce un dizionario (DICT). Nota l'uso del separatore ':'.
'
' 3. SET: { <espressione_output> FOR <var> IN <coll> [IF <cond>] }
'    - Produce un HASHSET. La sintassi è simile a quella di lista, ma usa le graffe.
'      I duplicati vengono automaticamente eliminati.
'
' --- COMPONENTI ---
'
'   - <espressione_output>: L'espressione che determina il valore di ogni elemento.
'   - <variabile>: L'iteratore locale.
'   - <collezione>: L'array, lista o range su cui iterare.
'   - <condizione>: (Opzionale) Un filtro booleano.
'
' --- COMPRENSIONI ANNIDATE (NESTED) ---
'
'   [ ... FOR <v1> IN <c1> FOR <v2> IN <c2> ... [IF <cond>] ]
'
'   È possibile concatenare più clausole FOR per creare prodotti cartesiani o appiattire
'   strutture dati annidate, proprio come cicli FOR annidati.
'
' =================================================================================================

IMPORT ARRAYS
IMPORT COLLECTIONS ' Necessario per i tipi HASHSET e DICT

PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
PRINT "║ ArraysComprehensionFull - Test Comprensioni di Liste, Dizionari e Set       ║"
PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"

' --- Sezione di Test per LIST COMPREHENSION ([...]) ---

DIM numeri = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}
PRINT "Array Originale: ";
StampaArray(numeri)

PRINT
PRINT "--- 1. Map Semplice (raddoppia ogni numero) ---"
DIM doppi = [ n * 2 FOR n IN numeri ]
StampaArray(doppi) ' -> {2, 4, ..., 20}

PRINT
PRINT "--- 2. Filter Semplice (solo numeri > 5) ---"
DIM maggiori_di_5 = [ n FOR n IN numeri IF n > 5 ]
StampaArray(maggiori_di_5) ' -> {6, 7, 8, 9, 10}

PRINT
PRINT "--- 3. Map + Filter (quadrato dei numeri pari) ---"
DIM quadrati_pari = [ n * n FOR n IN numeri IF n MOD 2 = 0 ]
StampaArray(quadrati_pari) ' -> {4, 16, 36, 64, 100}

PRINT
PRINT "--- 4. Utilizzo di una Funzione nel Filtro ---"
DIM primi = [ n FOR n IN numeri IF IsPrimo(n) ]
StampaArray(primi) ' -> {2, 3, 5, 7}

PRINT
PRINT "--- 5. Test di Shadowing dello Scope ---"
DIM n = 999
PRINT "Valore di 'n' prima del ciclo: "; n
DIM risultato_shadow = [ n + 1 FOR n IN {10, 20} ]
PRINT "Valore di 'n' dopo il ciclo: "; n ' -> 999
PRINT "Risultato della comprensione: ";
StampaArray(risultato_shadow) ' -> {11, 21}

PRINT
PRINT "--- 6. Nested Comprehension (Prodotto Cartesiano) ---"
DIM setA = {10, 20}
DIM setB = {1, 2, 3}
DIM cartesiano = [ a + b FOR a IN setA FOR b IN setB ]
StampaArray(cartesiano) ' -> {11, 12, 13, 21, 22, 23}

PRINT
PRINT "--- 7. Nested Comprehension con Filtro (Esclusione diagonale) ---"
DIM range = {1, 2, 3}
DIM pairs = [ x * 10 + y FOR x IN range FOR y IN range IF x <> y ]
StampaArray(pairs) ' -> {12, 13, 21, 23, 31, 32}

' --- Sezione di Test per DICTIONARY COMPREHENSION ({k:v FOR ...}) ---

PRINT
PRINT "--- 8. Dictionary Comprehension (numero a suo quadrato) ---"
' Crea un dizionario dove la chiave è il numero e il valore è il suo quadrato.
DIM dict_quadrati AS DICT OF DOUBLE, DOUBLE = { x : x*x FOR x IN {1, 2, 3, 4} }
StampaDict(dict_quadrati) ' -> {1:1, 2:4, 3:9, 4:16}

PRINT
PRINT "--- 9. Dictionary Comprehension con Filtro ---"
' Crea un dizionario di quadrati solo per i numeri dispari.
DIM dict_dispari AS DICT OF DOUBLE, DOUBLE = { x : x*x FOR x IN numeri IF x MOD 2 <> 0 }
StampaDict(dict_dispari) ' -> {1:1, 3:9, 5:25, 7:49, 9:81}

' --- Sezione di Test per SET COMPREHENSION ({... FOR ...}) ---

PRINT
PRINT "--- 10. Set Comprehension (elimina duplicati) ---"
DIM dati_con_duplicati = {1, 2, 2, 3, 4, 4, 4, 5}
' La sintassi è come quella di lista, ma le graffe creano un HashSet.
DIM set_unici AS HASHSET OF DOUBLE = { x FOR x IN dati_con_duplicati }
' La stampa non avrà un ordine garantito, ma non ci saranno duplicati.
StampaSet(set_unici) ' -> {1, 2, 3, 4, 5} (in qualsiasi ordine)

PRINT
PRINT "Test terminato."
END

' --- Funzioni Helper per la Stampa ---

SUB StampaArray(arr() AS DOUBLE)
    FOR i = 0 TO UBOUND(arr)
        PRINT arr(i);
        IF i < UBOUND(arr) THEN PRINT ", ";
    NEXT i
    PRINT
END SUB

SUB StampaDict(d AS DICT OF DOUBLE, DOUBLE)
    DIM chiavi = ARRAYS.ASORT(d.Keys())
    FOR i = 0 TO UBOUND(chiavi)
        DIM k = chiavi(i)
        PRINT k; ":"; d(k);
        IF i < UBOUND(chiavi) THEN PRINT ", ";
    NEXT i
    PRINT
END SUB

SUB StampaSet(s AS HASHSET OF DOUBLE)
    ' Per una stampa ordinata e testabile, convertiamo il set in un array e lo ordiniamo.
    DIM elementi = ARRAYS.ASORT(s.ToArray())
    StampaArray(elementi)
END SUB

'Funzione helper per il test del filtro
FUNCTION IsPrimo(num AS INTEGER) AS BIT
    IF num <= 1 THEN FUNCTION = FALSE
    IF num = 2 THEN FUNCTION = TRUE
    IF num MOD 2 = 0 THEN FUNCTION = FALSE
    FOR i = 3 TO SQR(num) STEP 2
        IF num MOD i = 0 THEN FUNCTION = FALSE
    NEXT i
    FUNCTION = TRUE
END FUNCTION
