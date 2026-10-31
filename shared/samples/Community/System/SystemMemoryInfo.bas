' FILE: System/SystemMemoryInfo.bas
' ======================================================================================================================
' GUIDA: memoria di sistema con SYSTEM.SYS(...) — funzione attendibile (API native del SO)
' ======================================================================================================================
' SYSTEM.SYS(prop) ritorna la proprietà come stringa. Proprietà di memoria:
'   MEMAVAILABLE / MEMFREE / MEMRECLAIMABLE : byte disponibili+reclamabili (senza swap)
'   MEMTOTAL                                : byte di RAM totale
'   MEMUSED                                 : byte usati (totale − disponibile)
'   MEMAVAILABLEMB / MEMTOTALMB             : valori in MB
' Stessa implementazione (runtime C condiviso / IlRuntime) su TUTTI e 5 i backend.
' ======================================================================================================================
IMPORT SYSTEM WITH ALIAS

SUB Main()
    PRINT "Memoria di sistema (SYSTEM.SYS)"
    PRINT "=============================="
    PRINT "RAM totale          : "; SYSTEM.SYS("MEMTOTALMB"); " MB"
    PRINT "Disponibile+reclam. : "; SYSTEM.SYS("MEMAVAILABLEMB"); " MB"
    PRINT "Usata               : "; VAL(SYSTEM.SYS("MEMUSED")) / 1048576; " MB"

    ' Verifica di sanità: totale e disponibile devono essere > 0 e usata <= totale.
    DIM tot AS DOUBLE, av AS DOUBLE, us AS DOUBLE
    tot = VAL(SYSTEM.SYS("MEMTOTAL"))
    av  = VAL(SYSTEM.SYS("MEMAVAILABLE"))
    us  = VAL(SYSTEM.SYS("MEMUSED"))
    IF tot > 0 AND av > 0 AND us <= tot THEN
        PRINT "mem_check: ok"
    ELSE
        PRINT "mem_check: fail"
    END IF

    ' Uso pratico: prima di caricare un modello MLX (che carica tutto in RAM),
    ' confronta la dimensione del modello con MEMAVAILABLE.
    PRINT "Esempio: un modello che richiede 99 GB NON entra con "; av / 1073741824; " GB disponibili."
    PRINT "Test completato."
END SUB
