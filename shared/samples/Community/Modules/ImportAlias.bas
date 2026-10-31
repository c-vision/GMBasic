' FILE: Modules/ImportAlias.bas
' ======================================================================================================================
' GUIDA TEORICA: IMPORTAZIONE DI NAMESPACE CON ALIAS
' ======================================================================================================================
' Questo programma illustra l'uso degli alias per i namespace importati, una funzionalità chiave per la modularità.
'
' 1. SINTASSI IMPORT AS
'    - L'istruzione `IMPORT NamespaceName AS AliasName` permette di mappare un namespace lungo o complesso
'      su un identificatore più breve o contestualmente più significativo nel file corrente.
'
' 2. RISOLUZIONE DEI NOMI QUALIFICATI
'    - Una volta definito l'alias, l'accesso ai membri pubblici del namespace (funzioni, sub, tipi) DEVE avvenire 
'      tramite la notazione puntata usando l'alias (es. `Alias.Membro`).
'    - L'alias agisce come un prefisso locale, valido solo all'interno del file (modulo) in cui è dichiarato.
'      Non "inquina" lo spazio dei nomi globale degli altri moduli.
'
' 3. VANTAGGI DELL'ALIASING
'    - Risoluzione dei conflitti: Permette di usare due namespace che hanno membri con lo stesso nome (es. `Lib1.Open` e `Lib2.Open`).
'    - Brevità: Semplifica la scrittura del codice (es. `M.Add` invece di `MathematicalUtilities.Add`).
'    - Decoupling: Permette di cambiare il namespace sottostante senza dover rinominare tutte le chiamate nel file, 
'      basta cambiare l'istruzione `IMPORT`.
' ======================================================================================================================

' Importa il namespace 'MATHUTILS' (assunto definito in un altro modulo, es. MathUtils.bas)
' e gli assegna l'alias locale 'M'.
IMPORT MATHUTILS AS M

SUB Main()
    DIM result AS DOUBLE

    CLS
    PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ ImportAlias - Test 'IMPORT NAMESPACE AS ALIAS'                              ║"
    PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' Test 1: Chiamata a funzione qualificata tramite alias
    PRINT "Chiamata a Add(20, 10) tramite alias 'M'..."
    ' Il parser riconosce 'M' come alias di namespace e risolve 'Add' in 'MATHUTILS'.
    result = M.Add(20.0, 10.0)
    PRINT "Risultato:", result ' Atteso: 30
    PRINT

    ' Test 2: Chiamata a un'altra funzione dello stesso namespace aliasato
    PRINT "Chiamata a Subtract(20, 10) tramite alias 'M'..."
    result = M.Subtract(20.0, 10.0)
    PRINT "Risultato:", result ' Atteso: 10
    
    ' VERIFICA DI COMPLETEZZA
    ' In un ambiente reale, si potrebbe testare anche l'accesso a costanti o tipi definiti nel namespace.
    ' Esempio (se MATHUTILS definisse PI):
    ' PRINT "Valore di PI da alias:", M.PI
END SUB