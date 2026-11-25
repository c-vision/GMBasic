' TextureFile.bas — `SCENE TEXTURE "file.png"`: la texture è un'immagine, non una scacchiera.
'
' Un solo verbo, due forme: `SCENE TEXTURE celle` costruisce la scacchiera procedurale,
' `SCENE TEXTURE "file.png"` carica un PNG (RGBA8, riga 0 in alto) e lo usa come materiale di
' TUTTI i draw successivi. Per una foto basta la stessa riga:
'
'     SCENE TEXTURE "mia/foto.png"          ' percorso relativo alla cwd del processo
'
' Il sample non dipende da un asset esterno: la prima parte costruisce il mosaico 4x4
' blu/ambra con RENDER+SAVE (64x64, ~400 byte), la seconda lo ricarica come texture e
' disegna la scena. Gira da qualunque cwd e su tutte e quattro le gambe.
'
' Cosa mostra:
'   * pavimento 40x40 con UV 0..10: la texture si ripete 10 volte per lato;
'   * torre di 4 blocchi: ogni faccia ha UV 0..1, quindi un "piastrellone" per lato;
'   * pilastri tinti: la tint MOLTIPLICA la texture (blu e ambra), non la sostituisce;
'   * camera in orbita di 24 frame, salvati in /tmp/tex_frames/.
IMPORT SCENE
IMPORT CONVERSIONS

SUB Main()
  NF = 24
  TEXPATH$ = "scene_mosaic.png"

  ' ── 1) il mosaico: 16 quad colorati, un frame 64x64 salvato su disco ───────
  SCENE VIEW 64, 64, 60
  k = 0
  FOR gy = 0 TO 3
    FOR gx = 0 TO 3
      x0 = -1.0 + gx * 0.5
      y0 = -1.0 + gy * 0.5
      IF ((gx + gy) MOD 2) = 0 THEN
        SCENE OBJECT 34 + k, 78 + k, 132 + k
      ELSE
        SCENE OBJECT 236 - k, 176 - k, 74 - k
      END IF
      SCENE VERTEX x0,       y0,       0, 0, 1
      SCENE VERTEX x0 + 0.5, y0,       0, 1, 1
      SCENE VERTEX x0 + 0.5, y0 + 0.5, 0, 1, 0
      SCENE VERTEX x0,       y0 + 0.5, 0, 0, 0
      SCENE QUAD 0, 3, 2, 1
      k = k + 4
    NEXT gx
  NEXT gy
  SCENE CAMERA 0, 0, -1.732, 0, 0
  SCENE RENDER
  SCENE SAVE TEXPATH$
  PRINT "texture "; SCENE.TRIS(); " triangoli -> "; TEXPATH$

  ' ── 2) la scena, con il PNG come materiale ─────────────────────────────────
  SCENE RESET
  SCENE VIEW 640, 360, 55
  SCENE TEXTURE TEXPATH$

  FOR fr = 0 TO NF - 1
    ang = fr * 360.0 / NF
    arc = ang * 3.14159265358979 / 180.0

    SCENE RESET
    SCENE TEXTURE TEXPATH$

    ' pavimento: la texture si ripete 10x10
    SCENE OBJECT 235, 235, 235
    SCENE VERTEX -20, 0, -20, 0, 10
    SCENE VERTEX -20, 0,  20, 0, 0
    SCENE VERTEX  20, 0,  20, 10, 0
    SCENE VERTEX  20, 0, -20, 10, 10
    SCENE QUAD 0, 1, 2, 3

    ' torre: facce con un piastrellone intero
    FOR s = 0 TO 3
      dw = 2.6 - s * 0.35
      SCENE OBJECT 255, 255, 255
      SCENE BOX -dw, 0, -dw, dw, 1.8, dw
      SCENE OBJPOS 0, s * 1.9, 0, ang * 1.5 + s * 15
    NEXT s

    ' pilastri: la tint colora la texture
    FOR i = 0 TO 7
      a = i * 45.0 + ang * 0.5
      r = a * 3.14159265358979 / 180.0
      IF (i MOD 2) = 0 THEN
        SCENE OBJECT 120, 190, 255
      ELSE
        SCENE OBJECT 255, 200, 120
      END IF
      SCENE BOX -0.5, 0, -0.5, 0.5, 3.2, 0.5
      SCENE OBJPOS COS(r) * 9, 0, SIN(r) * 9, (0 - a)
    NEXT i

    SCENE CAMERA COS(arc) * 16, 7.5 + SIN(arc * 2.0) * 1.5, SIN(arc) * 16, 270 - ang, -11
    SCENE RENDER
    SCENE SAVE "/tmp/tex_frames/f" + CSTR(fr) + ".png"

    IF fr = 0 THEN
      PRINT "vertici="; SCENE.VERTS(); "  triangoli="; SCENE.TRIS(); "  disegnati="; SCENE.DRAWN()
    END IF
  NEXT fr

  PRINT "frame="; NF; "  ultimo hash="; SCENE.HASH(); "  pixel="; SCENE.PIXELS()
END SUB
