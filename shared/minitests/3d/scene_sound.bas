' EXPECT: g1= 0.5|p1= 0|g2= 0.5|p2=-1|g3= 0.5|p3= 1|g4= 0.25|g5= 0|g6= 0.5|gy= 0.5|py= 1|gu=-1|pu=-2|gr=-1|pr=-2
' scene_sound — audio posizionale (`SCENE SOUND`), specifica M-SND.
'
' Senza device non c'è nulla da sentire: quello che il programma può verificare sono i
' numeri, cioè gli stessi gain/pan che finiscono nei due canali del PCM sintetizzato.
' `SCENE SOUND n, freq, dur, x, y, z, dist, vol` mette una sorgente nello slot n (1..16);
' il LISTENER è la camera, quindi gain e pan si calcolano dall'occhio e dallo yaw.
'
' Camera all'origine, yaw 0 = sguardo verso +z: come su tutto il motore (SCENE.PIXEL)
' il +x è a SINISTRA dell'occhio, quindi pan −1; −x dà pan +1; davanti pan 0.
' gain = (1 − d/dist) × vol/100, 0 oltre `dist`; `dist` 0 = nessuna attenuazione.
' Slot vuoto: gain −1 e pan −2 (letture, non errori).
IMPORT SCENE

SUB Main()
  SCENE RESET
  SCENE VIEW 32, 32, 60
  SCENE CAMERA 0, 0, 0, 0, 0
  ' Geometria minima: RENDER deve riuscire, perché è lui che aggiorna le sorgenti.
  SCENE OBJECT 255, 255, 255
  SCENE VERTEX -10, -10, 0, 0, 1
  SCENE VERTEX  10, -10, 0, 1, 1
  SCENE VERTEX  10,  10, 0, 1, 0
  SCENE VERTEX -10,  10, 0, 0, 0
  SCENE QUAD 0, 3, 2, 1
  SCENE OBJPOS 0, 0, 60, 0

  ' ── A. davanti alla camera, a metà della distanza udibile ────────────────
  SCENE SOUND 1, 440, 0.005, 0, 0, 10, 20, 100
  PRINT "g1="; SCENE.SOUNDGAIN(1)
  PRINT "p1="; SCENE.SOUNDPAN(1)

  ' ── B. +x a sinistra, −x a destra: i due estremi ────────────────────────
  SCENE SOUND 2, 440, 0.005, 10, 0, 0, 20, 100
  PRINT "g2="; SCENE.SOUNDGAIN(2)
  PRINT "p2="; SCENE.SOUNDPAN(2)
  SCENE SOUND 3, 440, 0.005, -10, 0, 0, 20, 100
  PRINT "g3="; SCENE.SOUNDGAIN(3)
  PRINT "p3="; SCENE.SOUNDPAN(3)

  ' ── C. volume e attenuazione ────────────────────────────────────────────
  SCENE SOUND 4, 440, 0.005, 0, 0, 10, 20, 50
  PRINT "g4="; SCENE.SOUNDGAIN(4)
  SCENE SOUND 5, 440, 0.005, 0, 0, 40, 20, 100
  PRINT "g5="; SCENE.SOUNDGAIN(5)
  SCENE SOUND 6, 440, 0.005, 0, 0, 500, 0, 50
  PRINT "g6="; SCENE.SOUNDGAIN(6)

  ' ── D. il listener è la camera: ruotando, la stessa sorgente cambia lato ─
  SCENE CAMERA 0, 0, 0, 90, 0
  SCENE RENDER
  PRINT "gy="; SCENE.SOUNDGAIN(1)
  PRINT "py="; SCENE.SOUNDPAN(1)

  ' ── E. slot vuoto: sentinelle, nessun errore ────────────────────────────
  PRINT "gu="; SCENE.SOUNDGAIN(9)
  PRINT "pu="; SCENE.SOUNDPAN(9)

  ' ── F. RESET svuota gli slot con il resto della scena ───────────────────
  SCENE RESET
  PRINT "gr="; SCENE.SOUNDGAIN(1)
  PRINT "pr="; SCENE.SOUNDPAN(1)
END SUB
