' TICK...END TICK - Game Loop Demo
' Mostra un game loop sincronizzato a 60 FPS con contatore

SCREEN 13
FONT "font.ttf", 12

DIM frameCount AS INTEGER = 0
DIM startTime AS INTEGER = 0
DIM fps AS INTEGER = 0
DIM running AS BOOL = TRUE

startTime = TICKS

TICK 60
    ' Game logic
    frameCount = frameCount + 1
    
    ' Calcola FPS ogni secondo
    IF TICKS - startTime >= 1000 THEN
        fps = frameCount
        frameCount = 0
        startTime = TICKS
        
        ' Mostra FPS
        LOCATE 1, 1
        PRINT "FPS: "; fps
        
        ' Esci dopo 5 secondi
        IF fps > 0 AND TICKS > 5000 THEN
            running = FALSE
        END IF
    END IF
    
    ' Sincronizza il frame
    SYNC
    
    ' Esci dal loop
    IF NOT running THEN
        EXIT TICK
    END IF
END TICK

LOCATE 30, 1
PRINT "TICK loop terminato dopo 5 secondi"

WAITKEY
SCREEN 0
