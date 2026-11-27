' EXPECT: kbase= 16777215|chalf= 16777215|cchalf= 988969|kend= 16777215|ckend= 988969|kpose= 16777215|cpose= 988969|klong= 16777215|breset= 16777215
' scene_anim — piste di chiavi degli ossi (`SCENE ANIM`), specifica M-SKIN-KEY.
'
' Stessa scena di scene_skin: un quad ±10 a z=60 in un 32×32 con fov 60, un osso e
' tutti e quattro i vertici legati a lui con peso 1. Le chiavi dicono dove sta
' l'osso a ogni tempo: `RENDER` campiona al tempo di scena (`SCENE TIME`) e scrive
' la posa nell'osso, esattamente come `SCENE KEY` fa con gli oggetti.
'
' 1 unità = 0,462 px e il quad copre i pixel 11..21; una posa +20 lo sposta di
' 9,2 px verso sinistra (la camera guarda +z con up +y, quindi +x va a sinistra),
' cioè sui pixel 5..11. Il fondo è 988969 (0x0f1729).
IMPORT SCENE

SUB Main()
  SCENE RESET
  SCENE VIEW 32, 32, 60
  SCENE CAMERA 0, 0, 0, 0, 0
  SCENE OBJECT 255, 255, 255
  SCENE VERTEX -10, -10, 0, 0, 1
  SCENE VERTEX  10, -10, 0, 1, 1
  SCENE VERTEX  10,  10, 0, 1, 0
  SCENE VERTEX -10,  10, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE OBJPOS 0, 0, 60, 0
  SCENE BONES 1
  FOR v = 0 TO 3
    SCENE WEIGHT 1, v, 1, 1
  NEXT v

  ' ── A. identità a 0 s: il frame è quello dichiarato ─────────────────────
  SCENE ANIM 1, 0.0, 0, 0, 0, 0
  SCENE TIME 0.0
  SCENE RENDER
  PRINT "kbase="; SCENE.PIXEL(16, 16)

  ' ── B. chiave +20 a 1 s: a metà tempo la posa è +10 ─────────────────────
  SCENE ANIM 1, 1.0, 20, 0, 0, 0
  SCENE TIME 0.5
  SCENE RENDER
  PRINT "chalf="; SCENE.PIXEL(14, 16)
  PRINT "cchalf="; SCENE.PIXEL(20, 16)

  ' ── C. dopo l'ultima chiave vale l'ultima (clamp, nessuna estrapolazione) ─
  SCENE TIME 2.0
  SCENE RENDER
  PRINT "kend="; SCENE.PIXEL(8, 16)
  PRINT "ckend="; SCENE.PIXEL(16, 16)

  ' ── D. una posa data a mano è lo stesso stato: la pista la sovrascrive ───
  SCENE POSEBONE 1, 0, 0, 0, 0
  SCENE RENDER
  PRINT "kpose="; SCENE.PIXEL(8, 16)
  PRINT "cpose="; SCENE.PIXEL(16, 16)

  ' ── E. la forma con pitch/roll, su una chiave allo stesso tempo ──────────
  SCENE ANIM 1, 2.0, 0, 0, 0, 0, 0, 0
  SCENE RENDER
  PRINT "klong="; SCENE.PIXEL(16, 16)

  ' ── F. ridichiarare lo scheletro svuota le piste ─────────────────────────
  SCENE BONES 1
  FOR v = 0 TO 3
    SCENE WEIGHT 1, v, 1, 1
  NEXT v
  SCENE TIME 2.0
  SCENE RENDER
  PRINT "breset="; SCENE.PIXEL(16, 16)
END SUB
