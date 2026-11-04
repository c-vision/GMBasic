' FILE: Graphics/GraphicsDrawTextures.bas
' =================================================================================================
'       GUIDA TEORICA: RENDERING ACCELERATO, SPRITESHEET E POLIGONI
' =================================================================================================
'
' Questo test verifica comandi grafici avanzati di GM-BASIC, progettati per la performance
' e la gestione efficiente delle risorse grafiche.
'
' 1. BATCH RENDERING CON `DRAW TEXTURES` (ARRAY PARALLELI)
'    - Comando: `DRAW TEXTURES textureID, x_array(), y_array(), rot_array(), scale_array()`
'    - Scopo: Inviare i dati di migliaia di sprite alla GPU in una singola chiamata, minimizzando
'      l'overhead e massimizzando le prestazioni per scene complesse.
'
' 2. BATCH RENDERING CON `DRAW TEXTURES` (OGGETTI SPRITE)
'    - Comando: `DRAW TEXTURES sprite_object_array()`
'    - Scopo: Semplificare la gestione dei dati raggruppando le proprietà di ogni sprite
'      (ID, posizione, rotazione, scala, colore) in un unico oggetto `TYPE SpriteInfo`.
'
' 3. GESTIONE DI SPRITESHEET CON `LOAD TEXTURE ... CELL()`
'    - Comando: `LOAD TEXTURE "sheet.png" INTO idArray, CELL(larghezza, altezza)`
'    - Scopo: Caricare un'unica immagine (spritesheet) e dividerla in più sub-texture.
'
' 4. DISEGNO DI POLIGONI PIENI CON `POLYGON`
'    - Comando: `POLYGON punti_array(), colore`
'    - Scopo: Fornire un modo nativo e performante per disegnare forme poligonali complesse.
'
' =================================================================================================

IMPORT GRAPHICS
IMPORT CONSOLE
IMPORT STRINGS
IMPORT VECTOR

' Definiamo la struttura dati per i vertici del poligono
TYPE Point2D
    X AS DOUBLE
    Y AS DOUBLE
END TYPE

SUB Main()
  ' Inizializza la modalità grafica e carica il font per il testo informativo.
  SCREEN 12
  FONT "font.ttf", 12
  
  ' Esegue le tre dimostrazioni in sequenza
  DemoDrawTextures()
  DemoSpritesheet()
  DemoPolygon()
  DemoSpriteObject()
  
  ' Schermata finale di conferma.
  CLS
  COLOR 14, 0
  FONT "font.ttf", 12
  CenterPrint(200, "TEST COMPLETATO")
  CenterPrint(250, "Premi un tasto per tornare alla REPL.")
  WAITKEY
  ' Torna alla modalità testo (chiude la finestra grafica).
  SCREEN 0
END SUB

' === DEMO 1: DRAW TEXTURES (Batch Rendering con Array Paralleli) ===
SUB DemoDrawTextures()
    DIM ShipTextureID AS LONG
    DIM NumSprites AS LONG: NumSprites = 100
    DIM Scale AS DOUBLE: Scale = 1.0
  
    CCLS : CPRINT "Caricamento texture 'sprite.png'..."
    LOAD TEXTURE "sprite.png" INTO ShipTextureID
    IF ShipTextureID = 0 THEN
        CCLS : CPRINT "ERRORE: Impossibile caricare 'sprite.png'."
        WAITKEY : END
    END IF
  
    ShowIntroScreen("Test di DRAW TEXTURES (Array Paralleli)", FORMAT(NumSprites, "#,##0") + " sprites")
  
    CPRINT "--> Esecuzione di DRAW TEXTURES..."
    CPRINT "    Se vedi gli sprite, il test ha avuto successo."

    DIM X(NumSprites) AS DOUBLE, Y(NumSprites) AS DOUBLE
    DIM Rot(NumSprites) AS DOUBLE, Scales(NumSprites) AS DOUBLE
    
    VECTOR FILL RANDOM X(), 50, 590
    VECTOR FILL RANDOM Y(), 50, 430
    VECTOR FILL RANDOM Rot(), 0, 360
    VECTOR SET Scales(), Scale
  
    SCREENSET , 1
    CLS 1 

    ' SINTASSI CORRETTA RIPRISTINATA: Usa () per passare l'intero array.
    DRAW TEXTURES ShipTextureID, X(), Y(), Rot(), Scales()

    SCREENSET 1
    
    WAITKEY
END SUB

' === DEMO 2: LOAD TEXTURE con SPRITESHEET ===
SUB DemoSpritesheet()
    DIM SpriteIDs(15) AS LONG
  
    CCLS : CPRINT "Caricamento spritesheet 'spritesheet.png'..."
    
    ' La sintassi corretta per INTO è senza parentesi, perché è un'assegnazione.
    LOAD TEXTURE "spritesheet.png" INTO SpriteIDs, CELL(32, 32)
    
    IF SpriteIDs(0) = 0 THEN
        CCLS : CPRINT "ERRORE: Impossibile caricare 'spritesheet.png'."
        WAITKEY : END
    END IF

    ShowIntroScreen("Test di SPRITESHEET", "16 sprites da un unico file")

    CPRINT "--> Esecuzione di DRAW TEXTURE con ID da spritesheet..."
    
    SCREENSET , 1
    CLS 1
    
    DIM i, j AS INTEGER
    FOR i = 0 TO 3
        FOR j = 0 TO 3
            DIM id = SpriteIDs(i * 4 + j)
            DIM xPos = 100 + j * 100
            DIM yPos = 50 + i * 100
            DRAW TEXTURE id, xPos, yPos, (i+j)*15, 2.5, 2.5
        NEXT j
    NEXT i
    
    SCREENSET 1
    
    WAITKEY
END SUB

' === DEMO 3: POLYGON ===
SUB DemoPolygon()
    ShowIntroScreen("Test di POLYGON", "Disegno di un poligono pieno")

    CPRINT "--> Esecuzione di POLYGON..."
    CPRINT "    Se vedi un esagono colorato, il test ha avuto successo."

    DIM Hexagon(5) AS Point2D
    Hexagon(0).X = 320 : Hexagon(0).Y = 100
    Hexagon(1).X = 420 : Hexagon(1).Y = 160
    Hexagon(2).X = 420 : Hexagon(2).Y = 280
    Hexagon(3).X = 320 : Hexagon(3).Y = 340
    Hexagon(4).X = 220 : Hexagon(4).Y = 280
    Hexagon(5).X = 220 : Hexagon(5).Y = 160

    SCREENSET , 1
    CLS 1

    ' SINTASSI CORRETTA RIPRISTINATA: Usa () per passare l'intero array.
    POLYGON Hexagon(), 14

    SCREENSET 1
    
    WAITKEY
END SUB

' === DEMO 4: DRAW TEXTURES con OGGETTI SPRITE ===
SUB DemoSpriteObject()
    DIM ShipTextureID AS LONG
    DIM NumSprites AS LONG: NumSprites = 150
  
    CCLS : CPRINT "Caricamento texture 'sprite.png' per test oggetti..."
    LOAD TEXTURE "sprite.png" INTO ShipTextureID
    IF ShipTextureID = 0 THEN
        CCLS : CPRINT "ERRORE: Impossibile caricare 'sprite.png'."
        WAITKEY : END
    END IF

    ShowIntroScreen("Test di DRAW TEXTURES (Oggetti)", FORMAT(NumSprites, "#,##0") + " sprites")

    CPRINT "--> Esecuzione di DRAW TEXTURES con un array di oggetti SpriteInfo..."
    
    DIM Sprites(NumSprites - 1) AS SpriteInfo
    DIM Cols AS INTEGER: Cols = 15
    DIM CellW AS DOUBLE: CellW = 640 / Cols
    DIM CellH AS DOUBLE: CellH = 480 / 10
    
    DIM i AS INTEGER
    FOR i = 0 TO UBOUND(Sprites)
        Sprites(i).TextureID = ShipTextureID
        Sprites(i).X = CellW * 0.5 + (i MOD Cols) * CellW
        Sprites(i).Y = CellH * 0.5 + (i \ Cols) * CellH
        Sprites(i).Rotation = RND() * 360
        Sprites(i).Scale = 0.4 + RND() * 0.2
        Sprites(i).Color = CHOOSE(INT(RND()*4)+1, -1, -256, -16776961, -65281)
    NEXT i

    SCREENSET , 1
    CLS 1
    
    ' SINTASSI CORRETTA RIPRISTINATA: Usa () per passare l'intero array.
    DRAW TEXTURES Sprites()
    
    SCREENSET 1
    
    WAITKEY
END SUB

' Helper per centrare il testo orizzontalmente.
SUB CenterPrint(Y AS INTEGER, Text$ AS STRING)
    LOCATE Y, (640 - TEXTWIDTH(Text$)) / 2
    PRINT Text$
END SUB

' Mostra una schermata di attesa generica.
SUB ShowIntroScreen(Title$ AS STRING, Subtitle$ AS STRING)
    SCREENSET , 0: CLS
    FONT "font.ttf", 12: COLOR 15, 0
    CenterPrint(200, Title$)
    FONT "font.ttf", 12: COLOR 14, 0
    CenterPrint(250, Subtitle$)
    FONT "font.ttf", 12: COLOR 15, 0
    CenterPrint(300, "Premi un tasto per continuare...")
    SCREENSET 0
    WAITKEY
    CLS : FONT "font.ttf", 12
END SUB