' MiniTest CONSTRUCTS — PRINT ANNIDATO: un PRINT che scrive mentre è in corso un altro
' PRINT (es. dentro una FUNCTION valutata come argomento) NON deve distruggere il parziale
' del PRINT esterno né fondere le due righe.
'
' Canone (4 gambe): il PRINT interno emette la SUA riga, il PRINT esterno riprende dal
' proprio parziale e committa la riga completa alla fine.
'
'     [dentro]
'     b= 5
'
' Implementazione: lo staging di stampa è per-istruzione e ogni livello conosce il FRAME
' che l'ha aperto (C99: pila dei livelli in gmb_console.c; IL: _printStageSaved; PCode:
' PrintLineStagingFrame; VM/C: print_staging_frame) — un PRINT dentro una procedura
' chiamata come argomento ha un frame più interno e SOSPENDE il livello esterno.
'
' Un PRINT interrotto da un ERRORE invece NON deve emettere il parziale (canone separato,
' coperto da exceptions/: il salto all'handler scarta tutti i livelli).
'
' EXPECT: a= 1|[dentro]|b= 5|[dentro2]|c= 2  d= 6|fine
FUNCTION Dentro() AS LONG
    PRINT "[dentro]"
    Dentro = 5
END FUNCTION

FUNCTION Dentro2() AS LONG
    PRINT "[dentro2]"
    Dentro2 = 6
END FUNCTION

SUB Main()
    PRINT "a="; 1
    PRINT "b="; Dentro()
    PRINT "c="; 2; " d="; Dentro2()
    PRINT "fine"
END SUB
