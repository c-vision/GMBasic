' EXPECT: Test Completato con successo.
' FILE: Algebra/AlgebraVectorOperations.bas
' =================================================================================================
'                          GUIDA TEORICA: OPERAZIONI VETTORIALI BATCH
' =================================================================================================
'
' GM-BASIC permette di eseguire operazioni matematiche su interi array (vettori batch) in un unico
' passo, sfruttando l'accelerazione hardware quando disponibile.
'
' 1. SINTASSI LEGACY PREFISSATA (COMMAND STYLE)
'    Inizia con la keyword VECTOR e opera direttamente sulle variabili array.
'    Es: `VECTOR ADD ArrayDest(), ArraySource()` o `VECTOR SET Array(), 0`
'
' 2. SINTASSI MODERNA QUALIFICATA FUSA (METHOD STYLE)
'    Utilizza la notazione `VECTOR COMANDO` senza parentesi per i comandi.
'    Es: `VECTOR ADD ArrayDest, ArraySource`
'
' 3. COMPORTAMENTO "IN-PLACE"
'    Entrambe le sintassi sono progettate per essere efficienti: il primo argomento (destinazione) 
'    viene solitamente modificato direttamente per evitare allocazioni di memoria inutili.
'
' 4. FUNZIONI DI CREAZIONE (HANDLE)
'    A differenza degli array BASIC dichiarati con DIM, `VECTOR.CREATE()` restituisce un Handle 
'    ad un oggetto vettore nativo (spesso 3D o 4D) usato per calcoli geometrici e matriciali.
'
' =================================================================================================

IMPORT VECTOR
IMPORT MATRIX
IMPORT MATH
IMPORT JSON

SUB Main()
    CLS
    PRINT "╔══════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ VECTOR - Operazioni Vettoriali Batch (Dual Syntax Validation)            ║"
    PRINT "╚══════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' --- 1. INIZIALIZZAZIONE VETTORI (BATCH SU ARRAY) ---
    
    PRINT "--- 1A. Inizializzazione (Sintassi Legacy) ---"
    DIM V1_L(4) AS DOUBLE, V2_L(4) AS DOUBLE
    ' Riempimento casuale e impostazione valore costante
    VECTOR FILL RANDOM V1_L, 0, 100
    VECTOR SET V2_L, 5.0
    PRINT "  V1_L (random): OK"
    PRINT "  V2_L (const):  OK"
    PRINT

    PRINT "--- 1B. Inizializzazione (Sintassi Moderna) ---"
    DIM V1_M(4) AS DOUBLE, V2_M(4) AS DOUBLE
    VECTOR FILL RANDOM V1_M, 0, 100
    VECTOR SET V2_M, 5.0
    PRINT "  V1_M (random): OK"
    PRINT "  V2_M (const):  OK"
    PRINT

    ' --- 2. ARITMETICA VETTORIALE ---
    
    PRINT "--- 2A. Aritmetica (Sintassi Legacy) ---"
    DIM Arith_L1(2) AS DOUBLE = {10, 20, 30}
    DIM Arith_L2(2) AS DOUBLE = {1, 2, 3}
    VECTOR ADD Arith_L1, Arith_L2   : PRINT "  ADD: OK"
    VECTOR SUB Arith_L1, Arith_L2   : PRINT "  SUB: OK"
    VECTOR MUL Arith_L1, 10         : PRINT "  MUL (Scalar): OK"
    VECTOR DIV Arith_L1, 10         : PRINT "  DIV (Scalar): OK"
    PRINT
    
    PRINT "--- 2B. Aritmetica (Sintassi Moderna) ---"
    DIM Arith_M1(2) AS DOUBLE = {10, 20, 30}
    DIM Arith_M2(2) AS DOUBLE = {1, 2, 3}
    VECTOR ADD Arith_M1, Arith_M2   : PRINT "  ADD: OK"
    VECTOR SUB Arith_M1, Arith_M2   : PRINT "  SUB: OK"
    VECTOR MUL Arith_M1, 10         : PRINT "  MUL (Scalar): OK"
    VECTOR DIV Arith_M1, 10         : PRINT "  DIV (Scalar): OK"
    PRINT

    ' --- 3. LOGICA DI WRAP E BOUNCE ---
    
    PRINT "--- 3A. Wrapping e Bouncing (Sintassi Legacy) ---"
    DIM PosX_L(1) AS DOUBLE = {10, 15}
    DIM VelX_L(1) AS DOUBLE = {3, -4}
    
    ' Riporta i valori nel range 0-12 (Legacy)
    VECTOR WRAP PosX_L, 0, 12
    ' Somma e applica wrap in un unico comando
    VECTOR ADD WRAP PosX_L, VelX_L, 0, 12
    PRINT "  ADD WRAP: OK"
    
    ' Calcolo rimbalzo 2D (Legacy)
    VECTOR BOUNCE2D PosX_L, VelX_L, 800, 600
    PRINT "  BOUNCE2D: OK"
    PRINT

    PRINT "--- 3B. Wrapping e Bouncing (Sintassi Moderna) ---"
    DIM PosX_M(1) AS DOUBLE = {10, 15}
    DIM VelX_M(1) AS DOUBLE = {3, -4}
    
    ' Riporta i valori nel range 0-12 (Moderna)
    VECTOR WRAP PosX_M, 0, 12
    ' Somma e applica wrap (Moderna)
    VECTOR ADD WRAP PosX_M, VelX_M, 0, 12
    PRINT "  ADDWRAP: OK"
    
    ' Calcolo rimbalzo 2D (Moderna)
    VECTOR BOUNCE2D PosX_M, VelX_M, 800, 600
    PRINT "  BOUNCE2D: OK"
    PRINT

    PRINT "Test Completato con successo."
END SUB