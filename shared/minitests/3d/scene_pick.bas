' EXPECT: notarget=-1|nt_on=-1|off=-2|near= 1|overlap= 1|far= 2|bg= 0|oob=-1|same=-1
' scene_pick — SCENE.PICK(x, y): buffer di id per pixel di SCENE PICK.
'
' Il pick e' ADDITIVO e spento di default: senza `SCENE PICK on` non esiste il
' buffer e la lettura risponde -2 (stesso numero su tutte e quattro le gambe).
' Le sentinelle sono due e non si confondono: -1 = assente/fuori (nessun target,
' come SCENE.PIXEL, oppure coordinate fuori dal target), -2 = buffer spento su
' un target che esiste. Il fondo e' 0.
' Acceso, il buffer segue il depth test del raster: dove due oggetti si
' sovrappongono vince il PIU' VICINO (occlusione), dove non c'e' geometria
' resta 0 (fondo). Fuori dal target -1, come SCENE.PIXEL.
'
' Geometria: due quad.
'   oggetto 1 (id 1), z = 0  → striscia a sinistra, x da 0 a 2.9
'   oggetto 2 (id 2), z = 2  → striscia piu' larga (piu' lontana), x da -1.5 a 0.5
' La camera e' a z = -5, quindi l'oggetto 1 e' davanti: nella zona di overlap
' (pixel 6-7) il pick deve dare 1, non 2. A destra il fondo: 0.
'
' L'ordine degli id e' quello di dichiarazione degli OBJECT (1..256); il fondo,
' il cielo e i thing valgono 0. I colori (PIXEL) non cambiano col pick acceso:
' il buffer e' un array in piu', non un secondo renderer.
IMPORT SCENE

SUB Main()
  ' ── 0. nessun target (VIEW mai chiamato): -1 come SCENE.PIXEL, non -2 ─────
  ' Il -2 e' riservato al buffer spento SU un target che esiste: senza target
  ' l'oggetto assente e' la stessa sentinella di SCENE.PIXEL.
  PRINT "notarget="; SCENE.PICK(1, 1)
  ' Anche col flag acceso il buffer non esiste finche' non c'e' un target.
  SCENE PICK on
  PRINT "nt_on="; SCENE.PICK(1, 1)
  SCENE PICK off

  ' ── 1. pick spento (default) su un target che esiste: -2 ───────────────────
  SCENE VIEW 16, 16, 60
  PRINT "off="; SCENE.PICK(1, 1)

  ' ── 2. acceso: due oggetti, uno davanti all'altro ──────────────────────────
  SCENE PICK on
  SCENE OBJECT 255, 0, 0
  SCENE VERTEX 0.0, -2, 0, 0, 0
  SCENE VERTEX 2.9, -2, 0, 1, 0
  SCENE VERTEX 2.9,  2, 0, 1, 1
  SCENE VERTEX 0.0,  2, 0, 0, 1
  SCENE QUAD 0, 3, 2, 1

  SCENE OBJECT 0, 255, 0
  SCENE VERTEX -1.5, -2.5, 2, 0, 0
  SCENE VERTEX  0.5, -2.5, 2, 1, 0
  SCENE VERTEX  0.5,  2.5, 2, 1, 1
  SCENE VERTEX -1.5,  2.5, 2, 0, 1
  SCENE QUAD 0, 3, 2, 1

  SCENE CAMERA 0, 0, -5, 0, 0
  SCENE RENDER

  ' Solo oggetto 1 (sinistra).
  PRINT "near="; SCENE.PICK(2, 8)
  ' Zona di OVERLAP: vince l'oggetto davanti (1), non il lontano (2).
  PRINT "overlap="; SCENE.PICK(6, 8)
  ' Solo oggetto 2 (destra).
  PRINT "far="; SCENE.PICK(9, 8)
  ' Nessuna geometria: fondo = 0.
  PRINT "bg="; SCENE.PICK(12, 8)
  ' Fuori dal target: -1, come SCENE.PIXEL.
  PRINT "oob="; SCENE.PICK(16, 8)

  ' ── 3. additivo: i colori non cambiano col pick acceso ────────────────────
  ' Il pixel 2 e' rosso pieno (oggetto 1): stesso valore di una scena senza pick.
  PRINT "same="; (SCENE.PIXEL(2, 8) = 16711680)
END SUB
