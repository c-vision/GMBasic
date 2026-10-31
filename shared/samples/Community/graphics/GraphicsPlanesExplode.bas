' FILE: Graphics/GraphicsPlanesExplode.bas — Aeroplani GPU che esplodono alle collisioni
' =================================================================================================
' Rendering: DRAW TEXTURES batch (GPU). Collisioni: COLLIDEBATCH(X(),Y(),w,h) — spatial grid
' su array (O(n), zero alloc). Due aeroplani che collidono esplodono (explosion.png).
' ESC per uscire; si ferma quando non restano aeroplani e le esplosioni sono finite.
' =================================================================================================

IMPORT GRAPHICS
IMPORT CONSOLE

SUB Main()
  CONST N = 324
  DIM PlaneTex AS LONG
  DIM BoomTex AS LONG
  DIM I AS INTEGER
  DIM X#(N), Y#(N), DX#(N), DY#(N)
  DIM Rot#(N), Scale#(N)
  DIM Alive(N) AS INTEGER
  
  SCREEN 12
  SET CURSOR OFF
  FONT "font.ttf", 12
  COLOR 15, 1
  
  LOAD TEXTURE "sprite.png" INTO PlaneTex
  LOAD TEXTURE "explosion.png" INTO BoomTex
  
  FOR I = 0 TO N - 1
    X#(I) = 16 + (I MOD 18) * 35
    Y#(I) = 16 + (I \ 18) * 26
    DX#(I) = (RND() * 2) - 1
    DY#(I) = (RND() * 2) - 1
    Rot#(I) = RND() * 360
    Scale#(I) = 0.7
    Alive(I) = 1
  NEXT I
  
  DIM ActiveCount AS LONG
  DIM CB() AS INTEGER
  DIM RX#(N), RY#(N), RRot#(N), RSca#(N)
  DIM ExX#(32), ExY#(32), ExRot#(32)
  DIM BoomX#(32), BoomY#(32), BoomRot#(32), BoomScale#(32)
  DIM ExTimer(64) AS INTEGER
  DIM ExAlive(64) AS INTEGER
  DIM ExCount AS LONG
  DIM Explosions AS LONG
  DIM VisEx AS LONG
  DIM P AS INTEGER, a AS INTEGER, b AS INTEGER, S AS LONG, slot AS LONG, nw AS LONG
  DIM WorkPage AS INTEGER
  DIM K$ AS STRING
  
  ActiveCount = 324
  ExCount = 0
  Explosions = 0
  WorkPage = 1
  
  DO
    SCREENSET , WorkPage : CLS
    
    FOR I = 0 TO N - 1
      IF Alive(I) = 1 THEN
        X#(I) = X#(I) + DX#(I)
        Y#(I) = Y#(I) + DY#(I)
        IF X#(I) < 0 THEN X#(I) = 640
        IF X#(I) > 640 THEN X#(I) = 0
        IF Y#(I) < 0 THEN Y#(I) = 480
        IF Y#(I) > 480 THEN Y#(I) = 0
        Rot#(I) = Rot#(I) + 1
        IF Rot#(I) > 360 THEN Rot#(I) = 0
      END IF
    NEXT I
    
    CB = COLLIDEBATCH(X, Y, 14, 14)
    IF UBOUND(CB) > 0 THEN
      FOR P = 0 TO UBOUND(CB) - 1 STEP 2
        a = CB(P) : b = CB(P + 1)
        IF Alive(a) = 1 AND Alive(b) = 1 THEN
          ' Riusa uno slot esplosione LIBERO (altrimenti gli aerei morirebbero senza esplosione)
          slot = -1
          FOR S = 0 TO 31
            IF ExAlive(S) = 0 THEN slot = S : S = 31
          NEXT S
          IF slot = -1 THEN slot = 0
          ExX#(slot) = (X#(a) + X#(b)) / 2
          ExY#(slot) = (Y#(a) + Y#(b)) / 2
          ExRot#(slot) = RND() * 360
          ExTimer(slot) = TI()
          ExAlive(slot) = 1
          Explosions = Explosions + 1
          Alive(a) = 0 : Alive(b) = 0
          X#(a) = 10000 : Y#(a) = 10000
          X#(b) = 10000 : Y#(b) = 10000
          ActiveCount = ActiveCount - 2
        END IF
      NEXT P
    END IF
    
    FOR I = 0 TO 31
      IF ExAlive(I) = 1 THEN
        IF TI() - ExTimer(I) > 0.6 THEN ExAlive(I) = 0
      END IF
    NEXT I
    
    nw = 0
    FOR I = 0 TO N - 1
      IF Alive(I) = 1 THEN
        RX#(nw) = X#(I) : RY#(nw) = Y#(I) : RRot#(nw) = Rot#(I) : RSca#(nw) = Scale#(I)
        nw = nw + 1
      END IF
    NEXT I
    ' Azzera gli elementi stale (oltre nw): DRAW TEXTURES disegna TUTTO l'array,
    ' i morti con valori vecchi rispunterebbero → posizione fuori schermo.
    FOR I = nw TO N - 1
      RX#(I) = 10000 : RY#(I) = 10000
    NEXT I
    IF nw > 0 THEN DRAW TEXTURES PlaneTex, RX, RY, RRot, RSca
    
    VisEx = 0
    FOR I = 0 TO 31
      IF ExAlive(I) = 1 THEN
        BoomX#(VisEx) = ExX#(I)
        BoomY#(VisEx) = ExY#(I)
        BoomRot#(VisEx) = ExRot#(I)
        BoomScale#(VisEx) = 0.8
        VisEx = VisEx + 1
      END IF
    NEXT I
    FOR I = VisEx TO 31
      BoomX#(I) = 10000 : BoomY#(I) = 10000
    NEXT I
    IF VisEx > 0 THEN DRAW TEXTURES BoomTex, BoomX, BoomY, BoomRot, BoomScale
    
    SCREENSET WorkPage
    WorkPage = 1 - WorkPage
    SYNC
    INKEY K$
    IF K$ = CHR(27) THEN EXIT DO
    IF ActiveCount <= 0 AND VisEx = 0 THEN EXIT DO
    SLEEP 16
  LOOP
  ' CLS finale: l'ultimo frame del loop mostrava ancora le esplosioni attive —
  ' il loop esce subito quando VisEx=0 ma l'ultima texture disegnata resta.
  SCREENSET , WorkPage : CLS
  SCREENSET WorkPage : SYNC
  
  SCREEN 0
END SUB
