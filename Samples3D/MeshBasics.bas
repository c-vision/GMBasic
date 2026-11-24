' MeshBasics.bas — accettazione di P3: cubo texturizzato, piano e 100 oggetti.
'
' Come gira: usa il dominio BASIC `SCENE` (piano §4.2) su tutte e quattro le
' gambe — C99Native, IL, PCode e VM/C — con la stessa sorgente. I comandi e le
' letture sono quelli del namespace `SCENE` (StdLib.Scene.cs); ogni verbo è UNA
' chiamata alla facciata `gmb_scene_*` di native/gmb_runtime_c/src/gmb_3d_cmd.c,
' che accumula la geometria e chiama `gmb_3d_mesh_draw` (src/gmb_3d_frame.c):
' target ARGB+depth, clip dei 6 piani del frustum, z-buffer, UV prospettici via
' gm_tex_sample. Nessuna `DECLARE`: lo stato della scena vive solo nel runtime C.
'
' `IMPORT SCENE` è obbligatoria: `SCENE` non è fra i namespace sempre disponibili
' (come WORLD e CLOCK). Senza, sia il comando sia le letture sono un errore
' dichiarato su ogni gamba.
'
' Convenzione: y è l'altezza (tradizione delle mesh) e i vertici si dichiarano
' in senso antiorario VISTO DA FUORI, il winding con cui il kernel distingue una
' faccia frontale da una back-face.
'
' I comandi non hanno valore di ritorno leggibile (§1): gli errori si vedono
' dallo stato — un VIEW fallito non alloca il target, quindi RENDER non disegna e
' DRAWN resta 0, che è esattamente ciò che verificano le asserzioni qui sotto.
'
' Il frame viene scritto in MeshBasics.png (accanto al sample: il runner esegue
' con la cartella del sorgente come CWD) e stampato anche in ASCII, così l'esito
' si legge senza aprire un file.

IMPORT SCENE

SUB Main()
  ' ── 1. Bersaglio e inquadratura ─────────────────────────────────────────────
  ' 320x150 è la risoluzione dell'accettazione di P3 (§6); fov verticale 60°.
  SCENE RESET
  SCENE VIEW 320, 150, 60
  SCENE TEXTURE 8

  ' ── 2. Il piano: un quad texturizzato di 40x40 unità ─────────────────────────
  ' Antiorario visto da sopra (+y): il quad è frontale, se fosse invertito il
  ' cull lo scarterebbe e DRAWN non conterebbe i suoi 2 triangoli.
  SCENE OBJECT 90, 150, 220
  SCENE VERTEX -20, 0, -20, 0, 0
  SCENE VERTEX -20, 0,  20, 0, 8
  SCENE VERTEX  20, 0,  20, 8, 8
  SCENE VERTEX  20, 0, -20, 8, 0
  SCENE QUAD 0, 1, 2, 3

  ' ── 3. Il cubo texturizzato ─────────────────────────────────────────────────
  ' BOX genera le 6 facce con UV 0..1 per faccia: 24 vertici, 12 triangoli.
  ' Da una camera in alto restano visibili 3 facce, le altre 3 sono back-face.
  ' L'oggetto nasce in origine, quindi qui OBJPOS non serve: lo usano le colonnine.
  SCENE OBJECT 220, 90, 60
  SCENE BOX -1, 0, -1, 1, 2, 1

  ' ── 4. Cento oggetti in griglia ─────────────────────────────────────────────
  ' 10x10 colonnine da 0,7 unità, passo 2,4: 100 oggetti, 12 triangoli ciascuno.
  ' Ogni oggetto ha la sua posizione (OBJPOS): la matrice la costruisce il
  ' motore, il BASIC non maneggia matrici.
  FOR gz = 0 TO 9
    FOR gx = 0 TO 9
      SCENE OBJECT 120 + gx * 12, 160, 90 + gz * 12
      SCENE BOX -0.35, 0, -0.35, 0.35, 0.7, 0.35
      SCENE OBJPOS gx * 2.4 - 10.8, 0, gz * 2.4 - 10.8, 0
    NEXT gx
  NEXT gz

  ' ── 5. Camera e disegno ─────────────────────────────────────────────────────
  ' x, y, z, yaw (gradi: 0 = sguardo verso +z), pitch (negativo = in basso).
  SCENE CAMERA 14, 9, 18, 220, -12
  SCENE RENDER

  ' ── 6. Che cosa è successo davvero ──────────────────────────────────────────
  ' VERTS/TRIS sono la geometria dichiarata, DRAWN i triangoli che il
  ' rasterizzatore ha effettivamente disegnato dopo cull e clip: se DRAWN è
  ' uguale a TRIS il culling non sta lavorando; se è 0, il frustum è sbagliato.
  PRINT "vertici dichiarati:  "; SCENE.VERTS()
  PRINT "triangoli dichiarati:"; SCENE.TRIS()
  PRINT "triangoli disegnati: "; SCENE.DRAWN()
  PRINT "triangoli scartati:  "; SCENE.CULLED()
  PRINT "pixel passati dal depth test: "; SCENE.PIXELS()
  PRINT "hash del frame (FNV-1a): "; SCENE.HASH()

  IF SCENE.VERTS() <> 2428 THEN
    PRINT "ERRORE: attesi 2428 vertici (4 piano + 24 cubo + 2400 oggetti)"
    END
  END IF
  IF SCENE.TRIS() <> 1214 THEN
    PRINT "ERRORE: attesi 1214 triangoli (2 piano + 12 cubo + 1200 oggetti)"
    END
  END IF
  IF SCENE.DRAWN() <= 0 THEN
    PRINT "ERRORE: nessun triangolo rasterizzato, il frustum non vede la scena"
    END
  END IF
  IF SCENE.DRAWN() >= SCENE.TRIS() THEN
    PRINT "ERRORE: nessun triangolo scartato, il culling per back-face non lavora"
    END
  END IF

  ' ── 7. Un giro completo: 1 frame dichiarato, 1 disegnato ─────────────────────
  ' Il ciclo di gioco vero chiama RENDER una volta per frame; qui si verifica che
  ' DRAWN e il contenuto del frame siano stabili fra due render identici
  ' (nessuno stato che si accumula).
  primo = SCENE.DRAWN()
  primoHash = SCENE.HASH()
  SCENE RENDER
  IF SCENE.DRAWN() <> primo THEN
    PRINT "ERRORE: DRAWN cambia fra due frame identici ("; primo; " -> "; SCENE.DRAWN(); ")"
    END
  END IF
  IF SCENE.HASH() <> primoHash THEN
    PRINT "ERRORE: il frame cambia fra due render identici ("; primoHash; " -> "; SCENE.HASH(); ")"
    END
  END IF
  PRINT "secondo frame: "; SCENE.DRAWN(); " triangoli, hash identico"

  ' ── 8. Il frame, in chiaro ──────────────────────────────────────────────────
  SCENE SAVE "MeshBasics.png"
  PRINT "frame salvato in MeshBasics.png"

  PRINT ""
  PRINT "anteprima 40x18 (luminanza: ' ' scuro ... '#' chiaro)"
  DIM riga AS STRING
  FOR py = 0 TO 17
    riga = ""
    FOR px = 0 TO 39
      c = SCENE.PIXEL(px * 8 + 4, py * 8 + 3)
      r = INT(c / 65536) MOD 256
      g = INT(c / 256) MOD 256
      b = c MOD 256
      l = INT((r + g + b) / 3)
      IF l < 20 THEN
        ch$ = " "
      ELSEIF l < 50 THEN
        ch$ = "."
      ELSEIF l < 90 THEN
        ch$ = ":"
      ELSEIF l < 140 THEN
        ch$ = "="
      ELSEIF l < 200 THEN
        ch$ = "*"
      ELSE
        ch$ = "#"
      END IF
      riga = riga + ch$
    NEXT px
    PRINT riga
  NEXT py

  PRINT ""
  PRINT "P3 OK: geometria, cull, clip, depth e UV hanno prodotto un frame."
END SUB
