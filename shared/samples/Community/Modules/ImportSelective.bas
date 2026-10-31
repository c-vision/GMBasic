' FILE: Modules/ImportSelective.bas
' ======================================================================================================================
' GUIDA TEORICA: IMPORTAZIONE SELETTIVA DI MEMBRI
' ======================================================================================================================
' Questo programma testa una funzionalità avanzata del sistema di moduli: l'importazione selettiva.
'
' 1. SINTASSI IMPORT SELECTIVE
'    - `IMPORT Namespace (Membro1, Membro2 [AS Alias])`
'    - Questa sintassi permette di importare nello scope corrente SOLO specifici membri di un namespace, invece
'      di tutto il suo contenuto.
'    - È analoga al `from module import specific_function` di Python o agli import nominati di ES6.
'
' 2. VANTAGGI
'    - Pulizia dello Scope: Evita di inquinare lo spazio dei nomi globale con centinaia di simboli se ne servono solo due.
'    - Prevenzione dei Conflitti: Se due librerie hanno una funzione `Init`, si può importare solo quella necessaria
'      o usare gli alias per distinguerle (`IMPORT Lib (Init AS LibInit)`).
'    - Chiarezza: Rende esplicito quali dipendenze esterne vengono utilizzate nel file.
'
' 3. COMBINAZIONE DI IMPORT
'    - È possibile combinare import selettivi con import di namespace nello stesso file.
'    - Nell'esempio sotto, importiamo `Add` e `Multiply` per l'uso diretto, ma importiamo anche l'intero namespace
'      `MATHUTILS` (tramite `WITH ALIAS`) per poter accedere agli altri membri (come `Subtract`) tramite qualificazione.
' ======================================================================================================================

' Importa specificamente le funzioni 'Add' e 'Multiply' dal namespace MATHUTILS.
' Queste saranno visibili come se fossero definite localmente.
IMPORT MATHUTILS (Add, Multiply)

' Importa anche il riferimento al namespace stesso.
' Questo è necessario se vogliamo accedere a membri NON inclusi nella lista sopra (es. Subtract)
' usando la sintassi qualificata (MATHUTILS.Subtract).
IMPORT MATHUTILS WITH ALIAS

SUB Main()
    DIM result AS DOUBLE

    CLS
    PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ ImportSelective - Test 'IMPORT NAMESPACE (Members)'                          ║"
    PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
    PRINT
    
    ' Test 1: Chiamata diretta a membro importato selettivamente (Add)
    PRINT "Chiamata a Add(7.0, 3.0) senza prefisso (importato selettivamente)..."
    ' Il Resolver deve trovare 'Add' nello scope globale grazie all'import selettivo.
    result = Add(7.0, 3.0)
    PRINT "Risultato:", result ' Atteso: 10
    PRINT
    
    ' Test 2: Chiamata diretta a membro importato selettivamente (Multiply)
    PRINT "Chiamata a Multiply(7.0, 3.0) senza prefisso (importato selettivamente)..."
    result = Multiply(7.0, 3.0)
    PRINT "Risultato:", result ' Atteso: 21
    PRINT
    
    ' Test 3: Chiamata qualificata a membro NON importato selettivamente (Subtract)
    ' Se provassimo a chiamare 'Subtract(7, 3)' qui, il compilatore dovrebbe dare errore
    ' perché non è stato importato nella lista {Add, Multiply}.
    PRINT "Chiamata a Subtract(7, 3) con prefisso (non importato selettivamente)..."
    result = MATHUTILS.Subtract(7.0, 3.0)
    PRINT "Risultato:", result ' Atteso: 4
END SUB

' ======================================================================================================================
' VERIFICA DI COMPLETEZZA
' ======================================================================================================================
' Il test copre:
' 1. Sintassi `IMPORT Namespace (...)`.
' 2. Visibilità diretta dei membri importati.
' 3. Coesistenza con `IMPORT Namespace WITH ALIAS`.
' 4. Accesso qualificato ai membri residui.
' Nessuna modifica necessaria.