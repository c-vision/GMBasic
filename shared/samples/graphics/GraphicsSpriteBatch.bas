' FILE: Graphics/GraphicsSpriteBatch.bas
' DRAW TEXTURES batch: array paralleli

SUB Main()
    CLS
    PRINT "GraphicsSpriteBatch - DRAW TEXTURES batch demo"

    SCREEN 1, 800, 600, 32
    IF SCREEN <> 1 THEN PRINT "ERRORE schermo": END

    DIM texId = 0
    LOAD TEXTURE "sprite.png" INTO texId
    IF texId = 0 THEN PRINT "ERRORE texture": END

    ' Array paralleli
    DIM N = 100
    DIM X(N) AS DOUBLE
    DIM Y(N) AS DOUBLE
    DIM Rot(N) AS DOUBLE
    DIM Scale(N) AS DOUBLE

    FOR i = 0 TO N - 1
        X(i) = 50 + (i MOD 10) * 70
        Y(i) = 50 + (i \ 10) * 50
        Rot(i) = i * 15
        Scale(i) = 0.5 + (i MOD 5) * 0.2
    NEXT i

    DRAW TEXTURES texId, X(), Y(), Rot(), Scale()
    PRINT "Disegnati "; N; " sprite via array paralleli"

    WAITKEY
    SCREEN 0
    PRINT "OK."
END SUB
