' EXPECT: red= 16711680|loaded= 16711680|drawn= 2|atomic= 16711680
' scene_texture_file — SCENE TEXTURE "file.png" (stesso verbo di `SCENE TEXTURE celle`).
'
' Il PNG di prova se lo costruisce la scena stessa: il RENDER di un quad rosso a
' schermo pieno + SAVE scrive un'immagine 8x8 uniformemente rossa, che poi viene
' ricaricata come texture. Nessun asset esterno: il test gira identico sulle
' quattro gambe (IL, PCode, VM-C, C99Native) e non dipende dal cwd.
'
' Tre cose verificate:
'   1. il quad rosso si vede (red=): la geometria e il raster funzionano;
'   2. il PNG caricato viene CAMPIONATO (loaded=, quad bianco + texture rossa);
'   3. un file inesistente non azzera la texture: vale quella precedente (atomic=).
IMPORT SCENE

SUB Main()
  ' 1) frame rosso a schermo pieno, salvato come PNG: sarà la texture
  SCENE VIEW 8, 8, 60
  SCENE OBJECT 255, 0, 0
  SCENE VERTEX -1, -1, 0, 0, 0
  SCENE VERTEX  1, -1, 0, 1, 0
  SCENE VERTEX  1,  1, 0, 1, 1
  SCENE VERTEX -1,  1, 0, 0, 1
  SCENE QUAD 0, 3, 2, 1
  SCENE CAMERA 0, 0, -2, 0, 0
  SCENE RENDER
  PRINT "red="; SCENE.PIXEL(4, 4)
  SCENE SAVE "scene_tex_probe.png"

  ' 2) lo stesso PNG come texture: quad bianco, quindi il colore viene dall'immagine
  SCENE RESET
  SCENE VIEW 8, 8, 60
  SCENE TEXTURE "scene_tex_probe.png"
  SCENE OBJECT 255, 255, 255
  SCENE VERTEX -1, -1, 0, 0, 0
  SCENE VERTEX  1, -1, 0, 1, 0
  SCENE VERTEX  1,  1, 0, 1, 1
  SCENE VERTEX -1,  1, 0, 0, 1
  SCENE QUAD 0, 3, 2, 1
  SCENE CAMERA 0, 0, -2, 0, 0
  SCENE RENDER
  PRINT "loaded="; SCENE.PIXEL(4, 4)
  PRINT "drawn="; SCENE.DRAWN()

  ' 3) load atomico: un percorso sbagliato lascia in piedi la texture precedente
  SCENE TEXTURE "non_esiste_mai.png"
  SCENE RENDER
  PRINT "atomic="; SCENE.PIXEL(4, 4)
END SUB
