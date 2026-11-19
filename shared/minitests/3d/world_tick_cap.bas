' EXPECT: t1= 600|t2= 660
' TAG: native-only
' MOTORE 3D — WORLD TICK: un singolo tick non accoda più di 10 s (anti-spirale).
' TICK 100 avanza 10 s (600 tick), non 100: l'eccesso si scarta, così un hitch non innesca
' una spirale di passi da recuperare. Un secondo TICK 1 riparte normalmente da lì.
SUB Main()
  WORLD LOAD "worlds/demo.gmbscene"
  WORLD TICK 100
  PRINT "t1="; INT(WORLD.TIME() * 60 + 0.5)
  WORLD TICK 1
  PRINT "t2="; INT(WORLD.TIME() * 60 + 0.5)
END SUB
