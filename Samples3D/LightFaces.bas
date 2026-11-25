' LightFaces.bas — `SCENE LIGHT dx, dy, dz, livello`: luce direzionale per faccia.
'
' Il vettore (dx, dy, dz) punta VERSO la luce (come il vettore L di Lambert) e viene
' normalizzato dal runtime; la normale di faccia si calcola a ogni RENDER sulle posizioni
' MONDO, quindi un cubo ruotato si illumina come è messo davvero. Il coseno diventa un
' intero 0..255 e la miscela ambiente(40)→livello è intera: stessi byte su tutte le gambe.
'
' Qui il "sole" gira attorno alla scena: il pavimento resta chiaro nella parte rivolta
' alla luce e le facce in ombra scendono all'ambiente, mantenendo leggibile la forma.
' Con LIGHT attiva la luce per faccia SOSTITUISCE la tabella fissa delle 6 facce del cubo.
'
' Scene: pavimento 40x40 con texture a scacchi (UV 0..8), tre torri di blocchi, otto
' pilastri; 24 frame in /tmp/light_frames/.
IMPORT SCENE
IMPORT CONVERSIONS

SUB Main()
  NF = 24
  SCENE VIEW 640, 360, 55
  SCENE TEXTURE 4

  FOR fr = 0 TO NF - 1
    sun = fr * 360.0 / NF                         ' giro del sole in un ciclo
    sunr = sun * 3.14159265358979 / 180.0
    ang = sun * 0.5

    SCENE RESET
    SCENE TEXTURE 4

    ' il sole: sopra l'orizzonte e in giro; livello 235 = facce esposte quasi piene
    SCENE LIGHT COS(sunr), 0.9, SIN(sunr), 235

    ' ── pavimento ────────────────────────────────────────────────────────────
    SCENE OBJECT 220, 220, 220
    SCENE VERTEX -20, 0, -20, 0, 8
    SCENE VERTEX -20, 0,  20, 0, 0
    SCENE VERTEX  20, 0,  20, 8, 0
    SCENE VERTEX  20, 0, -20, 8, 8
    SCENE QUAD 0, 1, 2, 3

    ' ── tre torri di blocchi ─────────────────────────────────────────────────
    FOR t = 0 TO 2
      ta = t * 120.0
      tar = ta * 3.14159265358979 / 180.0
      cx = COS(tar) * 7
      cz = SIN(tar) * 7
      FOR s = 0 TO 3
        dw = 2.2 - s * 0.3
        SCENE OBJECT 255 - t * 40, 220 - s * 20, 180 + t * 25
        SCENE BOX -dw, 0, -dw, dw, 1.7, dw
        SCENE OBJPOS cx, s * 1.8, cz, ang + s * 18
      NEXT s
    NEXT t

    ' ── pilastri in cerchio ──────────────────────────────────────────────────
    FOR i = 0 TO 7
      a = i * 45.0
      r = a * 3.14159265358979 / 180.0
      SCENE OBJECT 200, 200, 210
      SCENE BOX -0.45, 0, -0.45, 0.45, 3.4, 0.45
      SCENE OBJPOS COS(r) * 12, 0, SIN(r) * 12, 0
    NEXT i

    SCENE CAMERA COS(sunr) * 18, 9, SIN(sunr) * 18, 270 - sun, -12
    SCENE RENDER
    SCENE SAVE "/tmp/light_frames/f" + CSTR(fr) + ".png"

    IF fr = 0 THEN
      PRINT "vertici="; SCENE.VERTS(); "  triangoli="; SCENE.TRIS(); "  disegnati="; SCENE.DRAWN()
    END IF
  NEXT fr

  PRINT "frame="; NF; "  ultimo hash="; SCENE.HASH(); "  pixel="; SCENE.PIXELS()
END SUB
