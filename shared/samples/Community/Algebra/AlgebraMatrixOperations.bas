' FILE: Algebra/AlgebraMatrixOperations.bas
' =================================================================================================
'                          GUIDA TEORICA: MATRICI DI TRASFORMAZIONE
' =================================================================================================
'
' In GM-BASIC, le matrici 4x4 sono fondamentali per la pipeline grafica 3D e il calcolo spaziale.
' Il namespace MATRIX gestisce queste entità tramite Handle (LONG).
'
' 1. CREAZIONE
'    Si utilizza `MATRIX.CREATE(righe, colonne)` per ottenere un handle. 
'    Per la pipeline 3D standard si usano matrici 4x4.
'
' 2. SINTASSI LEGACY (COMMAND-BASED)
'    Ottimizzata per la scrittura rapida. Non usa parentesi.
'    Es: `MATRIX TRANSLATE handle, x, y, z`
'
' 3. SINTASSI LEGACY (a spazio)
'    Utilizza la notazione punto senza parentesi per i comandi.
'    Es: `MATRIX TRANSLATE handle, x, y, z`
'
' 4. STACK DELLE MATRICI
'    Il sistema permette di salvare e ripristinare stati matriciali tramite PUSH e POP,
'    essenziale per il rendering gerarchico (es. muovere un braccio rispetto al corpo).
'
' =================================================================================================

IMPORT MATRIX
IMPORT MATH
IMPORT JSON

SUB Main()
    CLS
    PRINT "╔════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ MATRIX - Operazioni su Matrici (Operazioni Matriciali)                ║"
    PRINT "╚════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' --- 1. CREAZIONE MATRICI ---
    PRINT "--- 1. Creazione Matrici ---"
    ' La creazione avviene tramite funzione qualificata (ritorna un handle LONG)
    DIM M1 = MATRIX.CREATE(4, 4)
    DIM M2 = MATRIX.CREATE(4, 4)
    DIM M_Result = MATRIX.CREATE(4, 4)
    PRINT "  Matrici M1, M2, M_Result create con handle: "; M1; ", "; M2; ", "; M_Result
    PRINT

    ' --- 2. TRASFORMAZIONI DI BASE (SINTASSI LEGACY) ---
    PRINT "--- 2A. Trasformazioni (Sintassi Legacy - Comandi) ---"
    
    MATRIX IDENTITY M1              : PRINT "  MATRIX IDENTITY: OK"
    MATRIX TRANSLATE M1, 10, 20, 30 : PRINT "  MATRIX TRANSLATE: OK"
    MATRIX ROTATE M2, 45, 0, 0      : PRINT "  MATRIX ROTATE: OK"
    MATRIX SCALE M2, 2, 2, 2        : PRINT "  MATRIX SCALE: OK"
    PRINT

    ' --- 3. TRASFORMAZIONI DI BASE ---
    PRINT "--- 2B. Trasformazioni ---"
    
    MATRIX IDENTITY M1              : PRINT "  MATRIX IDENTITY: OK"
    MATRIX TRANSLATE M1, 10, 20, 30 : PRINT "  MATRIX TRANSLATE: OK"
    MATRIX ROTATE M2, 45, 0, 0      : PRINT "  MATRIX ROTATE: OK"
    MATRIX SCALE M2, 2, 2, 2        : PRINT "  MATRIX SCALE: OK"
    PRINT
    
    ' --- 4. MOLTIPLICAZIONE MATRICIALE (IN-PLACE) ---
    PRINT "--- 3. Moltiplicazione ---"
    
    ' Legacy
    MATRIX MULTIPLY M1, M2, M_Result : PRINT "  MATRIX MULTIPLY (Legacy): OK"
        PRINT
    
    ' --- 5. GESTIONE STACK MATRICI ---
    PRINT "--- 4. Stack Matrici ---"
    
    ' Legacy
    MATRIX PUSH M1                  : PRINT "  MATRIX PUSH (Legacy): OK"
    MATRIX POP M1                   : PRINT "  MATRIX POP (Legacy): OK"
    
        PRINT
    
    ' --- 6. IMPOSTAZIONE MATRICI GLOBALI (WORLD/VIEW/PROJECTION) ---
    PRINT "--- 5. Impostazione Pipeline Rendering ---"
    
    ' Legacy
    MATRIX WORLD M1                 : PRINT "  MATRIX WORLD: OK"
    MATRIX VIEW M2                  : PRINT "  MATRIX VIEW: OK"
    MATRIX PROJECTION M_Result      : PRINT "  MATRIX PROJECTION: OK"
    
        MATRIX WORLD M1                 : PRINT "  MATRIX WORLD: OK"
    MATRIX VIEW M2                  : PRINT "  MATRIX VIEW: OK"
    MATRIX PROJECTION M_Result      : PRINT "  MATRIX PROJECTION: OK"
    PRINT

    PRINT "Test Completato con successo."
END SUB