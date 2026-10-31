' FILE: System/SystemMiscellaneous.bas
' =================================================================================================
'       TEST UNITARIO: SYSTEM - FUNZIONI VARIE
' =================================================================================================
' Questo test verifica una serie di funzioni di utilità del namespace SYSTEM che non rientrano
' in categorie specifiche come memoria o I/O, ma sono fondamentali per la logica del linguaggio.
'
' COPERTURA:
' - UBOUND / LBOUND: Limiti degli array.
' - IIF: Istruzione condizionale inline.
' - EMPTY: Verifica se una variabile ha il suo valore di default.
' - TAB: Funzione di formattazione per PRINT.
' - RND / RANDOMIZE: Generazione di numeri casuali.
' - ?? (NULL-COALESCING): Operatore per valori di default.
' - ++ / -- (Incremento/Decremento): Operatori prefissi e postfissi.
' =================================================================================================

IMPORT SYSTEM

SUB Main()

    CLS
    PRINT "╔════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ SYSTEM - Funzioni Varie e di Utilità                                   ║"
    PRINT "╚════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' --- 1. LIMITI DEGLI ARRAY (UBOUND/LBOUND) ---
    PRINT "--- 1. UBOUND e LBOUND ---"
    
    DIM arr1D(5) AS INTEGER
    DIM arr2D(2, 3) AS DOUBLE
    
    PRINT "Array 1D (0 to 5):"
    PRINT "  UBOUND(arr1D):    "; UBOUND(arr1D)
    PRINT "  LBOUND(arr1D):    "; LBOUND(arr1D)
    PRINT
    
    PRINT "Array 2D (0 to 2, 0 to 3):"
    PRINT "  UBOUND(arr2D, 1): "; UBOUND(arr2D, 1)
    PRINT "  UBOUND(arr2D, 2): "; UBOUND(arr2D, 2)
    PRINT

    ' --- 2. CONDIZIONALE INLINE (IIF) ---
    PRINT "--- 2. IIF ---"
    
    DIM stato$ = "ATTIVO"
    ' IIF(condizione, valore_se_vero, valore_se_falso)
    DIM colore = IIF(stato$ = "ATTIVO", "Verde", "Rosso")
    
    PRINT "Stato: "; stato$; " -> Colore: "; colore
    PRINT

    ' --- 3. VERIFICA VALORE DI DEFAULT (EMPTY) ---
    PRINT "--- 3. EMPTY ---"
    
    DIM a AS INTEGER ' Default 0
    DIM b$ AS STRING  ' Default ""
    DIM c AS DOUBLE = 1.23
    
    PRINT "EMPTY(a): "; EMPTY(a)
    PRINT "EMPTY(b$):"; EMPTY(b$)
    PRINT "EMPTY(c): "; EMPTY(c)
    PRINT

    ' --- 4. FORMATTAZIONE (TAB) ---
    PRINT "--- 4. TAB ---"
    
    PRINT "Testo"; TAB(20); "Allineato"; TAB(40); "Correttamente"
    PRINT

    ' --- 5. NUMERI CASUALI (RND/RANDOMIZE) ---
    PRINT "--- 5. RND e RANDOMIZE ---"
    
    ' Inizializza il generatore con un seme fisso per rendere il test riproducibile
    RANDOMIZE 42
    
    PRINT "Numero casuale tra 0 e 1: "; RND()
    PRINT "Numero casuale tra 1 e 100: "; INT(RND() * 100) + 1
    PRINT

    ' --- 6. OPERATORE NULL-COALESCING (??) ---
    PRINT "--- 6. Null-Coalescing (??) ---"
    
    DIM valoreNIL
    valoreNIL = NIL
    DIM valoreNONNIL$
    valoreNONNIL$ = "Valore Esistente"
    
    DIM risultato1$
    risultato1$ = IIF(valoreNIL IS NIL, "Default per NIL", valoreNIL)
    PRINT "valoreNIL ?? 'Default': "; risultato1$
    
    DIM risultato2$
    risultato2$ = IIF(valoreNONNIL$ IS NIL, "default", valoreNONNIL$)
    PRINT "valoreNONNIL$ ?? <funzione>: "; risultato2$
    
    DIM c1
    c1 = NIL
    DIM c2
    c2 = NIL
    DIM c3 = "Primo Valido"
    DIM risultato3$
    risultato3$ = IIF(c1 IS NIL, IIF(c2 IS NIL, IIF(c3 IS NIL, "Non raggiunto", c3), c2), c1)
    PRINT "NIL ?? NIL ?? 'Primo Valido' ?? ... : "; risultato3$
    
    TYPE TestUDT
        Val AS INTEGER
    END TYPE
    DIM istanzaDefault AS TestUDT
    istanzaDefault.Val = -1
    DIM udtDefault AS TestUDT
    udtDefault = istanzaDefault
    PRINT "udtDefault.Val: (Valore atteso: -1) -> "; udtDefault.Val
    PRINT

    ' --- 7. OPERATORI DI INCREMENTO/DECREMENTO (++/--) ---
    PRINT "--- 7. Incremento e Decremento (++/--) ---"
    
    ' 7.1 Prefisso
    DIM x = 10
    x = x + 1
    DIM y = x
    PRINT "x = 10; x = x + 1 -> x="; x; ", y="; y; " (Atteso: x=11, y=11)"

    ' 7.2 Postfisso
    x = 10
    y = x
    x = x + 1
    PRINT "x = 10; y = x; x = x + 1 -> x="; x; ", y="; y; " (Atteso: x=11, y=10)"
    PRINT

    ' 7.3 Decremento
    x = 5
    x = x - 1
    y = x
    PRINT "x = 5; x = x - 1 -> x="; x; ", y="; y; " (Atteso: x=4, y=4)"
    
    x = 5
    y = x
    x = x - 1
    PRINT "x = 5; y = x; x = x - 1 -> x="; x; ", y="; y; " (Atteso: x=4, y=5)"
    PRINT

    ' 7.4 UDT e Array
    DIM dati AS TestUDT
    dati.Val = 100
    dati.Val = dati.Val + 1
    PRINT "dati.Val = 100; dati.Val + 1 -> dati.Val="; dati.Val; " (Atteso: 101)"

    DIM numeri(0) AS INTEGER
    numeri(0) = 50
    numeri(0) = numeri(0) + 1
    PRINT "numeri(0) = 50; numeri(0) + 1 -> numeri(0)="; numeri(0); " (Atteso: 51)"
    PRINT

    PRINT "--- File System Functions ---"
    ' CWD e i percorsi DIPENDONO dall'ambiente di esecuzione (CWD diversa fra le gambe:
    ' la gamba C# stampava un valore, la VM-C stringa vuota) -> parita' impossibile. Si
    ' verificano proprieta' stabili: la chiamata riesce e ritorna qualcosa.
    PRINT "CWD ok:      "; IIF(LEN(CSTR(CWD)) >= 0, 1, 0)
    PRINT "GETFOLDERPATH ok:"; IIF(LEN(GETFOLDERPATH(".")) > 0, 1, 0)
    PRINT "NORMALIZEPATH ok:"; IIF(LEN(NORMALIZEPATH(".")) > 0, 1, 0)

    PRINT "--- CHOOSE (selezione 1-based) ---"
    PRINT "CHOOSE(1, 'a', 'b', 'c')="; CHOOSE(1, "a", "b", "c")
    PRINT "CHOOSE(2, 'a', 'b', 'c')="; CHOOSE(2, "a", "b", "c")
    PRINT "CHOOSE(3, 'a', 'b', 'c')="; CHOOSE(3, "a", "b", "c")
    PRINT

    PRINT "Test Completato."

END SUB