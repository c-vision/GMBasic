' FILE: Modules/ImportWildcard.bas
' ======================================================================================================================
' GUIDA TEORICA: IMPORTAZIONE WILDCARD (GLOBALE)
' ======================================================================================================================
' Questo programma testa la modalità di importazione più comune e diretta: l'importazione "Wildcard".
'
' 1. SINTASSI IMPORT SEMPLICE
'    - `IMPORT NamespaceName` (senza `AS` o `(...)`).
'    - Questa istruzione rende TUTTI i membri `PUBLIC` del namespace specificato direttamente visibili e utilizzabili
'      nello scope corrente, senza bisogno di qualificazione (prefisso).
'
' 2. MECCANISMO DI RISOLUZIONE
'    - Quando il compilatore incontra una chiamata a funzione non qualificata (es. `Add(...)`), cerca la definizione
'      in questo ordine:
'      1. Procedure locali o definite nel file corrente.
'      2. Procedure pubbliche nei namespace importati con modalità Wildcard.
'      3. Procedure del namespace di sistema/default.
'
' 3. VANTAGGI E RISCHI
'    - Vantaggio: Massima comodità di scrittura. Il codice appare pulito come se le funzioni fossero native.
'    - Rischio: "Inquinamento" dello scope globale. Se si importano due librerie che hanno entrambe una funzione
'      chiamata `Open`, si creerà un'ambiguità che il compilatore segnalerà come errore (Ambiguous Call),
'      costringendo l'utente a passare all'import con Alias o Selettivo.
' ======================================================================================================================

' Importa tutti i membri pubblici del namespace 'MATHUTILS' nello scope globale del modulo corrente.
' Non è necessario usare un prefisso per chiamare le funzioni.
IMPORT MATHUTILS

SUB Main()
    DIM result AS DOUBLE
    
    CLS
    PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ ImportWildcard - Test 'IMPORT NAMESPACE.*'                                  ║"
    PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
    PRINT
    
    ' Test 1: Chiamata diretta a funzione importata (Add)
    PRINT "Chiamata a Add(10, 5) senza prefisso..."
    ' Il Resolver cerca 'Add'. Non trovandolo localmente, lo trova nel set di simboli di MATHUTILS importati.
    result = Add(10, 5)
    PRINT "Risultato:", result ' Atteso: 15
    PRINT
    
    ' Test 2: Chiamata diretta ad un'altra funzione dello stesso namespace (Subtract)
    PRINT "Chiamata a Subtract(10, 5) senza prefisso..."
    result = Subtract(10, 5)
    PRINT "Risultato:", result ' Atteso: 5
END SUB
