' FILE: Exceptions/ExceptionsErrDefer.bas
' =================================================================================================
'       GUIDA TEORICA: ERRDEFER — cleanup SOLO sul percorso d'errore
' =================================================================================================
'
' `DEFER` esegue il cleanup a OGNI uscita del blocco. `ERRDEFER` lo esegue SOLO quando la
' procedura esce perché un errore la attraversa (equivalente di `errdefer` di Zig).
'
' Il caso tipico è la risorsa che va rilasciata solo se il lavoro NON è andato a buon fine:
' annullare una transazione, cancellare un file temporaneo, ripristinare uno stato.
'
' Questo sample mostra:
'   1. uscita NORMALE      → DEFER corre, ERRDEFER no;
'   2. RETURN anticipato   → DEFER corre, ERRDEFER no;
'   3. errore che sfugge   → ERRDEFER corre (in ordine LIFO) e l'errore è RILANCIATO
'                            identico (numero e descrizione conservati); il DEFER NON corre:
'                            il suo epilogo è legato alle uscite normali, quindi i due
'                            costrutti si complementano.
' =================================================================================================

' --- procedura con entrambi i tipi di cleanup ------------------------------------------------
SUB Elabora(n AS LONG)
    DEFER PRINT "  [defer]    sempre"
    ERRDEFER PRINT "  [errdefer] solo su errore (A)"
    ERRDEFER PRINT "  [errdefer] solo su errore (B)"
    PRINT "  lavoro (n="; n; ")"

    IF n = 1 THEN RETURN                       ' uscita anticipata: solo DEFER
    IF n = 2 THEN ERROR 11, "elaborazione fallita"
    PRINT "  lavoro completato"
END SUB

' --- caso realistico: file temporaneo che va rimosso solo se il lavoro fallisce ---------------
SUB ElaboraFile(percorso AS STRING, fallisci AS LONG)
    OPEN percorso FOR OUTPUT AS #1
    ERRDEFER CLOSE #1
    ERRDEFER KILL percorso

    PRINT #1, "riga di lavoro"
    IF fallisci = 1 THEN ERROR 12, "scrittura rifiutata"
    CLOSE #1
    PRINT "  file scritto: "; percorso
END SUB

SUB Main()
    PRINT "== 1. uscita normale =="
    Elabora(0)

    PRINT "== 2. RETURN anticipato =="
    Elabora(1)

    PRINT "== 3. errore che sfugge =="
    TRY
        Elabora(2)
    CATCH e AS ERR
        PRINT "  catturato "; e.Number; ": "; e.Description
    END TRY

    PRINT "== 4. file: errore -> il cleanup rimuove il file =="
    TRY
        ElaboraFile("work/errdefer_temp.txt", 1)
    CATCH e AS ERR
        PRINT "  catturato "; e.Number; ": "; e.Description
    END TRY

    PRINT "== 5. file: successo -> il file resta =="
    ElaboraFile("work/errdefer_ok.txt", 0)

    PRINT "== fine =="
END SUB
