' FILE: Graphics/GraphicsTexturesBenchmark.bas — Benchmark GPU-only (DRAW TEXTURES)
' Fasi: 100/200/500/1000 sprite con doppio buffer corretto (SCREENSET WorkPage + SYNC barriera).
' La scala è adattiva (100/SQR(n)): gli sprite restano leggibili a tutti i conteggi.
IMPORT VECTOR
IMPORT MATRIX
IMPORT GRAPHICS
IMPORT CONSOLE
IMPORT STRINGS

SUB Main()
  DIM ShipTextureID AS LONG
  
  SCREEN 12
  FONT "font.ttf", 12
  
  CCLS
  CPRINT "FASE 1: Setup delle risorse..."
  
  ' --- Preparazione della Texture Hardware ---
  LOAD TEXTURE "sprite.png" INTO ShipTextureID
  
  IF ShipTextureID = 0 THEN
    CCLS : CPRINT "ERRORE: Impossibile caricare 'sprite.png'."
    WAITKEY : END
  END IF
  
  ' Benchmark Parameters — SOLO GPU
  DIM HwCounts() AS LONG = {500, 1000, 1500, 2500}
  
  DIM I AS INTEGER
  FOR I = 0 TO UBOUND(HwCounts)
    DIM spriteScale AS DOUBLE = 500 / HwCounts(I)
    ShowIntroScreen(I + 1, 4, HwCounts(I), "GPU / HARDWARE ACCELERATED", spriteScale)
    TestHardware(ShipTextureID, HwCounts(I), spriteScale)
  NEXT I
  
  ShowIntroScreen(0, 0, 0, "TEST GPU COMPLETATI", 0)
  WAITKEY
  SCREEN 0
END SUB

' Helper per la visualizzazione dell'interfaccia di test
SUB ShowIntroScreen(TestNum AS LONG, Total AS LONG, Num AS LONG, Mode$ AS STRING, sc AS DOUBLE)
    SCREENSET , 0 : CLS
    FONT "font.ttf", 12 : COLOR 14, 0
    DIM L1$ = FORMAT(Num, "#,##0") + " SPRITES"
    LOCATE 160, (640 - TEXTWIDTH(L1$)) / 2 : PRINT L1$
    
    FONT "font.ttf", 12 : COLOR 15, 0
    DIM L2$ = "MODE: " + Mode$
    LOCATE 230, (640 - TEXTWIDTH(L2$)) / 2 : PRINT L2$
    
    DIM L3$ = "PREMI UN TASTO PER INIZIARE..."
    LOCATE 320, (640 - TEXTWIDTH(L3$)) / 2 : PRINT L3$
    
    IF TestNum > 0 THEN
        DIM L4$ = "TEST " + STR(TestNum) + " DI " + STR(Total)
        LOCATE 420, (640 - TEXTWIDTH(L4$)) / 2 : PRINT L4$
    END IF
    
    SCREENSET 0 : SLEEP 500 : WAITKEY : CLS : FONT "font.ttf", 12
END SUB

' RENDERING HARDWARE: Uso massiccio del namespace VECTOR (Dual Syntax)
SUB TestHardware(TextureID AS LONG, MAX AS LONG, spriteScale AS DOUBLE)
    DIM X#(MAX), Y#(MAX), DX#(MAX), DY#(MAX)
    DIM Rot#(MAX), DRot#(MAX), Scales#(MAX)
    
    ' 1. Inizializzazione (Sintassi Legacy e Moderna)
    VECTOR FILL RANDOM X, 0, 640       ' Legacy
    VECTOR FILL RANDOM Y, 0, 480         ' Moderna
    VECTOR FILL RANDOM DX, -2, 2      ' Legacy
    VECTOR FILL RANDOM DY, -2, 2         ' Moderna
    VECTOR FILL RANDOM Rot, 0, 360    ' Legacy
    VECTOR FILL RANDOM DRot, -3, 3       ' Moderna
    VECTOR SET Scales, spriteScale      ' Moderna
  
    DIM T_START AS DOUBLE = TI(), FRAMES AS LONG = 0
    DIM T0 AS DOUBLE = TI()
    DIM WorkPage AS INTEGER = 1
    DIM K$ AS STRING
    
    COLOR 15, 1 ' sfondo blu
    DO
      SCREENSET , WorkPage : CLS ' back buffer: cancella (colore sfondo corrente) + disegneremo qui

      ' 2. Aggiornamento dati (Sintassi Legacy e Moderna)
      VECTOR ADD X, DX              ' Legacy: x = x + dx
      VECTOR ADD Y, DY                 ' Moderna: y = y + dy
      
      ' Logica di rimbalzo batch (Legacy)
      VECTOR BOUNCE2D X, Y, DX, DY, 0, 0, 640, 480
      
      ' Incremento rotazione con wrap (Moderna)
      VECTOR ADD WRAP Rot, DRot, 0, 360
  
      ' 3. Rendering Batch (Inviato alla GPU) sul back buffer
      ' La sintassi corretta per passare un array è senza parentesi
      DRAW TEXTURES TextureID, X, Y, Rot, Scales
  
      FRAMES = FRAMES + 1
      IF TI() - T_START > 1000 THEN
        DIM FPS AS DOUBLE = FRAMES / ((TI() - T_START) / 1000)
        CLOCATE 1, 1 : CPRINT "MODE: GPU | SPRITES: " + STR(MAX) + " | FPS: " + FORMAT(FPS, "#0.0")
        T_START = TI() : FRAMES = 0
      END IF
      SCREENSET WorkPage ' rendi visibile la pagina appena disegnata
      SYNC ' barriera: il Present mostra SOLO su SYNC
      INKEY K$ : IF K$ = CHR(27) THEN EXIT DO
      SLEEP 16 ' frame limit ~60fps
      IF TI() - T0 >= 5000 THEN EXIT DO
    LOOP
    WAITKEY ' osserva l'FPS della fase prima di passare alla successiva
END SUB
