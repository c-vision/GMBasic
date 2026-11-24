' SceneShowcase.bas — vetrina del dominio `SCENE` (renderer mesh, P3).
'
' Cosa mostra, in un solo programma:
'   * piano texturizzato 60x60 (UV 0..8: la texture a scacchi si ripete 8 volte);
'   * torre di 6 blocchi, ognuno ruotato e colorato (OBJPOS con yaw animato);
'   * anello di 24 pilastri, orientati verso il centro;
'   * 12 blocchi bassi su un anello esterno;
'   * 8 monoliti alti e un ponte sospeso (BOX sottili);
'   * camera in orbita completa con quota che oscilla e fov che "respira".
'
' La geometria si ricostruisce a ogni frame (RESET + comandi) perché OBJPOS
' agisce sull'oggetto corrente: è il modo in cui il dominio anima le trasformazioni
' senza che il BASIC maneggi matrici.
'
' Uscita: un PNG per frame in <dir>f<indice>.png, più un riepilogo a video.
' Cambia OUTDIR$ per scrivere altrove; l'indice non è paddato (l'encoder ordina
' numericamente il nome).
'
' Tutto su quattro gambe: `tools/sh/gmb_c99native_run.sh Samples3D/SceneShowcase.bas`
' (nativa) e `test_ilrun --engine=il|pcode|vmc` per IL/PCode/VM-C.

IMPORT SCENE
IMPORT CONVERSIONS

SUB Main()
  NF = 120                     ' frame: 5 secondi a 24 fps
  OUTDIR$ = "/tmp/scene_frames/f"

  SCENE VIEW 640, 360, 55      ' 16:9; il fov viene poi animato con CAMERA? no: fov fisso
  SCENE TEXTURE 4              ' texture a scacchi 32x32 (4 celle per lato: leggibile sui box)

  FOR fr = 0 TO NF - 1
    t = fr / NF
    ang = fr * 360.0 / NF                       ' giro completo in un ciclo
    arc = ang * 3.14159265358979 / 180.0

    SCENE RESET

    ' ── piano ─────────────────────────────────────────────────────────────────
    SCENE OBJECT 70, 92, 124
    SCENE VERTEX -30, 0, -30, 0, 0
    SCENE VERTEX -30, 0,  30, 0, 8
    SCENE VERTEX  30, 0,  30, 8, 8
    SCENE VERTEX  30, 0, -30, 8, 0
    SCENE QUAD 0, 1, 2, 3

    ' ── torre: 5 blocchi che si stringono, rotazione a spirale ────────────────
    FOR s = 0 TO 4
      dw = 3.2 - s * 0.45
      SCENE OBJECT 210 - s * 18, 130 + s * 8, 60 + s * 22
      SCENE BOX -dw, 0, -dw, dw, 2.0, dw
      SCENE OBJPOS 0, s * 1.85, 0, ang * 1.2 + s * 11
    NEXT s

    ' ── anello di 24 pilastri, ognuno rivolto al centro ───────────────────────
    FOR i = 0 TO 23
      a = i * 15.0 + ang
      r = a * 3.14159265358979 / 180.0
      SCENE OBJECT 120 + (i MOD 3) * 40, 170, 90 + (i MOD 2) * 40
      SCENE BOX -0.45, 0, -0.45, 0.45, 4.2, 0.45
      SCENE OBJPOS COS(r) * 16, 0, SIN(r) * 16, -a
    NEXT i

    ' ── 12 blocchi bassi sull'anello esterno ──────────────────────────────────
    FOR i = 0 TO 11
      a = i * 30.0 - ang * 0.5
      r = a * 3.14159265358979 / 180.0
      SCENE OBJECT 200, 150, 110
      SCENE BOX -1.1, 0, -1.1, 1.1, 1.1, 1.1
      SCENE OBJPOS COS(r) * 23, 0, SIN(r) * 23, a * 2
    NEXT i

    ' ── 8 monoliti alti ───────────────────────────────────────────────────────
    FOR i = 0 TO 7
      a = i * 45.0 + ang * 0.25
      r = a * 3.14159265358979 / 180.0
      SCENE OBJECT 90, 120, 190
      SCENE BOX -0.6, 0, -0.6, 0.6, 6.5, 0.6
      SCENE OBJPOS COS(r) * 11, 0, SIN(r) * 11, a
    NEXT i

    ' ── ponte sospeso fra due pilastri ────────────────────────────────────────
    SCENE OBJECT 230, 200, 120
    SCENE BOX -5, 7, -0.7, 5, 8, 0.7
    SCENE OBJPOS 0, 0, -12, 0

    ' ── camera: orbita, quota che oscilla, sguardo al centro ──────────────────
    SCENE CAMERA COS(arc) * 29, 9.5 + SIN(arc * 2.0) * 2.5, SIN(arc) * 29, 270 - ang, -12 + SIN(arc * 3.0) * 3

    SCENE RENDER
    SCENE SAVE OUTDIR$ + CSTR(fr) + ".png"

    IF (fr MOD 24) = 0 THEN
      PRINT "frame "; fr; "  vertici="; SCENE.VERTS(); "  triangoli="; SCENE.TRIS(); "  disegnati="; SCENE.DRAWN(); "  scartati="; SCENE.CULLED()
    END IF
  NEXT fr

  SCENE CAMERA 26, 9, 0, 270, -11
  SCENE RENDER
  SCENE SAVE "/tmp/scene_frames/hero.png"

  PRINT ""
  PRINT "frame totali: "; NF; "  ultimo hash="; SCENE.HASH(); "  pixel="; SCENE.PIXELS()
  PRINT "frame scritti in /tmp/scene_frames/"
END SUB
