' FILE: Functions/FunctionsAdvanced.bas
' ======================================================================================================================
' GUIDA TEORICA: FUNZIONI LOCALI AVANZATE, RICORSIONE E CLOSURE SU PARAMETRI BYREF
' ======================================================================================================================
'
' Questo file di test esplora due scenari avanzati che mettono alla prova la robustezza del sistema di scope e delle
' closure di GM-BASIC: la ricorsione di funzioni locali e la cattura (closure) di parametri passati per riferimento.
'
' 1. RICORSIONE DI FUNZIONI LOCALI
'    - Una funzione locale, essendo una funzione a tutti gli effetti, può essere ricorsiva, ovvero può chiamare se stessa.
'    - Il compilatore deve essere in grado di risolvere correttamente il nome della funzione all'interno del suo stesso
'      corpo. Questo verifica che il simbolo della funzione sia correttamente registrato nello scope prima che il corpo
'      venga analizzato in profondità.
'    - La funzione `FattorialeLocale` dimostra questo concetto, calcolando il fattoriale con un algoritmo ricorsivo pur
'      essendo definita all'interno di un'altra procedura.
'
' 2. CLOSURE SU PARAMETRI PASSATI PER RIFERIMENTO (BYREF)
'    - Questo è uno dei test più complessi per il meccanismo di closure.
'    - Una funzione locale può "catturare" non solo variabili locali della sua funzione genitore, ma anche i parametri
'      di quest'ultima.
'    - Se il parametro catturato è `BYREF`, la funzione locale non riceve una copia, ma un riferimento diretto alla
'      variabile originale del chiamante della funzione genitore.
'    - Nell'esempio `CreaIncrementatore`, la funzione locale `Incrementa` cattura `contatore`, che è un parametro `BYREF`
'      di `CreaIncrementatore`.
'    - Di conseguenza, quando la funzione `Incrementa` (restituita come delegato) viene chiamata, essa modifica
'      direttamente la variabile `contatoreGlobale` che si trova nello scope di `Main`, dimostrando che il riferimento
'      è stato catturato e preservato correttamente attraverso più livelli di chiamate.
'    - Questo meccanismo è reso possibile dalla "Heap Promotion", che sposta il riferimento alla variabile in una
'      locazione di memoria che sopravvive alla terminazione della funzione `CreaIncrementatore`.
'
' ======================================================================================================================

SUB Main
    CLS
    PRINT "╔════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ FunctionsAdvanced - Test Ricorsione Locale e Closure su BYREF          ║"
    PRINT "╚════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' ---------------------------------------------------------------------------------------------
    ' Test 1: Funzione Locale Ricorsiva
    ' ---------------------------------------------------------------------------------------------
    PRINT "--- 1. Test Funzione Locale Ricorsiva ---"

    ' 'FattorialeLocale' è definita qui e può chiamare se stessa.
    FUNCTION FattorialeLocale(N AS LONG) AS LONG
        ' Caso base della ricorsione.
        IF N <= 1 THEN
            RETURN 1
        ELSE
            ' Passo ricorsivo: la funzione chiama se stessa con un argomento ridotto.
            RETURN N * FattorialeLocale(N - 1)
        END IF
    END FUNCTION
    
    DIM numero = 5
    DIM risultato = FattorialeLocale(numero)
    PRINT "  Il fattoriale di"; numero; "calcolato da una funzione locale ricorsiva è:"; risultato
    IF risultato = 120 THEN
        PRINT "  -> ESITO: SUCCESSO"
    ELSE
        PRINT "  -> ESITO: FALLIMENTO"
    END IF
    PRINT
    
    ' ---------------------------------------------------------------------------------------------
    ' Test 2: Closure su Parametro BYREF
    ' ---------------------------------------------------------------------------------------------
    PRINT "--- 2. Test Closure su Parametro BYREF ---"
    
    ' 'contatoreGlobale' è una variabile locale di Main.
    DIM contatoreGlobale AS INTEGER = 100
    
    ' La funzione 'CreaIncrementatore' accetta un riferimento a 'contatoreGlobale'
    ' e restituisce un'altra funzione (un delegato) che "ricorda" quel riferimento.
    DIM MioIncrementatore AS FUNCTION() AS INTEGER
    MioIncrementatore = CreaIncrementatore(contatoreGlobale)
    
    PRINT "  Valore iniziale del contatore globale:"; contatoreGlobale
    
    ' Chiamiamo la funzione restituita. Ogni chiamata modificherà la variabile 'contatoreGlobale' originale.
    MioIncrementatore()
    PRINT "  Valore dopo la prima chiamata all'incrementatore:"; contatoreGlobale; "(Atteso: 101)"

    MioIncrementatore()
    PRINT "  Valore dopo la seconda chiamata all'incrementatore:"; contatoreGlobale; "(Atteso: 102)"
    
    IF contatoreGlobale = 102 THEN
        PRINT "  -> ESITO: SUCCESSO"
    ELSE
        PRINT "  -> ESITO: FALLIMENTO"
    END IF
    PRINT
    
    PRINT "--- Test completato ---"

END SUB


' Funzione di ordine superiore che crea e restituisce una closure.
' Accetta un parametro 'contatore' PER RIFERIMENTO.
FUNCTION CreaIncrementatore(BYREF contatore AS INTEGER) AS FUNCTION() AS INTEGER

    ' Questa funzione locale 'Incrementa' "cattura" il riferimento al parametro 'contatore'.
    ' Non sta catturando il valore 100, ma un puntatore alla variabile 'contatoreGlobale' di Main.
    FUNCTION Incrementa() AS INTEGER
        ' Questa modifica ha effetto sulla variabile esterna originale.
        contatore = contatore + 1
        RETURN contatore
    END FUNCTION

    ' Restituisce la funzione locale come un valore (delegato).
    RETURN Incrementa
END FUNCTION
