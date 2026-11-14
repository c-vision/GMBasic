' MiniTest CONSTRUCTS — ERRDEFER: cleanup eseguito SOLO se la procedura esce per un errore
' (equivalente di `errdefer` di Zig).
'
' Copre i tre percorsi che distinguono ERRDEFER da DEFER:
'   1. uscita NORMALE        → il cleanup NON corre;
'   2. RETURN anticipato     → il cleanup NON corre (la procedura non esce per errore);
'   3. errore che attraversa → cleanup in ordine LIFO + RILANCIO dello STESSO errore
'      (numero e descrizione conservati: il CATCH esterno li vede identici).
' In più: un errore catturato da un TRY *interno* non fa uscire la procedura, quindi il
' cleanup non corre (stessa regola del blocco di Zig).
' E una SECONDA procedura con ERRDEFER: i due binding impliciti non devono collidere
' (bug reale: due procedure con ERRDEFER non compilavano su IL/PCode — "la base è un valore
' costante" perché il nome del binding era lo stesso in entrambe).
'
' EXPECT: n 0 :entro|n 0 :normale|n 1 :entro|n 2 :entro|n 2 :B|n 2 :A|n2:catturato  11  boom|n 3 :entro|n 3 :interno|n 3 :normale|altra:A|altra:  13  altro|fine
SUB Rischiosa(n AS LONG)
    ERRDEFER PRINT "n"; n; ":A"
    ERRDEFER PRINT "n"; n; ":B"
    PRINT "n"; n; ":entro"
    IF n = 1 THEN RETURN
    IF n = 2 THEN ERROR 11, "boom"
    IF n = 3 THEN
        TRY
            ERROR 12, "interno"
        CATCH e AS ERR
            PRINT "n"; n; ":interno"
        END TRY
    END IF
    PRINT "n"; n; ":normale"
END SUB

' Seconda procedura con ERRDEFER (binding implicito distinto: vedi intestazione).
SUB Altra()
    ERRDEFER PRINT "altra:A"
    ERROR 13, "altro"
END SUB

SUB Main()
    Rischiosa(0)
    Rischiosa(1)
    TRY
        Rischiosa(2)
    CATCH e AS ERR
        PRINT "n2:catturato "; e.Number; " "; e.Description
    END TRY
    Rischiosa(3)
    TRY
        Altra()
    CATCH e AS ERR
        PRINT "altra: "; e.Number; " "; e.Description
    END TRY
    PRINT "fine"
END SUB
