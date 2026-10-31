' FILE: Graphics/GraphicsUnloadResource.bas
' UNLOAD TEXTURE / UNLOAD FONT — rilascio esplicito risorse grafiche

SUB Main()
    CLS
    PRINT "GraphicsUnloadResource - UNLOAD TEXTURE / UNLOAD FONT"

    SCREEN 1, 800, 600, 32
    IF SCREEN <> 1 THEN PRINT "ERRORE schermo": END

    ' Carica texture
    DIM texId = 0
    LOAD TEXTURE "sprite.png" INTO texId
    IF texId = 0 THEN PRINT "ERRORE texture": END
    PRINT "Texture caricata: ID "; texId

    ' Disegna con la texture
    DRAW TEXTURE texId, 100, 100
    PRINT "Texture disegnata"

    ' Rilascia la texture
    UNLOAD TEXTURE texId
    PRINT "Texture scaricata: ID "; texId

    ' Carica font
    FONT "font.ttf", 12
    PRINT "Font caricato"

    ' Rilascia font
    UNLOAD FONT
    PRINT "Font scaricato"

    WAITKEY
    SCREEN 0
    PRINT "OK. UNLOAD test completato."
END SUB
