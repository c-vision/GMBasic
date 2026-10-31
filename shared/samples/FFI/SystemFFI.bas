' FILE: System/SystemFFI.bas
' ======================================================================================================================
' GUIDA TEORICA: FOREIGN FUNCTION INTERFACE (FFI) CON LA DIRETTIVA `DECLARE`
' ======================================================================================================================
' GM-BASIC permette di chiamare funzioni da librerie native esterne (.dll, .so, .dylib) utilizzando la
' direttiva `DECLARE`. Questo apre le porte all'interfacciamento con codice scritto in C, C++, Rust, etc.,
' per sfruttare librerie esistenti o per operazioni che richiedono performance a livello di sistema.
'
' 1. SINTASSI DELLA DICHIARAZIONE
'    - `DECLARE FUNCTION <NomeBASIC> LIB "nome_libreria" [ALIAS "nome_nativo"] ([parametri]) AS <TipoRitorno>`
'    - `DECLARE SUB <NomeBASIC> LIB "nome_libreria" ...`
'
'    - `<NomeBASIC>`: Il nome con cui la funzione sarà chiamata nel codice BASIC.
'    - `"nome_libreria"`: Il nome del file della libreria (senza estensione). Il runtime cercherà
'      automaticamente l'estensione corretta per il sistema operativo (.dll, .so, .dylib).
'    - `ALIAS "nome_nativo"` (Opzionale): Specifica il nome reale della funzione all'interno della
'      libreria, se è diverso dal nome usato in BASIC.
'    - `[parametri]`: La lista dei parametri, con tipi di dato corrispondenti a quelli nativi.
'      - I tipi numerici (`INTEGER`, `LONG`, `DOUBLE`) e i puntatori vengono mappati direttamente.
'      - `BYREF` è fondamentale per passare puntatori a variabili, permettendo alla funzione
'        nativa di scrivere dati in memoria gestita da GM-BASIC.
'
' 2. GESTIONE DEI PUNTATORI E DEGLI "HANDLE"
'    - Molte librerie C/C++ operano con puntatori opachi (void*) o "handle". In GM-BASIC, questi vengono
'      tipicamente mappati a tipi interi come `LONG` o `QUAD`.
'    - Il programma BASIC riceve l'handle dalla libreria, lo conserva e lo ripassa alle funzioni
'      successive, senza mai interpretarne il valore.
'
' 3. CICLO DI VITA E PULIZIA
'    - È responsabilità del programmatore BASIC gestire il ciclo di vita delle risorse create dalla
'      libreria nativa, chiamando le funzioni di pulizia appropriate (es. `DestroyBody`, `ShutdownEngine`)
'      per prevenire memory leak.
'
' QUESTO ESEMPIO:
' Simula l'uso di una libreria di motore fisico 2D. Vengono dichiarate funzioni per inizializzare il
' motore, creare oggetti (corpi rigidi), applicare forze, simulare il passo fisico e leggere lo stato
' (posizione, velocità).
'
' ======================================================================================================================

' --- Dichiarazione delle Funzioni Esterne dalla libreria "physics_engine" ---

' CORREZIONE: La clausola LIB va prima dei parametri.
DECLARE FUNCTION InitEngine LIB "physics_engine" () AS DOUBLE ' 0 = assente (firma tipizzata)
DECLARE SUB SetGravity LIB "physics_engine" (engineHandle AS DOUBLE, x AS DOUBLE, y AS DOUBLE) ' handle BYVAL (firma uniforme FFI)
DECLARE FUNCTION CreateBody LIB "physics_engine" (engineHandle AS DOUBLE, x AS DOUBLE, y AS DOUBLE, radius AS DOUBLE, mass AS DOUBLE) AS DOUBLE ' firma omogenea DOUBLE
DECLARE SUB ApplyForce LIB "physics_engine" (bodyHandle AS DOUBLE, force_x AS DOUBLE, force_y AS DOUBLE) ' handle BYVAL
DECLARE SUB StepEngine LIB "physics_engine" (engineHandle AS DOUBLE, timeStep AS DOUBLE) ' handle BYVAL
DECLARE SUB GetBodyPosition LIB "physics_engine" (bodyHandle AS DOUBLE, x AS DOUBLE, y AS DOUBLE) ' BYVAL (BYREF FFI non supportata)
DECLARE SUB DestroyBody LIB "physics_engine" (bodyHandle AS DOUBLE) ' handle BYVAL
DECLARE SUB ShutdownEngine LIB "physics_engine" (engineHandle AS DOUBLE) ' handle BYVAL

' --- PROGRAMMA PRINCIPALE ---

SUB Main()
    CLS
    PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ SystemFFI - Test di Interfacciamento con Libreria Esterna (FFI)             ║"
    PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
    PRINT
    
    ON ERROR GOTO GestoreErrori

    ' 1. Inizializzazione del motore fisico
    PRINT "Inizializzazione del motore fisico..."
    DIM Engine = InitEngine()
    IF Engine = 0 THEN
        ERROR 1000, "Impossibile inizializzare il motore fisico. Assicurarsi che 'physics_engine' sia presente."
    END IF
    PRINT "Motore inizializzato. Handle: "; Engine
    
    ' Imposta la gravità (solo componente Y)
    SetGravity(Engine, 0, 9.8)
    
    ' 2. Creazione di un oggetto (corpo rigido)
    PRINT "Creo una palla a (100, 50)..."
    DIM Palla = CreateBody(Engine, 100, 50, 10, 1.0)
    IF Palla = 0 THEN
        ERROR 1001, "Impossibile creare il corpo rigido."
    END IF
    PRINT "Palla creata. Handle: "; Palla
    
    ' Applica una forza iniziale per spingerla
    ApplyForce(Palla, 50, 0) ' Spinta orizzontale
    
    ' 3. Ciclo di Simulazione
    PRINT
    PRINT "--- Inizio Simulazione (200 passi) ---"
    
    DIM posX AS DOUBLE, posY AS DOUBLE
    
    FOR i = 1 TO 200
        ' Esegue un passo della simulazione (es. 1/60 di secondo)
        StepEngine(Engine, 1.0 / 60.0)
        
        ' Recupera la nuova posizione della palla
        GetBodyPosition(Palla, posX, posY)
        
        ' Stampa la posizione ogni 20 passi per non intasare l'output
        IF i MOD 20 = 0 THEN
            PRINT "Passo "; i; " -> Posizione Palla: ("; FORMAT(posX, "0.0"); ", "; FORMAT(posY, "0.0"); ")"
        END IF
    NEXT i
    
    PRINT "--- Simulazione terminata ---"
    PRINT
    
    ' 4. Pulizia delle risorse
    PRINT "Rilascio delle risorse..."
    DestroyBody(Palla)
    ShutdownEngine(Engine)
    PRINT "Risorse rilasciate."
    
    GOTO Fine

GestoreErrori:
    PRINT
    PRINT "*** ERRORE RUNTIME ***"
    PRINT "Codice: "; ERR.Number
    PRINT "Descrizione: "; ERR.Description
    PRINT "Alla riga: "; ERR.LineNumber
    ' Tenta una pulizia anche in caso di errore
    IF Engine > 0 THEN ShutdownEngine(Engine)
    
Fine:
    PRINT
    PRINT "Test FFI completato."
END SUB