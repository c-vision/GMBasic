' FILE: ALGEBRA/AlgebraTests.bas
' =================================================================================================
'                          GUIDA TEORICA: ALGEBRA LINEARE IN GM-BASIC
' =================================================================================================
'
' GM-BASIC supporta due stili sintattici per manipolare Handle di matrici e vettori.
'
' 1. SINTASSI LEGACY PREFISSATA (COMMAND STYLE)
'    Usa la keyword del namespace come comando principale seguita dal sottocomando.
'    È lo stile "Clipper-like" per operazioni in-place.
'    Es: `VECTOR ADD v1, v2`
'
' 2. SINTASSI LEGACY FUSA (METHOD STYLE)
'    Usa la notazione punto senza parentesi per i comandi.
'    Es: `VECTOR ADD v1, v2`
'
' 3. COMPATIBILITÀ
'    Entrambe le sintassi sono identiche a livello di esecuzione (GPM). La forma Legacy
'    è ideale per la leggibilità in script matematici densi, la Moderna per l'integrazione
'    con il resto del codice strutturato.
'
' =================================================================================================

IMPORT MATRIX
IMPORT VECTOR
IMPORT MATH

SUB Main()
    CLS
    PRINT "╔══════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ AlgebraTests - Vettori e Matrici (Test Vettori e Matrici)                      ║"
    PRINT "╚══════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' --- PREPARAZIONE HANDLE ---
    DIM v1 = VECTOR.CREATE(10, 20, 30)
    DIM v2 = VECTOR.CREATE(1, 1, 1)
    DIM vMin = VECTOR.CREATE(0, 0, 0)
    DIM vMax = VECTOR.CREATE(100, 100, 100)
    DIM m1 = MATRIX.CREATE(4, 4)
    DIM m2 = MATRIX.CREATE(4, 4)

    ' =========================================================================
    ' SECTION 1: VECTOR OPERATIONS
    ' =========================================================================
    PRINT "--- 1. VECTOR: Arithmetic & Assignment ---"
    
    'ADD
    VECTOR ADD v1, v2               : PRINT "Vector ADD (Legacy) OK"

    'SUB
    VECTOR SUB v1, v2               : PRINT "Vector SUB (Legacy) OK"

    'MUL
    VECTOR MUL v1, 2.5              : PRINT "Vector MUL (Legacy) OK"

    'DIV
    VECTOR DIV v1, 2.0              : PRINT "Vector DIV (Legacy) OK"

    'SET
    VECTOR SET v1, 0                : PRINT "Vector SET (Legacy) OK"
    PRINT

    PRINT "--- 2. VECTOR: Range & Physics ---"
    
    'WRAP
    VECTOR WRAP v1, vMin, vMax      : PRINT "Vector WRAP (Legacy) OK"

    'ADD WRAP
    VECTOR ADD WRAP v1, v2, vMin, vMax : PRINT "Vector ADD WRAP (Legacy) OK"

    'FILL RANDOM
    VECTOR FILL RANDOM v1, -1, 1    : PRINT "Vector FILL RANDOM (Legacy) OK"

    'BOUNCE2D
    VECTOR BOUNCE2D v1, v1, v2, v2, 0, 0, 800, 600 : PRINT "Vector BOUNCE2D (Legacy) OK"
    PRINT

    ' =========================================================================
    ' SECTION 2: MATRIX OPERATIONS
    ' =========================================================================
    PRINT "--- 3. MATRIX: Transformations ---"

    'IDENTITY
    MATRIX IDENTITY m1              : PRINT "Matrix IDENTITY (Legacy) OK"

    'TRANSLATE
    MATRIX TRANSLATE m1, 10, 20, 30 : PRINT "Matrix TRANSLATE (Legacy) OK"

    'ROTATE
    MATRIX ROTATE m1, 45, 0, 0      : PRINT "Matrix ROTATE (Legacy) OK"

    'SCALE
    MATRIX SCALE m1, 2, 2, 2         : PRINT "Matrix SCALE (Legacy) OK"
    PRINT

    PRINT "--- 4. MATRIX: Stack & Algebra ---"

    'PUSH/POP
    MATRIX PUSH m1                  : PRINT "Matrix PUSH (Legacy) OK"
    MATRIX POP m1                   : PRINT "Matrix POP (Legacy) OK"

    ' MULTIPLY (in-place)
    MATRIX MULTIPLY m1, m1, m2      : PRINT "Matrix MULTIPLY (Legacy) OK"
    PRINT

    PRINT "--- 5. MATRIX: Pipeline ---"

    'WORLD/VIEW/PROJECTION
    MATRIX WORLD m1                 : PRINT "Matrix WORLD (Legacy) OK"
    
    MATRIX VIEW m1                  : PRINT "Matrix VIEW (Legacy) OK"

    MATRIX PROJECTION m1            : PRINT "Matrix PROJECTION (Legacy) OK"
    
    PRINT
    PRINT "--- LINALG Functions ---"
    DIM A(1,1), B(1)
    A(0,0)=4: A(0,1)=1: A(1,0)=1: A(1,1)=3
    B(0)=1: B(1)=2
    PRINT "DETERMINANT test: "; DETERMINANT(A)
    PRINT "PRODUCT test: "; VECTOR.PRODUCT(B, B)
    PRINT "EUCLIDEA test: "; VECTOR.EUCLIDEA(B)
    PRINT "SOLVE test: "; SOLVE(A, B)
    PRINT "CHOLESKY test: "; CHOLESKY(A)
    PRINT "LU test: "; LU(A)
    PRINT "QR test: "; QR(A)
    PRINT "SVD test: "; SVD(A)
    PRINT "EVD test: "; EVD(A)
    PRINT
    PRINT "--- TEST COMPLETATO CON SUCCESSO ---"

END SUB