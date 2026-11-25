' MaterialFlags.bas — `SCENE SMOOTH|ALPHA|TWOSIDED on|off` (idea #5).
'
' I tre flag erano già nel kernel (`gmb_3d_material_t`) ma il BASIC non li raggiungeva: questi
' verbi li espongono senza toccare il rasterizzatore. Default = comportamento storico
' (SMOOTH ON, ALPHA ON, TWOSIDED OFF).
'
' La scena: otto pannelli di fogliame attorno a un anello centrale di lastre sottili.
'   * ALPHA ON  → i buchi del fogliame sono scartati: si vede cosa c'è dietro;
'     ALPHA OFF → i buchi vengono disegnati col loro rgb: i pannelli diventano rettangoli pieni.
'   * TWOSIDED ON → le lastre girate di schiena si vedono lo stesso;
'     TWOSIDED OFF → metà dell'anello sparisce (finisce in SCENE.CULLED()).
'   * SMOOTH: bilineare sui pannelli, senza toccare la geometria.
'
' Le tre fasi (8 frame ciascuna) mostrano i tre stati; 24 frame in /tmp/material_frames/.
IMPORT SCENE
IMPORT CONVERSIONS

SUB Main()
  NF = 24
  PIV = 3.14159265358979                          ' non `PI`: sulle gambe gestite e' una funzione
  SCENE VIEW 640, 360, 55
  SCENE TEXTURE "FoliageAlpha.png"

  FOR fr = 0 TO NF - 1
    phase = fr \ 8
    ang = fr * 360.0 / NF
    angr = ang * PIV / 180.0

    SCENE RESET
    SCENE TEXTURE "FoliageAlpha.png"
    SCENE SMOOTH ON

    ' fogliame: alpha-test acceso, tranne nella fase 1 dove lo si spegne
    IF phase = 1 THEN
      SCENE ALPHA OFF
    ELSE
      SCENE ALPHA ON
    END IF
    ' lastre: due lati accesi, tranne nella fase 2 (cull attivo)
    IF phase = 2 THEN
      SCENE TWOSIDED OFF
      SCENE ALPHA ON
    ELSE
      SCENE TWOSIDED ON
    END IF

    SCENE LIGHT 0.4, 1, 0.3, 230

    ' ── otto pannelli di fogliame, rivolti verso il centro ────────────────
    FOR i = 0 TO 7
      a = i * 45.0
      ar = a * PIV / 180.0
      SCENE OBJECT 240, 255, 240
      SCENE VERTEX -1.6, 0, 0, 0, 1
      SCENE VERTEX  1.6, 0, 0, 1, 1
      SCENE VERTEX  1.6, 4, 0, 1, 0
      SCENE VERTEX -1.6, 4, 0, 0, 0
      SCENE QUAD 0, 3, 2, 1
      SCENE OBJPOS COS(ar) * 8, 0, SIN(ar) * 8, a + 90
    NEXT i

    ' ── anello di lastre sottili: metà girate di schiena ──────────────────
    FOR j = 0 TO 5
      b = j * 60.0 + 15
      br = b * PIV / 180.0
      SCENE OBJECT 210, 225, 255
      SCENE VERTEX -0.9, 0, 0, 0, 1
      SCENE VERTEX  0.9, 0, 0, 1, 1
      SCENE VERTEX  0.9, 3, 0, 1, 0
      SCENE VERTEX -0.9, 3, 0, 0, 0
      SCENE QUAD 0, 1, 2, 3                          ' winding opposto ai pannelli
      SCENE OBJPOS COS(br) * 4, 0, SIN(br) * 4, b + 90
    NEXT j

    SCENE CAMERA COS(angr) * 14, 6, SIN(angr) * 14, 270 - ang, -10
    SCENE RENDER
    SCENE SAVE "/tmp/material_frames/f" + CSTR(fr) + ".png"

    IF fr = 0 OR fr = 8 OR fr = 16 THEN
      PRINT "frame "; fr; "  alphatest="; (1 - phase); "  drawn="; SCENE.DRAWN(); "  culled="; SCENE.CULLED()
    END IF
  NEXT fr

  PRINT "frame="; NF; "  hash="; SCENE.HASH(); "  pixel="; SCENE.PIXELS()
END SUB
