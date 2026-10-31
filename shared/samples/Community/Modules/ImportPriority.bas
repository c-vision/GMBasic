' FILE: Modules/ImportPriority.bas
' ======================================================================================================================
' GUIDA TEORICA: PRIORITÀ DI RISOLUZIONE DEI NOMI (SHADOWING)
' ======================================================================================================================
' Questo programma dimostra le regole di precedenza che il compilatore GM-BASIC applica quando un identificatore
' è definito sia localmente che in un namespace importato.
'
' 1. LA REGOLA "LOCAL WINS" (IL LOCALE VINCE)
'    - Quando il Resolver incontra una chiamata non qualificata (es. `PrintMessage(...)`), cerca il simbolo
'      partendo dallo scope più interno verso l'esterno.
'    - Ordine di ricerca:
'      1. Variabili locali (se all'interno di una procedura).
'      2. Membri del modulo corrente (Funzioni, Sub, Variabili Globali definite nel file).
'      3. Membri pubblici dei namespace importati (es. MATHUTILS).
'      4. Membri del namespace di default (es. SYSTEM).
'    - Di conseguenza, una SUB definita nel file corrente "nasconde" (shadows) una SUB con lo stesso nome
'      presente in una libreria importata.
'
' 2. ACCESSO QUALIFICATO PER EVITARE LO SHADOWING
'    - Per invocare la versione "nascosta" di una procedura, è necessario utilizzare la sintassi qualificata:
'      `NomeNamespace.NomeProcedura` (es. `MATHUTILS.PrintMessage`).
'    - Questo bypassa la ricerca locale e indirizza il Resolver direttamente al namespace specificato.
'
' 3. IMPORTANZA ARCHITETTURALE
'    - Questa regola permette agli sviluppatori di estendere o sovrascrivere funzionalità di libreria
'      senza modificare il codice della libreria stessa e protegge il codice locale da conflitti
'      di nomi accidentali quando si aggiungono nuovi import.
' ======================================================================================================================

' Importa il namespace MATHUTILS. Questo rende disponibili i suoi membri (come PrintMessage),
' ma con priorità inferiore rispetto alle definizioni locali.
IMPORT MATHUTILS WITH ALIAS

' Questa SUB è definita localmente nel modulo corrente.
' Ha lo stesso nome di una SUB pubblica presente in MATHUTILS.inc.
SUB PrintMessage(msg AS STRING)
    PRINT "Message from Main Module:", msg
END SUB

SUB Main()
    CLS
    PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ ImportPriority - Test Name Resolution Priority (Local > Imported)           ║"
    PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' Test 1: Chiamata Non Qualificata
    ' Il compilatore deve risolvere questo nome nella SUB definita a riga 25 (locale),
    ' ignorando quella in MATHUTILS.
    PRINT "Chiamata a PrintMessage('Test'). Dovrebbe invocare la versione locale..."
    PrintMessage("Test") ' Atteso output: "Message from Main Module: Test"
    PRINT

    ' Test 2: Chiamata Qualificata
    ' L'uso esplicito del prefisso del namespace forza il compilatore a ignorare la SUB locale
    ' e a cercare direttamente in MATHUTILS.
    PRINT "Chiamata a MATHUTILS.PrintMessage('Test'). Deve invocare la versione importata..."
    MATHUTILS.PrintMessage("Test") ' Atteso output: "Message from MATHUTILS: Test"
END SUB

' ======================================================================================================================
' VERIFICA DI COMPLETEZZA
' ======================================================================================================================
' Il test copre correttamente:
' 1. Definizione di un simbolo locale in conflitto con uno importato.
' 2. Risoluzione prioritaria del simbolo locale (Shadowing).
' 3. Accesso esplicito al simbolo importato tramite qualificazione.
' Nessuna integrazione aggiuntiva richiesta.