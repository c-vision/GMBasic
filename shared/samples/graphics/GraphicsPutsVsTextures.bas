' FILE: Graphics/GraphicsPutsVsTextures.bas
' =================================================================================================
'       GUIDA TEORICA: BENCHMARK GRAFICO - RENDERING CPU (IMAGEPUTS) vs. GPU (DRAW TEXTURES)
' =================================================================================================
'
' Questo programma confronta le performance tra il rendering software (CPU) e hardware (GPU),
' utilizzando i nuovi namespace specializzati per l'ottimizzazione dei dati.
'
' 1. RENDERING CPU-BOUND (IMAGEPUTS)
'    - IMAGEGET: Cattura un pattern di pixel dallo schermo e lo salva in un array.
'    - IMAGEPUTS: Disegna in batch un array di sprite usando la CPU per il calcolo dei pixel.
'    - Sintassi: `IMAGEPUTS SpriteArray, PositionsArray`
'
' 2. RENDERING GPU-BOUND (DRAW TEXTURES)
'    - LOAD TEXTURE: Carica un'immagine nella memoria video (VRAM) ottenendo un Handle.
'    - DRAW TEXTURES: Invia un unico comando alla GPU per disegnare migliaia di istanze.
'    - Sintassi: `DRAW TEXTURES TextureHandle, X(), Y(), Rot(), Scale()`
'
' 3. NAMESPACE VECTOR (OTTIMIZZAZIONE DATI)
'    Per gestire migliaia di coordinate senza rallentare il BASIC, si usa il namespace VECTOR
'    che opera in "batch" sugli array. Supporta due sintassi:
'    - Legacy (Comando): `VECTOR ADD Pos(), Vel()`
'    - Moderna (legacy): `VECTOR ADD Pos, Vel` (comando senza parentesi)
'
' =================================================================================================

' Importa i nuovi namespace che sostituiscono il vecchio LINALG
IMPORT VECTOR
IMPORT GRAPHICS
IMPORT CONSOLE
IMPORT STRINGS

SUB Main()
  ' Sprite software (byte array) e Texture hardware (handle LONG)
  DIM ShipSprite(150) AS INTEGER
  DIM ShipTextureID AS LONG
  
  SCREEN 12
  FONT "font.ttf", 12
  
  CCLS
  CPRINT "FASE 1: Setup delle risorse..."
  
  ' --- Preparazione dello Sprite Software: cattura i pixel ARGB veri di sprite.png ---
  DIM TmpSprite AS LONG
  SPRITE LOAD "sprite.png", TmpSprite
  IF TmpSprite = 0 THEN
    CCLS : CPRINT "ERRORE: Impossibile caricare 'sprite.png'."
    WAITKEY : END
  END IF
  ' GETPIXELS preserva la TRASPARENZA vera del PNG (-1 per alfa<128): a differenza di IMAGEGET
  ' da schermo (dove il SYNC blendava il PNG sul colore di sfondo e lo sprite catturato aveva
  ' il RIQUADRO che copriva gli sprite sotto), ora i pixel trasparenti restano trasparenti.
  SPRITE GETPIXELS TmpSprite, ShipSprite
  SPRITE OFF TmpSprite ' non ridisegnarlo nel loop dei benchmark
  SCREENSET 0, 0 ' attiva=visibile=0: il SYNC+IMAGEPUTS disegnano sulla pagina che il Present mostra (come sprites5)
  
  ' --- Preparazione della Texture Hardware ---
  ' Carica la texture direttamente in VRAM
  LOAD TEXTURE "sprite.png" INTO ShipTextureID
  
  IF ShipTextureID = 0 THEN
    CCLS : CPRINT "ERRORE: Impossibile caricare 'sprite.png'."
    WAITKEY : END
  END IF
  
  ' Benchmark Parameters
  DIM SwCounts() AS LONG = {10, 100, 500}
  DIM HwCounts() AS LONG = {500, 1000, 1500, 2500}
  
  ' Test Software (CPU)
  DIM I AS INTEGER
  FOR I = 0 TO UBOUND(SwCounts)
    ShowIntroScreen(I + 1, 7, SwCounts(I), "CPU / SOFTWARE MODE", 0)
    TestSoftware(ShipSprite, SwCounts(I))
  NEXT I
  
  ' Test Hardware (GPU)
  FOR I = 0 TO UBOUND(HwCounts)
    DIM spriteScale AS DOUBLE = 500 / HwCounts(I)
    ShowIntroScreen(I + 4, 7, HwCounts(I), "GPU / HARDWARE ACCELERATED", spriteScale)
    TestHardware(ShipTextureID, HwCounts(I), spriteScale)
  NEXT I
  
  ShowIntroScreen(0, 0, 0, "TEST COMPLETATI", 0)
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

' RENDERING SOFTWARE: doppio buffer — pattern professionale.
' Ogni frame: SCREENSET ,WorkPage (attiva = back buffer) → CLS (cancella il back) →
' IMAGEPUTS (disegna sul back) → SCREENSET WorkPage (rendi visibile la pagina appena
' disegnata — il 1° arg di SCREENSET è la pagina da MOSTRARE) → SYNC (barriera di
' presentazione: il Present mostra SOLO su SYNC, mai a metà frame). Niente lampeggio.
SUB TestSoftware(BYREF Sprite() AS INTEGER, MAX AS LONG)
  DIM Positions(MAX * 2 - 1) AS INTEGER
  DIM X#(MAX), Y#(MAX), DX#(MAX), DY#(MAX)
  
  ' Usa i comandi vettoriali per l'inizializzazione
  VECTOR FILL RANDOM X, 0, 630
  VECTOR FILL RANDOM Y, 0, 470
  VECTOR FILL RANDOM DX, -2, 2
  VECTOR FILL RANDOM DY, -2, 2
  
  DIM T_START AS DOUBLE = TI(), FRAMES AS LONG = 0
  DIM T0 AS DOUBLE = TI()
  DIM WorkPage AS INTEGER = 1
  DIM I AS INTEGER, K$ AS STRING
  
  COLOR 15, 1 ' sfondo blu: CLS senza argomento usa _backgroundColor → coincide col fallback del Present
  DO
    SCREENSET , WorkPage : CLS ' back buffer: cancella (colore sfondo corrente) + disegneremo qui

    FOR I = 0 TO MAX - 1
      X#(I) = X#(I) + DX#(I) : Y#(I) = Y#(I) + DY#(I)
      IF X#(I) < 0 OR X#(I) > 630 THEN DX#(I) = -DX#(I)
      IF Y#(I) < 0 OR Y#(I) > 470 THEN DY#(I) = -DY#(I)
      Positions(I * 2) = CINT(X#[I])
      Positions(I * 2 + 1) = CINT(Y#[I])
    NEXT I
    
    ' Disegno batch software (CPU) sul back buffer
    IMAGEPUTS Sprite, Positions
    
    FRAMES = FRAMES + 1
    IF TI() - T_START >= 1000 THEN
      DIM FPS AS DOUBLE = FRAMES / ((TI() - T_START) / 1000)
      CLOCATE 1, 1 : CPRINT "MODE: CPU | SPRITES: " + STR(MAX) + " | FPS: " + FORMAT(FPS, "#0.0")
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
    DIM WorkPage AS INTEGER = 1
    DIM K$ AS STRING
    
    DO
      SCREENSET , WorkPage : CLS 1 ' back buffer: cancella + disegneremo qui

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
    LOOP
END SUB