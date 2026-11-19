' EXPECT: t1= 0
' TAG: native-only
' MOTORE 3D — WORLD TICK non retrocede e non avanza con durate non positive.
' TICK 0 e TICK -1 lasciano il tempo a zero: la durata negativa non è "un tick all'indietro",
' è un TICK che non accoda nulla (nessun errore propagato al programma: zero secondi).
SUB Main()
  WORLD LOAD "worlds/demo.gmbscene"
  WORLD TICK 0
  WORLD TICK -1
  PRINT "t1="; INT(WORLD.TIME() * 60 + 0.5)
END SUB
