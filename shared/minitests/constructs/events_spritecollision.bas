' TAG: interactive
' ON SPRITE COLLISION: l'handler riceve (x, y) della collisione. Il SYNC N rileva gli sprite visibili con i
' rettangoli sovrapposti, il SYNC N+1 chiama l'handler (una volta per SYNC, con la prima collisione;
' x, y = centro del rettangolo dello sprite con id minore): due SYNC => 1 chiamata. Il risultato viene
' stampato dopo SCREEN 0 (con lo schermo grafico attivo PRINT scrive sulla finestra, non su stdout).
' Ambiente: richiede un driver video (SDL_VIDEODRIVER=dummy va bene), per questo e' interactive.
' EXPECT: hits= 1|x= 12|y= 12
DIM Hits AS LONG = 0
DIM Cx AS DOUBLE = 0
DIM Cy AS DOUBLE = 0

SUB OnCollision(x AS DOUBLE, y AS DOUBLE)
    Hits = Hits + 1
    Cx = x
    Cy = y
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
    SYNC
    SYNC
    SCREEN 0
    PRINT "hits="; Hits
    PRINT "x="; Cx
    PRINT "y="; Cy
END SUB
