' FILE: Exceptions/ExceptionsStackTrace.bas
' =================================================================================================
'       GUIDA TEORICA: STACK TRACE BASIC (catena delle procedure sull'errore)
' =================================================================================================
'
' Quando un errore attraversa più procedure, la sola RIGA non basta: serve sapere QUALE
' procedura e da CHI era stata chiamata. `e.StackTrace` (variabile del CATCH) e
' `ERR.StackTrace` (pseudo-oggetto) restituiscono la catena, dalla procedura più interna
' alla più esterna, con la riga corrente di ciascuna:
'
'     in ValidaImporto (riga 42)
'     in ValidaRiga (riga 31)
'     in ElaboraFile (riga 22)
'     in Main (riga 15)
'
' Lo stesso testo compare anche sul messaggio d'errore FATALE (stderr):
'
'     [GMBASIC] ERROR 11: importo non valido (line 42)
'       in ValidaImporto (riga 42)
'       ...
'
' Questo sample mostra tre cose:
'   1. un errore in profondità, catturato con TRY/CATCH: catena completa in `e.StackTrace`;
'   2. la stessa catena letta da `ERR.StackTrace` (pseudo-oggetto del linguaggio);
'   3. che dopo un errore catturato NON restano frame fantasma: un secondo errore
'      sollevato nel chiamante riporta solo il chiamante.
' =================================================================================================

' --- livello 3: il punto in cui l'errore nasce ------------------------------------------------
SUB ValidaImporto(importo AS DOUBLE)
    IF importo < 0 THEN
        ERROR 11, "importo non valido"
    END IF
    PRINT "  importo ok: "; importo
END SUB

' --- livello 2: il chiamante intermedio -------------------------------------------------------
SUB ValidaRiga(riga AS LONG, importo AS DOUBLE)
    PRINT "  riga "; riga
    ValidaImporto(importo)
END SUB

' --- livello 1: il chiamante esterno ----------------------------------------------------------
SUB ElaboraFile()
    ValidaRiga(1, 100)
    ValidaRiga(2, -5)          ' ← qui l'errore risale
    PRINT "  mai eseguito"
END SUB

SUB Main()
    PRINT "== errore in profondita' (catena completa) =="
    TRY
        ElaboraFile()
    CATCH e AS ERR
        PRINT "numero: "; e.Number
        PRINT "descrizione: "; e.Description
        PRINT "catena:"
        PRINT e.StackTrace
        PRINT "catena da ERR.StackTrace:"
        PRINT ERR.StackTrace
    END TRY

    PRINT "== dopo il catch: nessun frame fantasma =="
    TRY
        ERROR 12, "errore locale"
    CATCH e AS ERR
        PRINT "catena:"
        PRINT e.StackTrace
    END TRY

    PRINT "== fine =="
END SUB
