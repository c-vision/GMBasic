' FILE: System/SystemLowLevel.bas
' =================================================================================================
'       TEST UNITARIO: SYSTEM - FUNZIONI A BASSO LIVELLO E UTILITÀ
' =================================================================================================
' Questo test verifica le funzioni di sistema per l'introspezione della memoria,
' la gestione dei tipi UDT e le utilità generali.
'
' COPERTURA:
' - Memoria: SIZEOF, VARPTR, STRPTR.
' - UDT Layout: UDTPACKED, UDTALIGNED (Verifica layout in memoria).
' - Utilità: IIF (Inline If), EMPTY (Check inizializzazione), Operatore Ternario (? :).
' - Info Sistema: SYS, TI/STRTI.
' - Shortcut: ? per PRINT.
' =================================================================================================

IMPORT SYSTEM
IMPORT CONVERSIONS
IMPORT CONCURRENCY

TYPE MySimpleStruct
    ID AS INTEGER
    Value AS DOUBLE
    Flag AS BIT
END TYPE

TYPE MyPackedStruct
    B1 AS BYTE
    L1 AS LONG
    B2 AS BYTE
END TYPE

SUB Main()

    CLS
    PRINT "╔════════════════════════════════════════════════════════════════════════╗"
    PRINT "║ SYSTEM - Funzioni Low-Level e Utility                                  ║"
    PRINT "╚════════════════════════════════════════════════════════════════════════╝"
    PRINT

    ' --- 1. INTROSPEZIONE MEMORIA ---
    PRINT "--- 1. SizeOf e Pointers ---"
    
    DIM i AS INTEGER = 123
    DIM d AS DOUBLE = 3.14
    DIM s AS STRING = "Hello"
    DIM u AS MySimpleStruct
    
    ' SIZEOF = byte del TIPO STATICO (canone unico, identico sui 4 backend): gli scalari
    ' hanno la loro larghezza di linguaggio, non quella della slot machine (un INTEGER vale 2
    ' anche dove a runtime i numerici sono double da 8). -1 se statico non c'è.
    PRINT "SizeOf(INTEGER): "; SYSTEM.SIZEOF(i); " (Atteso 2)"
    PRINT "SizeOf(DOUBLE):  "; SYSTEM.SIZEOF(d); " (Atteso 8)"
    PRINT "SizeOf(UDT):     "; SYSTEM.SIZEOF(u)
    
    ' VARPTR/STRPTR restituiscono un TOKEN OPACO di identita (mai 0), non un indirizzo
    ' dereferenziabile: sulle gambe native coincide con un indirizzo reale, su quelle
    ' gestite e per gli scalari VM/C con un handle. Solo queste proprieta sono portabili:
    ' non-zero, STRPTR(x)=STRPTR(x) sullo stesso oggetto vivo. PEEK/POKE/FFI non devono usarlo.
    PRINT "VarPtr(i):       "; CONVERSIONS.HEX(SYSTEM.VARPTR(i))
    
    ' STRPTR restituisce l'indirizzo del buffer stringa
    PRINT "StrPtr(s):       "; CONVERSIONS.HEX(SYSTEM.STRPTR(s))
    PRINT

    ' --- 2. LAYOUT UDT ---
    PRINT "--- 2. UDT Layout (Packed vs Aligned) ---"
    
    DIM p AS MyPackedStruct
    ' UDTPACKED/UDTALIGNED sono NUMERI: il primo conta i campi del TYPE, il secondo vale 1.
    ' La dimensione in byte del layout (packed = somma, allineato = con padding) è SIZEOF.
    PRINT "UDT Packed campi: "; SYSTEM.UDTPACKED(p)
    PRINT "UDT Aligned:      "; SYSTEM.UDTALIGNED(p)
    PRINT

    ' --- 3. UTILITY FUNZIONALI ---
    PRINT "--- 3. IIF, Operatore Ternario e EMPTY ---"
    
    DIM val = 10
    ' IIF(cond, truePart, falsePart) - Valutazione condizionale inline
    DIM res$ = SYSTEM.IIF(val > 5, "Maggiore (IIF)", "Minore (IIF)")
    PRINT "IIF(10 > 5):      "; res$
    
    ' Operatore ternario (C-style)
    res$ = IIF(val > 5, "Maggiore (?:)", "Minore (?:)")
    PRINT "val > 5 ? ... : ... "; res$
    PRINT
    
    DIM nonInit AS STRING
    DIM initStr AS STRING = "Test"
    
    ' EMPTY verifica se una variabile è nel suo stato di default/null
    PRINT "EMPTY(nonInit):   "; SYSTEM.EMPTY(nonInit)
    PRINT "EMPTY(initStr):   "; SYSTEM.EMPTY(initStr)
    PRINT

    ' --- 4. SISTEMA E TEMPO ---
    PRINT "--- 4. Info Sistema e Tempo ---"
    
    ' SYS recupera info sull'ambiente (OS, UI, Runtime)
    PRINT "OS System:        "; SYSTEM.SYS("OS")
    PRINT "UI Mode:          "; SYSTEM.SYS("UI")
    
    ' TI restituisce i tick di sistema in MILLISECONDI interi (stessa unita sulle 4 gambe;
    ' l'epoca assoluta varia fra le gambe — solo i DELTA sono portabili)
    DIM t1 = SYSTEM.TI()
    SLEEP 100 ' Pausa 100ms (dal namespace CONCURRENCY importato implicitamente o SYSTEM)
    DIM t2 = SYSTEM.TI()
    PRINT "Elapsed TI (100ms): "; (t2 - t1)
    
    ' STRTI restituisce il tempo formattato
    PRINT "System Time Str:  "; SYSTEM.STRTI()
    PRINT
    
    ' --- 5. SHORTCUT PER PRINT ---
    PRINT "--- 5. Test per '?' come alias di PRINT ---"
    ? "Questa riga è stampata con '?'."
    DIM x = 42
    ? "Il valore di x è: "; x
    PRINT

    PRINT "Test Completato."

END SUB