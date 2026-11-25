' PresentLoop.bas — `SCENE PRESENT` (idea #2): il target 3D dentro la pagina 2D.
'
' Il ciclo di un'applicazione 3D interattiva, senza finestra: la pagina 2D è un canvas, lo
' stesso `active_page()` che il pump SDL mostra a schermo quando si usa `SCREEN`. Ogni
' fotogramma fa, in quest'ordine:
'
'   1. il 3D renderizza la scena nel proprio target (VIEW/RENDER);
'   2. `SCENE PRESENT` copia il target nella pagina 2D corrente — 1:1 dall'angolo in alto
'      a sinistra, nessuna scalatura (qui target e pagina sono entrambi 320x180);
'   3. la 2D disegna SOPRA: il 3D fa da sfondo, l'HUD resta leggibile;
'   4. `CANVAS SAVE` archivia il fotogramma.
'
' Con `SCREEN 12` al posto del canvas cambia solo la destinazione: la pagina è quella
' visibile e il pump SDL la mostra al tick successivo. PRESENT non tocca mai SDL (che vive
' solo sul main thread): scrive memoria dentro il task GPM che esegue il ciclo.
'
' 24 frame in /tmp/present_frames/.
IMPORT SCENE
IMPORT GRAPHICS
IMPORT CONVERSIONS

SUB Main()
  NF = 24
  PW = 320
  PH = 180
  PIV = 3.14159265358979

  ' Pagina 2D = canvas: stesso `active_page()` della finestra, ma headless e salvabile.
  ' In un'applicazione vera: `SCREEN 12` (o `SCREEN 12, PW, PH, 32`) e si toglie il canvas.
  CANVAS NEW PW, PH INTO page
  CANVAS USE page
  SCENE VIEW PW, PH, 55
  SCENE SMOOTH ON
  SCENE LIGHT 0.4, 1, 0.3, 235

  FOR fr = 0 TO NF - 1
    ang = fr * 360.0 / NF
    angr = ang * PIV / 180.0

    ' ── 1. scena 3D del fotogramma ────────────────────────────────────────
    SCENE RESET
    SCENE SMOOTH ON
    SCENE LIGHT 0.4, 1, 0.3, 235
    FOR i = 0 TO 5
      a = i * 60.0
      ar = a * PIV / 180.0
      SCENE OBJECT 210, 220, 255 - i * 20
      SCENE BOX -0.7, 0, -0.7, 0.7, (1 + i * 0.35), 0.7
      SCENE OBJPOS COS(ar) * 5, 0, SIN(ar) * 5, a + fr * 4
    NEXT i
    SCENE OBJECT 240, 200, 160
    SCENE VERTEX -12, 0, -12, 0, 12
    SCENE VERTEX -12, 0,  12, 0, 0
    SCENE VERTEX  12, 0,  12, 12, 0
    SCENE VERTEX  12, 0, -12, 12, 12
    SCENE QUAD 0, 1, 2, 3
    SCENE CAMERA COS(angr) * 11, 5, SIN(angr) * 11, 270 - ang, -14
    SCENE RENDER

    ' ── 2. il ponte: target 3D → pagina 2D corrente ────────────────────────
    SCENE PRESENT

    ' ── 3. HUD 2D SOPRA il 3D, sulla stessa pagina ────────────────────────
    LINE (1, 1)-(PW - 2, 1), 3
    LINE (PW - 2, 1)-(PW - 2, PH - 2), 3
    LINE (PW - 2, PH - 2)-(1, PH - 2), 3
    LINE (1, PH - 2)-(1, 1), 3
    FOR b = 0 TO 9
      LINE (8 + b * 14, PH - 12)-(18 + b * 14, PH - 12), 14
    NEXT b
    PSET (8 + (fr MOD 10) * 14, PH - 12), 15

    CANVAS SAVE page, "/tmp/present_frames/f" + CSTR(fr) + ".png"

    IF fr = 0 THEN PRINT "drawn="; SCENE.DRAWN(); "  pixel3d="; SCENE.PIXELS()
  NEXT fr

  CANVAS GET page, 4, 4 INTO corner
  CANVAS GET page, PW \ 2, PH \ 2 INTO centro
  CANVAS GET page, 0, 0 INTO bordo
  PRINT "frame="; NF; "  hash="; SCENE.HASH(); "  corner="; corner; "  centro="; centro; "  bordo="; bordo
END SUB
