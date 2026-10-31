' FILE: Functions/FunctionsLocalAndSubs.bas
' =================================================================================================
'       GUIDA TEORICA: FUNZIONI LOCALI, SUBROUTINE E CLOSURE
' =================================================================================================
'
' Questo test esplora due concetti potenti e correlati in GM-BASIC: le procedure locali
' e le "closure".
'
' 1. PROCEDURE LOCALI (`LOCAL FUNCTION` / `LOCAL SUB`)
'    Una procedura locale è una `FUNCTION` o una `SUB` definita all'interno di un'altra
'    procedura ("genitore").
'    - Visibilità: È visibile e chiamabile solo dall'interno della procedura genitore e dalle
'      altre procedure locali definite nello stesso genitore. Non è accessibile dall'esterno.
'    - Incapsulamento: Sono uno strumento eccellente per incapsulare logica helper complessa
'      che non ha senso esporre a livello di modulo.
'    - Sintassi: `LOCAL FUNCTION ...` o semplicemente `FUNCTION ...` (la keyword `LOCAL` è
'      opzionale quando la definizione si trova dentro un'altra procedura).
'
' 2. CLOSURE (CHIUSURE)
'    Una "closure" si verifica quando una funzione interna (come una funzione locale) "cattura"
'    e ricorda le variabili locali della sua funzione genitore.
'
'    - Come funziona: La funzione locale `Moltiplica` è definita all'interno di `Main` e usa la
'      variabile `fattore`, che è locale a `Main`. La funzione `Moltiplica` "chiude" (cattura)
'      un riferimento a `fattore`.
'    - Stato Persistente: Anche se la funzione locale viene chiamata in momenti diversi,
'      accederà sempre allo stato *attuale* della variabile catturata. Se `fattore` viene
'      modificato in `Main`, la successiva chiamata a `Moltiplica` userà il nuovo valore.
'
'    Questo meccanismo è estremamente potente e permette di creare funzioni "stateful"
'    senza dover ricorrere a variabili globali.
'
' =================================================================================================

SUB Main
    CLS
    PRINT "╔════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ FunctionsLocalAndSubs - Funzioni e Subroutine Locali Nominate          ║"
    PRINT "╚════════════════════════════════════════════════════════════════════════╝"
    PRINT
    
    PRINT "--- 1. Definizione e Chiamata di SUB e FUNCTION Locali ---"
    
    ' Variabile locale di Main. Verrà "catturata" dalla funzione locale Moltiplica.
    DIM fattore = 10

    // Definizione di una FUNCTION locale.
    ' "Moltiplica" è una closure perché accede alla variabile "fattore" del suo scope genitore (Main).
    FUNCTION Moltiplica(x) AS DOUBLE
        FUNCTION = x * fattore
    END FUNCTION

    // Definizione di una SUB locale.
    ' "StampaRisultato" non cattura variabili, è un semplice helper di utilità locale.
    SUB StampaRisultato(desc$, val)
        PRINT desc$; " -> "; val
    END SUB

    PRINT "Funzioni e SUB locali definite all'interno di Main."
    ' Chiama la funzione locale, che usa il valore corrente di 'fattore' (10).
    StampaRisultato("Moltiplica(5)", Moltiplica(5)) // Stampa -> 50
    
    ' Modifichiamo la variabile 'fattore' nello scope di Main.
    fattore = 20 // Modifichiamo la variabile catturata dalla closure
    PRINT "Variabile 'fattore' modificata a 20."
    ' La successiva chiamata a 'Moltiplica' userà il nuovo valore di 'fattore' (20).
    StampaRisultato("Moltiplica(5) dopo modifica", Moltiplica(5)) // Stampa -> 100
    PRINT

    PRINT "--- 2. Procedura Locale che chiama un'altra Procedura Locale ---"
    
    // "EseguiCalcolo" è una SUB locale che invoca
    ' "EseguiCalcolo" è visibile solo in Main e può chiamare le altre funzioni definite qui.
    SUB EseguiCalcolo(valore)
        PRINT "Dentro 'EseguiCalcolo' con valore "; valore; ":"
        DIM risultato = Moltiplica(valore)
        StampaRisultato("   Risultato interno", risultato)
    END SUB

    ' Esegue l'orchestratore.
    EseguiCalcolo(7) // Stampa -> 140
    PRINT

    PRINT "Esecuzione di Main terminata."
END SUB

' Le funzioni/SUB locali definite in Main non sono visibili qui.
' La riga seguente, se decommentata, causerebbe un errore di compilazione
' "Subprogram or function not defined" perché 'StampaRisultato' non esiste in questo scope.
' StampaRisultato("Test", 123)
