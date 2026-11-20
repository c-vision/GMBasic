' EXPECT: f= 10|c= 40|m= 0
' MOTORE 3D — WORLD SECMOVE settore, velocità, pavimento, soffitto.
' Le due quote si muovono con la STESSA velocità (240 unità/s = 4 per passo fisso): in 0,1 s
' (6 passi) il pavimento va da 0 a 10 e il soffitto da 64 a 40. Il mover resta in moto finché
' ENTRAMBE le quote sono arrivate, poi si ferma da solo.
IMPORT WORLD
SUB Main()
  WORLD LOAD "worlds/demo.gmbscene"
  WORLD SECMOVE 1, 240, 10, 40
  WORLD TICK 0.1
  PRINT "f="; INT(WORLD.FLOORZ(1) * 100 + 0.5) / 100
  PRINT "c="; INT(WORLD.CEILZ(1) * 100 + 0.5) / 100
  PRINT "m="; WORLD.MOVING(1)
END SUB
