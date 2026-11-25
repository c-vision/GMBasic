' EXPECT: dentro= 1090453504  fuori= 1073741994  ripetuta= 1090453504
' scene_present — `SCENE PRESENT` (idea #2): copia del TARGET 3D nella pagina 2D corrente.
'
' Il 3D disegna nel proprio target (SCENE VIEW/RENDER); PRESENT lo porta nella pagina 2D
' attiva, che è quella che il pump SDL mostra nella finestra. Qui la pagina è un canvas
' offscreen (CANVAS USE): stesso `active_page()`, ma headless e leggibile pixel per pixel,
' così la copia si verifica senza finestra.
'
' Proprietà verificate:
'   1. dentro il target si legge il colore renderizzato (rosso 0xFF0000);
'   2. fuori dal target la pagina resta quella di prima (CLEAR 1 → 0x400000AA);
'   3. la copia è 1:1 dall'angolo in alto a sinistra: nessuna scalatura (target 8x8 in
'      una pagina 16x16 riempie solo l'angolo).
IMPORT SCENE
IMPORT GRAPHICS

SUB Main()
  DIM v AS LONG, w AS LONG

  ' ── target 3D 8x8 con un quad rosso che riempie l'inquadratura ─────────────
  SCENE VIEW 8, 8, 60
  SCENE OBJECT 255, 0, 0
  SCENE VERTEX -2, -2, 0, 0, 1
  SCENE VERTEX  2, -2, 0, 1, 1
  SCENE VERTEX  2,  2, 0, 1, 0
  SCENE VERTEX -2,  2, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE CAMERA 0, 0, -3, 0, 0
  SCENE RENDER

  ' ── pagina 2D corrente: un canvas 16x16, sfondo palette 1 ─────────────────
  CANVAS NEW 16, 16 INTO w
  CANVAS CLEAR w, 1
  CANVAS USE w
  SCENE PRESENT
  CANVAS GET w, 4, 4 INTO v
  PRINT "dentro="; v;
  CANVAS GET w, 12, 12 INTO v
  PRINT " fuori="; v;

  ' ── una seconda PRESENT non cambia nulla (idempotente sul target) ──────────
  SCENE PRESENT
  CANVAS GET w, 0, 0 INTO v
  PRINT " ripetuta="; v
END SUB
