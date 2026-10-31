' FILE: Clipper/ClipperDynamicScope.bas
' =========================================================================================
' ESEMPIO DIMOSTRATIVO: VARIABILI DYNAMIC (SCOPE DINAMICO)
' =========================================================================================
' Questo programma illustra il concetto di "Scope Dinamico", una caratteristica
' fondamentale di linguaggi come Clipper/xBase, emulata in GM-BASIC tramite la
' keyword `DYNAMIC`.
'
' CHE COS'È LO SCOPE DINAMICO?
' A differenza dello scope lessicale standard (usato da DIM, LOCAL), dove la visibilità
' di una variabile è determinata da dove è scritta nel codice, la visibilità di una
' variabile DYNAMIC dipende dalla CATENA DI CHIAMATE a runtime.
'
' REGOLE CHIAVE:
' 1. Visibilità a Cascata: Se una procedura `A` dichiara una variabile `DYNAMIC x` e poi
'    chiama una procedura `B`, la procedura `B` (e qualsiasi altra procedura chiamata da `B`)
'    può vedere e modificare `x` come se fosse stata dichiarata localmente.
' 2. Shadowing: Se anche `B` dichiara una sua `DYNAMIC x`, questa nuova variabile "nasconde"
'    (shadowing) temporaneamente quella di `A`. Le modifiche in `B` si applicheranno
'    alla `x` di `B`.
' 3. Ripristino Automatico: Quando `B` termina e il controllo torna ad `A`, la `x` di `B`
'    viene distrutta, e la `x` originale di `A` torna a essere visibile con il suo
'    valore precedente.
' 4. Work Area: Il caso d'uso più importante è la gestione della work area del database.
'    Quando si esegue `SELECT Clienti`, l'alias "Clienti" diventa una variabile a
'    scope dinamico, rendendola l'area di lavoro attiva per tutte le funzioni
'    chiamate successivamente, senza bisogno di passarla come parametro.
' =========================================================================================
IMPORT CONSOLE

' Procedura di secondo livello: non conosce 'messaggio', ma dovrebbe vederla
' perché è chiamata da LivelloIntermedio, che a sua volta è chiamata da Main.
SUB LivelloProfondo()
    PRINT "  -> [LivelloProfondo] La variabile 'messaggio' è visibile qui:"
    PRINT "     Valore ricevuto: '" + messaggio + "'"
    
    ' Modifichiamo la variabile. Questa modifica sarà visibile al chiamante (LivelloIntermedio)
    messaggio = "Valore modificato da LivelloProfondo"
END SUB

' Procedura di primo livello: chiama la procedura più interna.
SUB LivelloIntermedio()
    PRINT " -> [LivelloIntermedio] Entrato. 'messaggio' è visibile da Main:"
    PRINT "    Valore ricevuto: '" + messaggio + "'"
    
    ' Qui dichiariamo una nostra variabile DYNAMIC con lo stesso nome.
    ' Questa "nasconderà" temporaneamente quella dichiarata in Main.
    DYNAMIC messaggio = "Questa è una variabile diversa, locale a LivelloIntermedio"
    PRINT " -> [LivelloIntermedio] Dichiarata una nuova DYNAMIC 'messaggio' (shadowing)."
    PRINT "    Valore attuale: '" + messaggio + "'"
    
    LivelloProfondo()
    
    PRINT " -> [LivelloIntermedio] Ritorno da LivelloProfondo. 'messaggio' ora vale:"
    PRINT "    Valore attuale: '" + messaggio + "'"
    
    PRINT " -> [LivelloIntermedio] Uscita. La nostra 'messaggio' sta per essere distrutta."
END SUB

' =========================================================================================
' MAIN
' =========================================================================================
SUB Main()
    CLS
    
    ' Dichiariamo una variabile a scope dinamico.
    ' Sarà visibile a tutte le funzioni chiamate da Main.
    DYNAMIC messaggio = "Valore originale da Main"
    
    PRINT "[Main] Variabile 'messaggio' dichiarata."
    PRINT "Valore iniziale: '" + messaggio + "'"
    PRINT ""
    
    ' Chiamiamo una procedura. Non passiamo 'messaggio' come parametro.
    LivelloIntermedio()
    
    PRINT ""
    PRINT "[Main] Ritorno da LivelloIntermedio."
    PRINT "La variabile 'messaggio' di LivelloIntermedio è stata distrutta."
    PRINT "Il valore della 'messaggio' originale di Main è stato preservato e non è stato modificato."
    PRINT "Valore finale: '" + messaggio + "'"
    PRINT ""
    PRINT "--- Test completato ---"
END SUB