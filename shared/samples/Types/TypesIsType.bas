' FILE: Types/TypesIsType.bas
' =================================================================================================
'       GUIDA TEORICA: RUNTIME TYPE INFORMATION (RTTI) E OPERATORE IS
' =================================================================================================
'
' Questo test copre l'utilizzo dell'operatore `IS` per l'ispezione dei tipi a runtime.
' L'operatore `IS` permette di interrogare la natura reale di un dato, facilitando la 
' programmazione polimorfica e la gestione di variabili dinamiche (ANY).
'
' 1. CONTROLLO TIPI PRIMITIVI
'    Permette di verificare se una variabile contiene un tipo base.
'    - Esempio: `IF x IS STRING THEN ...`, `IF n IS DOUBLE THEN ...`
'
' 2. CONTROLLO IDENTITA' (NIL)
'    L'operatore `IS` e' il modo standard per verificare se un riferimento e' nullo.
'    - Esempio: `IF obj IS NIL THEN ...`
'
' 3. POLIMORFISMO E EREDITARIETA'
'    Per le classi, `IS` restituisce TRUE non solo per il tipo esatto, ma anche per i tipi base.
'    - Esempio: Se `Cerchio` eredita da `Forma`, `mioCerchio IS Forma` e' TRUE.
'
' 4. TIPI DEFINITI DALL'UTENTE (UDT)
'    Funziona con le strutture definite tramite `TYPE...END TYPE`.
'
' 5. VARIABILI ANY E JSON
'    Fondamentale quando si lavora con dati eterogenei dove il tipo non e' noto a priori.
'
' =================================================================================================

IMPORT COLLECTIONS

CLASS Animale
END CLASS
CLASS Cane INHERITS Animale
END CLASS

DIM mioCane AS Cane
DIM contenitore AS ANY
DIM lista AS ANY

10 CLS
20 PRINT "╔═════════════════════════════════════════════════════════════════════════════╗"
30 PRINT "║ TypesIsType - Ispezione dei Tipi a Runtime e Operatore IS                   ║"
40 PRINT "╚═════════════════════════════════════════════════════════════════════════════╝"
50 PRINT

' --- Dispatcher dei Test ---
100 GOSUB TestPrimitives
110 GOSUB TestUDTs
120 GOSUB TestClassesAndInheritance
130 GOSUB TestAnyAndNil

190 PRINT "--- Test completati ---"
200 END


' -------------------------------------------------------------------------
' 1. TIPI PRIMITIVI: Verifica dei tipi base
' -------------------------------------------------------------------------
1000 TestPrimitives:
1010 PRINT "--- 1. Testing Primitive Types ---"
1020 DIM nome$ = "Gaetano", eta% = 42, saldo# = 100.50
1030 IF nome$ IS STRING THEN PRINT "  - 'nome$' e' una STRING"
1040 IF eta% IS INTEGER THEN PRINT "  - 'eta%' e' un INTEGER"
1050 IF saldo# IS DOUBLE THEN PRINT "  - 'saldo#' e' un DOUBLE"
1060 IF NOT (eta% IS STRING) THEN PRINT "  - 'eta%' NON e' una stringa (Corretto)"
1070 PRINT
1200 RETURN


' -------------------------------------------------------------------------
' 2. USER DEFINED TYPES: Verifica strutture TYPE
' -------------------------------------------------------------------------
2000 TestUDTs:
2010 PRINT "--- 2. Testing User-Defined Types (UDT) ---"
2020 TYPE Punto
2030     X AS INTEGER
2040     Y AS INTEGER
2050 END TYPE
2060 DIM p AS Punto
2070 IF p IS Punto THEN PRINT "  - Variabile 'p' identificata come TYPE 'Punto'"
2080 PRINT
2100 RETURN


' -------------------------------------------------------------------------
' 3. CLASSI E EREDITARIETA': Polimorfismo a runtime
' -------------------------------------------------------------------------
3000 TestClassesAndInheritance:
3010 PRINT "--- 3. Testing Classes and Inheritance ---"
3020 mioCane = NEW Cane()
3070 IF mioCane IS Cane THEN PRINT "  - Identificato correttamente come 'Cane'"
3080 IF mioCane IS Animale THEN PRINT "  - Identificato come 'Animale' (Ereditarieta' OK)"
3090 IF NOT (mioCane IS STRING) THEN PRINT "  - Un cane non e' una stringa (RTTI OK)"
3100 PRINT
3200 RETURN


' -------------------------------------------------------------------------
' 4. ANY E NIL: Gestione della dinamicita'
' -------------------------------------------------------------------------
4000 TestAnyAndNil:
4010 PRINT "--- 4. Testing ANY and NIL ---"
4020 contenitore = "Testo iniziale"

4030 IF contenitore IS STRING THEN PRINT "  - ANY contiene attualmente una STRING"
    
4040 contenitore = 123.45
4050 IF contenitore IS DOUBLE THEN PRINT "  - ANY contiene ora un DOUBLE"

4060 contenitore = NIL
4070 IF contenitore IS NIL THEN PRINT "  - ANY e' attualmente NIL"

4080 lista = NEW LIST<LONG>()
4090 IF lista IS LIST THEN PRINT "  - Identificato come LIST generica"
4100 PRINT
4200 RETURN
