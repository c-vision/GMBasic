' FILE: Namespaces.bas
' ======================================================================================================================
' GUIDA TEORICA: NAMESPACE E MODULARITÀ
' ======================================================================================================================
' GM-BASIC supporta un sistema di namespace e moduli per organizzare codice complesso, promuovere il riutilizzo e
' prevenire conflitti di nomi.
'
' 1. DEFINIZIONE DI UN NAMESPACE
'    - Sintassi: NAMESPACE <NomeNamespace>
'    - Un namespace è un "contenitore" logico per procedure, tipi e variabili globali.
'    - Lo stesso namespace può essere esteso su più file. Il compilatore aggrega tutte le definizioni.
'    - I simboli (procedure, tipi) possono essere dichiarati `PUBLIC` per essere visibili all'esterno.
'
' 2. UTILIZZO DEI NAMESPACE (IMPORT)
'    - `IMPORT <NomeNamespace>`: Importa tutti i simboli `PUBLIC` del namespace nello scope globale.
'      Permette di chiamare `MiaFunzione()` invece di `NomeNamespace.MiaFunzione()`.
'    - `IMPORT <NomeNamespace> AS <Alias>`: Importa il namespace, ma richiede l'uso dell'alias per l'accesso
'      qualificato. Esempio: `IMPORT System.Math AS Math` -> `Math.Sin(...)`.
'    - `IMPORT <NomeNamespace> WITH ALIAS`: Sintassi legacy equivalente a `AS <NomeNamespace>`.
'    - `IMPORT <NomeNamespace> (Simbolo1, Simbolo2 AS Alias2)`: Importa selettivamente solo i simboli specificati.
'      Questo è utile per importare solo ciò che è necessario e per risolvere conflitti di nomi.
'
' 3. PROCESSO DI COMPILAZIONE MULTI-FILE
'    - Quando si compila un file, GM-BASIC scansiona l'intera directory del progetto per trovare tutti i file `.bas`.
'    - Il compilatore costruisce una "mappa" di tutti i namespace e dei simboli che contengono prima di analizzare
'      la logica del programma principale.
'    - Questo permette al `Linker` di risolvere le direttive `IMPORT` e le chiamate a funzioni tra file diversi.
' ======================================================================================================================

' --- PROGRAMMA PRINCIPALE (Namespaces.bas) ---

' Importa il namespace built-in per le funzioni finanziarie come IRR.
IMPORT FINANCE

' Importa tutti i simboli pubblici dal namespace "Geometria" (definito in MyLibrary.bas).
' Permette di usare "Punto" e "Distanza" direttamente.
IMPORT Geometria

' Importa selettivamente solo alcuni simboli dal namespace "Calcoli".
' Solo "Somma", "PIGRECO", "FNAPR" e "AMMORTAMENTO" saranno visibili senza qualifica.
IMPORT Calcoli (Somma, PIGRECO, FNAPR, AMMORTAMENTO)

' Importa il namespace "Calcoli" con un alias implicito (Calcoli).
' Permette di accedere agli altri membri (es. MostraRisultato) con `Calcoli.MostraRisultato`.
IMPORT Calcoli WITH ALIAS

SUB Main()
    PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ Namespaces - Multi-Namespace e Import                                       ║"
    PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' Test di chiamata a funzione importata selettivamente
    APR = FNAPR(360, 1, 10)
    PRINT "Tasso Annuo Effettivo Globale (APR) calcolato: ";
    PRINT APR; "%"
    
    ' Test di chiamata a SUB importata selettivamente
    AMMORTAMENTO()

    ' Test di chiamata a funzione da namespace built-in importato
    DIM FLUSSI(3)
    FLUSSI(0) = -10000
    FLUSSI(1) = 3000
    FLUSSI(2) = 4200
    FLUSSI(3) = 6800
    TIR = IRR(FLUSSI, 0.10) ' IRR proviene da FINANCE
    PRINT "Tasso Interno di Rendimento (IRR): "; FORMAT(TIR * 100, "#.####"); "%"

    ' --- Utilizzo del namespace Geometria (importato globalmente) ---
    PRINT
    PRINT "--- Test Namespace Geometria ---"
    ' "Punto" è un UDT definito in Geometria
    DIM a AS Punto
    DIM b AS Punto

    a.X = 10
    a.Y = 20
    b.X = 15
    b.Y = 25

    PRINT "Punto A: (" + CSTR(a.X) + ", " + CSTR(a.Y) + ")"
    PRINT "Punto B: (" + CSTR(b.X) + ", " + CSTR(b.Y) + ")"
    ' "Distanza" è una funzione definita in Geometria
    PRINT "Distanza tra A e B: " + CSTR(Distanza(a, b))
    PRINT

    ' --- Utilizzo del namespace Calcoli ---
    PRINT "--- Test Namespace Calcoli ---"
    
    ' Utilizzo di simboli importati selettivamente
    DIM raggio AS DOUBLE = 10
    ' "PIGRECO" è una costante definita in Calcoli
    DIM circonferenza AS DOUBLE = 2 * PIGRECO * raggio
    PRINT "Valore di PIGRECO importato: " + CSTR(PIGRECO)
    PRINT "Circonferenza di un cerchio con raggio " + CSTR(raggio) + ": " + CSTR(circonferenza)
    
    ' "Somma" è una funzione definita in Calcoli
    DIM totale AS DOUBLE = Somma(15, 27.5)
    PRINT "Risultato di Somma(15, 27.5): " + CSTR(totale)
    PRINT
    
    ' Utilizzo di un simbolo qualificato (non importato selettivamente)
    ' "MostraRisultato" non è stato importato globalmente, quindi richiede il nome del namespace.
    PRINT "Chiamata a SUB qualificata:"
    Calcoli.MostraRisultato("  Somma qualificata", Calcoli.Somma(100, 200))

END SUB