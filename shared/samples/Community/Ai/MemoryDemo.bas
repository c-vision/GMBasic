' FILE: AI/MemoryDemo.bas
' =================================================================
' MEMORY store/load/search - memoria a breve e lungo termine
' =================================================================
IMPORT MEMORY

SUB Main()
    PRINT "Memory Store Demo"
    PRINT "================"
    PRINT

    ' --- 1. MEMORY.STORE ---
    MEMORY.STORE("user-name", "Mario Rossi", 0)
    MEMORY.STORE("user-email", "mario@example.com", 0)
    MEMORY.STORE("api-key-config", "sk-abc123", 1)
    PRINT "3 valori memorizzati."

    ' --- 2. MEMORY.LOAD ---
    DIM nome$ = MEMORY.LOAD("user-name")
    PRINT "Nome caricato: "; nome$

    DIM email$ = MEMORY.LOAD("user-email")
    PRINT "Email caricata: "; email$
    PRINT

    ' --- 3. MEMORY.SEARCH ---
    PRINT "Ricerca 'user':"
    DIM results$ = MEMORY.SEARCH("user")
    PRINT results$
    PRINT

    ' --- 4. MEMORY.LIST ---
    PRINT "Tutte le chiavi:"
    DIM allKeys$ = MEMORY.LIST()
    PRINT allKeys$
    PRINT

    ' --- 5. MEMORY.DELETE ---
    MEMORY.DELETE("api-key-config")
    PRINT "Dopo delete, lista:"
    PRINT MEMORY.LIST()
    PRINT

    PRINT "Test completato."
END SUB
