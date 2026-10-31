' FILE: Graphics/GraphicsPutsVsTextures.bas
' =================================================================================================
'       GUIDA TEORICA: BENCHMARK GRAFICO - RENDERING CPU (IMAGEPUTS) vs. GPU (DRAW TEXTURES)
' =================================================================================================
'
' GM-BASIC permette il calcolo ad alte prestazioni tramite i namespace MATRIX e VECTOR.
' Questo benchmark confronta il blitting software con il rendering accelerato hardware.
'
' 1. SINTASSI DUALE (MODALITÀ DI COMANDO vs METODO)
'    Ogni operazione nel namespace VECTOR supporta due stili:
'    - Legacy Prefissata: `VECTOR ADD Dest(), Src()` - Comando in-place senza parentesi.
'    - Moderna legacy: `VECTOR ADD Dest, Src` - Comando senza parentesi.
'    Entrambe mappano alle stesse routine GPM ottimizzate.
'
' 2. RENDERING SOFTWARE (IMAGEPUTS)
'    - Utilizza la CPU per calcolare la posizione e copiare i pixel tra buffer di memoria.
'    - Supporta `IMAGEGET` per catturare sprite e `IMAGEPUTS` per il disegno in batch.
'
' 3. RENDERING HARDWARE (DRAW TEXTURES)
'    - Carica i dati nella VRAM tramite `LOAD TEXTURE`.
'    - Esegue il disegno tramite la GPU inviando array di trasformazione (X, Y, Rot, Scale).
'    - Utilizza `VECTOR DRAW TEXTURES` o il comando abbreviato `DRAW TEXTURES`.
'
' 4. OTTIMIZZAZIONE VETTORIALE (BATCH OPERATIONS)
'    Invece di loop FOR...NEXT lenti, si usano comandi che operano su interi array:
'    - `VECTOR FILL RANDOM`: Popolamento istantaneo di dati casuali.
'    - `VECTOR ADD`: Integrazione fisica della velocità sulla posizione.
'    - `VECTOR BOUNCE2D`: Gestione collisioni bordi per l'intero set di dati.
'    - `VECTOR ADD WRAP`: Aggiornamento ciclico (es. rotazioni 0-360).
'
' =================================================================================================

' Importazione dei nuovi namespace specializzati (Ex LINALG)
IMPORT GRAPHICS
IMPORT CONSOLE
IMPORT STRINGS
IMPORT VECTOR
IMPORT MATRIX

SUB Main()
  ' Risorse: Sprite (CPU) e Texture (GPU Handle)
  DIM ShipSprite(150) AS INTEGER
  DIM ShipTextureID AS LONG
  
  SCREEN 12
  FONT "font.ttf", 12
  
  CCLS
  CPRINT "FASE 1: Setup delle risorse..."
  
  ' --- SETUP SOFTWARE (CPU) ---
  ' Disegno dello sprite su buffer nascosto
  SCREENSET , 1 
  CLS 0
  LINE (5, 0)-(5, 9), 15
  LINE (4, 2)-(6, 2), 15
  LINE (3, 3)-(7, 3), 15
  LINE (2, 8)-(8, 8), 15
  LINE (0, 9)-(10, 9), 15
  PAINT (5, 5), 14, 15
  
  ' Cattura sprite (Sintassi standard Graphics)
  IMAGEGET (0, 0)-(10, 9), ShipSprite
  SCREENSET 0, 0

  ' --- SETUP HARDWARE (GPU) ---
  ' Caricamento texture in VRAM (Handle LONG)
  LOAD TEXTURE "sprite.png" INTO ShipTextureID
  
  IF ShipTextureID = 0 THEN
    CCLS : CPRINT "ERRORE: Impossibile caricare 'sprite.png'."
    ' WAITKEY : END  (commentato: niente attesa tasto in headless)
  END IF
  
  ' Parametri dei test
  DIM SwCounts() AS LONG = {10, 100, 500}
  DIM HwCounts() AS LONG = {1000, 5000, 10000}
  
  ' Ciclo Test Software (CPU Bound)
  FOR I = 0 TO UBOUND(SwCounts)
    ShowIntroScreen(I + 1, 6, SwCounts(I), "CPU / SOFTWARE MODE", 0)
    TestSoftware(ShipSprite, SwCounts(I))
  NEXT I
  
  ' Ciclo Test Hardware (GPU Accelerated)
  FOR I = 0 TO UBOUND(HwCounts)
    DIM spriteScale AS DOUBLE = 500 / HwCounts(I)
    ShowIntroScreen(I + 4, 6, HwCounts(I), "GPU / HARDWARE ACCELERATED", spriteScale)
    TestHardware(ShipTextureID, HwCounts(I), spriteScale)
  NEXT I
  
  ShowIntroScreen(0, 0, 0, "TEST COMPLETATI", 0)
  ' WAITKEY  (commentato: niente attesa tasto in headless)
  SCREEN 0
END SUB

' Benchmark del Rendering Software con gestione fisica ottimizzata
SUB TestSoftware(BYREF Sprite() AS INTEGER, MAX AS LONG)
  CPRINT "--> TEST SOFTWARE (" + STR(MAX) + " sprites) AVVIATO."
  
  DIM Positions(MAX * 2 - 1) AS INTEGER
  DIM X#(MAX), Y#(MAX), DX#(MAX), DY#(MAX)
  
  ' --- DIMOSTRAZIONE SINTASSI LEGACY (SETUP) ---
  VECTOR FILL RANDOM X, 0, 630        ' Legacy Prefissata
  VECTOR FILL RANDOM Y, 0, 470          ' legacy
  VECTOR FILL RANDOM DX, -2, 2       ' Legacy Prefissata
  VECTOR FILL RANDOM DY, -2, 2          ' legacy
  
  DIM T_START = TI(), FRAMES = 0
  DIM Work = 1, View = 0
  
  DO
    SCREENSET , Work : CLS 1

    ' --- DIMOSTRAZIONE SINTASSI LEGACY (UPDATE) ---
    VECTOR ADD X, DX               ' Legacy: x = x + dx
    VECTOR ADD Y, DY                  ' Moderna: y = y + dy
    
    ' Rimbalzo bordi (Legacy)
    VECTOR BOUNCE2D X, Y, DX, DY, 0, 0, 630, 470

    ' Trasferimento coordinate per blitting software (CPU)
    FOR I = 0 TO MAX - 1
      Positions(I * 2) = CINT(X[I])
      Positions(I * 2 + 1) = CINT(Y[I])
    NEXT I
    
    ' Disegno in batch CPU
    IMAGEPUTS Sprite, Positions
    
    SCREENSET View : SWAP Work, View
    FRAMES = FRAMES + 1
    IF TI() - T_START >= 1000 THEN
      FPS = FRAMES / ((TI() - T_START) / 1000)
      CLOCATE 1, 1 : CPRINT "CPU MODE | SPRITES: " + STR(MAX) + " | FPS: " + FORMAT(FPS, "#0.0")
      T_START = TI() : FRAMES = 0
    END IF
    ' INKEY K$ : IF K$ <> "" THEN EXIT DO : SLEEP 1  (commentato: niente attesa tasto)
    IF FRAMES >= 2 THEN EXIT DO
  LOOP
END SUB

' Benchmark del Rendering Hardware (GPU) con uso intensivo di VECTOR
SUB TestHardware(TexID AS LONG, MAX AS LONG, sc AS DOUBLE)
    CPRINT "--> TEST HARDWARE (" + STR(MAX) + " sprites) AVVIATO."

    DIM X#(MAX), Y#(MAX), DX#(MAX), DY#(MAX)
    DIM Rot#(MAX), DRot#(MAX), Scales#(MAX)
    
    ' --- DIMOSTRAZIONE SINTASSI LEGACY (INITIALIZATION) ---
    VECTOR FILL RANDOM X, 0, 640      ' Legacy
    VECTOR FILL RANDOM Y, 0, 480        ' Moderna
    VECTOR FILL RANDOM DX, -2, 2        ' Moderna
    VECTOR FILL RANDOM DY, -2, 2     ' Legacy
    VECTOR FILL RANDOM Rot, 0, 360      ' Moderna
    VECTOR FILL RANDOM DRot, -3, 3   ' Legacy
    
    ' Impostazione valore costante batch
    VECTOR SET Scales, sc             ' Legacy
    ' VECTOR SET Scales, sc               ' (Equivalente moderna)
  
    DIM T_START = TI(), FRAMES = 0
    DIM Work = 1, View = 0
    
    DO
      SCREENSET , Work : CLS 1
  
      ' --- DIMOSTRAZIONE SINTASSI LEGACY (PER-FRAME PHYSICS) ---
      VECTOR ADD X, DX                ' Moderna
      VECTOR ADD Y, DY             ' Legacy
      
      ' Rimbalzo vettoriale batch (Legacy)
      VECTOR BOUNCE2D X, Y, DX, DY, 0, 0, 640, 480
      
      ' Incremento rotazione con wrap 0-360 (Moderna)
      VECTOR ADD WRAP Rot, DRot, 0, 360
  
      ' --- RENDERING BATCH GPU ---
      ' Il comando DRAW TEXTURES appartiene al namespace GRAPHICS.
      DRAW TEXTURES TexID, X, Y, Rot, Scales
  
      SCREENSET View : SWAP Work, View
      FRAMES = FRAMES + 1
      IF TI() - T_START > 1000 THEN
        FPS = FRAMES / ((TI() - T_START) / 1000)
        CLOCATE 1, 1 : CPRINT "GPU MODE | SPRITES: " + STR(MAX) + " | FPS: " + FORMAT(FPS, "#0.0")
        T_START = TI() : FRAMES = 0
      END IF
      ' INKEY K$ : IF K$ <> "" THEN EXIT DO : SLEEP 1  (commentato: niente attesa tasto)
      IF FRAMES >= 2 THEN EXIT DO
    LOOP
END SUB

' Helper per visualizzazione interfaccia
SUB CenterPrint(Y AS INTEGER, Text$ AS STRING)
    LOCATE Y, (640 - TEXTWIDTH(Text$)) / 2
    PRINT Text$
END SUB

' Helper per visualizzazione interfaccia
SUB ShowIntroScreen(TestNum AS LONG, Total AS LONG, Num AS LONG, Mode$ AS STRING, sc AS DOUBLE)
    SCREENSET , 0 : CLS
    FONT "font.ttf", 12 : COLOR 14, 0
    DIM L1$ = FORMAT(Num, "#,##0") + " SPRITES"
    LOCATE 160, (640 - TEXTWIDTH(L1$)) / 2 : PRINT L1$
    FONT "font.ttf", 12 : COLOR 15, 0
    LOCATE 230, (640 - TEXTWIDTH(Mode$)) / 2 : PRINT Mode$
    IF sc > 0 THEN 
        DIM LS$ = "SCALE: " + FORMAT(sc, "0.0#")
        LOCATE 270, (640 - TEXTWIDTH(LS$)) / 2 : PRINT LS$
    END IF
    LOCATE 340, (640 - TEXTWIDTH("PREMI UN TASTO")) / 2 : PRINT "PREMI UN TASTO PER INIZIARE"
    IF TestNum > 0 THEN
        DIM L4$ = "TEST " + STR(TestNum) + " DI " + STR(Total)
        LOCATE 420, (640 - TEXTWIDTH(L4$)) / 2 : PRINT L4$
    END IF
    SCREENSET 0 : SLEEP 500 : CLS : FONT "font.ttf", 12
END SUB