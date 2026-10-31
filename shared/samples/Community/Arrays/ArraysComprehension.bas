' FILE: Arrays/ArraysComprehension.bas
' =================================================================================================
'                                GUIDA TEORICA: COMPRENSIONE DI LISTA (LIST COMPREHENSION)
' =================================================================================================
'
' La "List Comprehension" è un costrutto sintattico potente che permette di creare nuovi array
' (o liste) applicando trasformazioni e filtri a una collezione esistente, tutto in una singola
' espressione compatta. È un'alternativa dichiarativa ai cicli FOR espliciti.
'
' --- SINTASSI BASE ---
'
'   [ <espressione_output> FOR <variabile> IN <collezione> [IF <condizione>] ]
'
'   - <espressione_output>: L'espressione che determina il valore di ogni elemento nel nuovo array.
'                           Corrisponde alla funzione di "Mappatura" (Map).
'   - <variabile>: La variabile iteratore che assume il valore di ogni elemento della collezione sorgente.
'                  Ha uno scope locale limitato all'espressione.
'   - <collezione>: L'array, lista o range su cui iterare.
'   - <condizione>: (Opzionale) Un'espressione booleana che filtra gli elementi. Solo se è TRUE,
'                   l'elemento viene incluso nel risultato. Corrisponde al "Filtro" (Filter).
'
' --- SINTASSI AVANZATA (NESTED COMPREHENSIONS) ---
'
'   [ <expr> FOR <v1> IN <c1> FOR <v2> IN <c2> ... [IF <cond>] ]
'
'   È possibile concatenare più clausole FOR per iterare su più collezioni simultaneamente
'   (come cicli FOR annidati). L'ordine di valutazione va da sinistra (ciclo esterno) a
'   destra (ciclo interno).
'
' --- ESEMPI ---
'
' 1. Mappatura pura:
'    [ x * x FOR x IN {1, 2, 3} ]  ->  {1, 4, 9}
'
' 2. Filtro puro:
'    [ x FOR x IN {1, 2, 3, 4} IF x > 2 ]  ->  {3, 4}
'
' 3. Nested (Prodotto Cartesiano):
'    [ x + y FOR x IN {10, 20} FOR y IN {1, 2} ] -> {11, 12, 21, 22}
'
' --- SCOPE E SHADOWING ---
' La variabile iteratore dichiarata nella comprehension (es. 'x' negli esempi sopra) è locale
' all'espressione. Se esiste una variabile esterna con lo stesso nome, essa viene "nascosta" (shadowed).
' =================================================================================================

IMPORT ARRAYS
10 PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
20 PRINT "║ ArraysComprehension - Test Completo (incluso Nested)                        ║"
30 PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"

' Creazione di un array letterale per i test
40 DIM numeri = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10}
50 PRINT "Array Originale: ";
60 StampaArray(numeri)

70 PRINT
80 PRINT "--- 1. Map Semplice (raddoppia ogni numero) ---"
' Esempio di mappatura: ogni elemento 'n' viene moltiplicato per 2.
' Non c'è clausola IF, quindi tutti gli elementi vengono processati.
90 DIM doppi = [ n * 2 FOR n IN numeri ]
100 StampaArray(doppi) // Dovrebbe stampare 2, 4, 6, ..., 20

110 PRINT
120 PRINT "--- 2. Filter Semplice (solo numeri > 5) ---"
' Esempio di filtro: l'espressione di output è l'elemento stesso ('n').
' La clausola IF seleziona solo gli elementi maggiori di 5.
130 DIM maggiori_di_5 = [ n FOR n IN numeri IF n > 5 ]
140 StampaArray(maggiori_di_5) //Dovrebbe stampare 6, 7, 8, 9, 10

150 PRINT
160 PRINT "--- 3. Map + Filter (quadrato dei numeri pari) ---"
' Combinazione: calcola il quadrato (Map) solo per i numeri pari (Filter).
170 DIM quadrati_pari = [ n * n FOR n IN numeri IF n MOD 2 = 0 ]
180 StampaArray(quadrati_pari) //Dovrebbe stampare 4, 16, 36, 64, 100

190 PRINT
200 PRINT "--- 4. Utilizzo di una Funzione nel Filtro ---"
' La condizione del filtro può essere una chiamata a funzione complessa.
' Qui usiamo 'IsPrimo' per selezionare solo i numeri primi.
210 DIM primi = [ n FOR n IN numeri IF IsPrimo(n) ]
220 StampaArray(primi) //Dovrebbe stampare 2, 3, 5, 7

230 PRINT
240 PRINT "--- 5. Test di Shadowing dello Scope ---"
' Verifica che la variabile iteratore 'n' non interferisca con una variabile esterna 'n'.
250 DIM n = 999 //Variabile esterna
260 PRINT "Valore di 'n' prima del ciclo: "; n
' Qui 'n' dentro le parentesi quadre è una nuova variabile locale, distinta da quella esterna.
270 DIM risultato_shadow = [ n + 1 FOR n IN {10, 20} ]
280 PRINT "Valore di 'n' dopo il ciclo: "; n//Deve essere ancora 999
290 PRINT "Risultato della comprensione: ";
300 StampaArray(risultato_shadow) //Dovrebbe essere 11, 21

310 PRINT
320 PRINT "--- 6. Nested Comprehension (Prodotto Cartesiano) ---"
' Genera tutte le combinazioni somma tra due insiemi.
DIM setA = {10, 20}
DIM setB = {1, 2, 3}
' L'equivalente di: FOR a IN setA: FOR b IN setB: yield a+b
DIM cartesiano = [ a + b FOR a IN setA FOR b IN setB ]
StampaArray(cartesiano) //Dovrebbe stampare 11, 12, 13, 21, 22, 23

330 PRINT
340 PRINT "--- 7. Nested Comprehension con Filtro (Esclusione diagonale) ---"
' Genera coppie (x * 10 + y) ma solo se x <> y
DIM range = {1, 2, 3}
DIM pairs = [ x * 10 + y FOR x IN range FOR y IN range IF x <> y ]
StampaArray(pairs) //Dovrebbe stampare 12, 13, 21, 23, 31, 32

350 PRINT
360 PRINT "Test terminato."
370 END

//Funzione helper per stampare un array
SUB StampaArray(arr() AS DOUBLE)
    FOR i = 0 TO UBOUND(arr)
        PRINT arr(i);
        IF i < UBOUND(arr) THEN PRINT ", ";
    NEXT i
    PRINT
END SUB

//Funzione helper per il test del filtro
FUNCTION IsPrimo(num AS INTEGER) AS BIT
    IF num <= 1 THEN FUNCTION = FALSE
    IF num = 2 THEN FUNCTION = TRUE
    IF num MOD 2 = 0 THEN FUNCTION = FALSE
    FOR i = 3 TO SQR(num) STEP 2
        IF num MOD i = 0 THEN FUNCTION = FALSE
    NEXT i
    FUNCTION = TRUE
END SUB