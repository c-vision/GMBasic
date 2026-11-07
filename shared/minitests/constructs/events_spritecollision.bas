' TAG: interactive
' ON SPRITE COLLISION / COLLISIONS. Il SYNC N rileva gli sprite visibili con i rettangoli sovrapposti, il SYNC N+1
' chiama gli handler: ON SPRITE COLLISION una volta con (x, y) della prima collisione (centro del rettangolo dello
' sprite con id minore), ON SPRITE COLLISIONS una volta con l'array degli id coinvolti. Due SYNC => 1 chiamata a testa.
' Il risultato viene stampato dopo SCREEN 0 (con lo schermo grafico attivo PRINT scrive sulla finestra, non su stdout).
' Ambiente: richiede un driver video (SDL_VIDEODRIVER=dummy va bene), per questo e' interactive.
' EXPECT: hits= 1|x= 12|y= 12|calls= 1|n= 2|sum= 3
DIM Hits AS LONG = 0
DIM Cx AS DOUBLE = 0
DIM Cy AS DOUBLE = 0
DIM Calls AS LONG = 0
DIM Nids AS LONG = 0
DIM SumIds AS DOUBLE = 0

SUB OnCollision(x AS DOUBLE, y AS DOUBLE)
    Hits = Hits + 1
    Cx = x
    Cy = y
END SUB

SUB OnCollisions(ids() AS DOUBLE)
    Calls = Calls + 1
    Nids = UBOUND(ids) + 1
    SumIds = 0
    FOR i = 0 TO UBOUND(ids)
        SumIds = SumIds + ids(i)
    NEXT
END SUB

SUB Main()
    SCREEN 64, 64
    DIM SpA
    DIM SpB
    SPRITE LOAD "../../samples/Fixtures/sprite_dot.png", SpA
    SPRITE LOAD "../../samples/Fixtures/sprite_dot.png", SpB
    SPRITE SET POSITION SpA, 10, 10
    SPRITE SET POSITION SpB, 10, 10
    SPRITE ON SpA
    SPRITE ON SpB
    ON SPRITE COLLISION OnCollision
    ON SPRITE COLLISIONS OnCollisions
    SYNC
    SYNC
    SCREEN 0
    PRINT "hits="; Hits
    PRINT "x="; Cx
    PRINT "y="; Cy
    PRINT "calls="; Calls
    PRINT "n="; Nids
    PRINT "sum="; SumIds
END SUB
