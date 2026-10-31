' FILE: Samples/Classes/ClassesEvents.bas
' Test RAISE EVENT, ADD HANDLER, REMOVE HANDLER

CLASS Contatore
    EVENT Incrementato(valore AS INTEGER)
    
    PRIVATE DIM m_valore AS INTEGER
    
    PUBLIC SUB NEW()
        SELF.m_valore = 0
    END SUB
    
    PUBLIC SUB Incrementa()
        SELF.m_valore = SELF.m_valore + 1
        RAISE EVENT Incrementato(SELF.m_valore)
    END SUB
    
    PUBLIC FUNCTION Valore() AS INTEGER
        RETURN SELF.m_valore
    END FUNCTION
END CLASS

DIM g_notifiche AS INTEGER
g_notifiche = 0

SUB Notifica(valore AS INTEGER)
    PRINT "Evento Incrementato: "; valore
    g_notifiche = g_notifiche + 1
END SUB

SUB Main()
    PRINT "=== Class Events Test ==="
    
    DIM c AS Contatore = NEW Contatore()
    
    ' Registra handler
    ADD HANDLER c.Incrementato, Notifica
    PRINT "Handler registrato"
    
    ' Incrementa (scatena evento)
    c.Incrementa()
    c.Incrementa()
    c.Incrementa()
    
    PRINT "Notifiche ricevute: "; g_notifiche
    PRINT "Valore contatore: "; c.Valore()
    
    ' Rimuovi handler
    REMOVE HANDLER c.Incrementato, Notifica
    PRINT "Handler rimosso"
    
    c.Incrementa()
    PRINT "Notifiche dopo remove: "; g_notifiche
    
    IF g_notifiche = 3 THEN
        PRINT "TEST PASSED"
    ELSE
        PRINT "TEST FAILED"
    END IF
END SUB
