' TILEMAP Engine Demo
' Dimostra caricamento tilemap, camera offset e scrolling

SCREEN 13

DIM texId AS INTEGER
DIM tileW AS INTEGER = 16
DIM tileH AS INTEGER = 16
DIM tilesPerRow AS INTEGER = 8
DIM mapW AS INTEGER = 20
DIM mapH AS INTEGER = 15
DIM tiles(299) AS INTEGER  ' 20*15

' Inizializza tilemap vuota (tutti tile 0)
DIM i AS INTEGER
FOR i = 0 TO 299
    tiles(i) = 0
NEXT i

' Crea un pattern checkerboard
DIM row AS INTEGER, col AS INTEGER
FOR row = 0 TO mapH - 1
    FOR col = 0 TO mapW - 1
        IF (row + col) MOD 2 = 0 THEN
            tiles(row * mapW + col) = 1
        ELSE
            tiles(row * mapW + col) = 2
        END IF
    NEXT col
NEXT row

' Carica tileset (placeholder - usa un file esistente)
LOAD TEXTURE "assets/tileset.png" INTO texId

' Inizializza tilemap
TILEMAP LOAD texId, tileW, tileH, tilesPerRow, mapW, mapH, tiles

' Disegna la tilemap
TILEMAP DRAW
SYNC

' Scrolling demo
DIM scrollX AS INTEGER = 0
DIM running AS BOOL = TRUE
DIM key$

TICK 30
    ' Scorrimento automatico
    scrollX = scrollX + 1
    TILEMAP SCROLL 1, 0
    
    ' Ridisegna
    CLS
    TILEMAP DRAW
    SYNC
    
    ' Esci con un tasto
    INKEY key$
    IF key$ <> "" THEN
        running = FALSE
    END IF
    
    IF NOT running THEN
        EXIT TICK
    END IF
END TICK

' Reset tile specifico
TILEMAP SET 7, 10, 3
TILEMAP OFFSET 0, 0
CLS
TILEMAP DRAW
SYNC

WAITKEY
SCREEN 0
