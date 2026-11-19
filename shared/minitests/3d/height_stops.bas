' EXPECT: z1= 14|m= 0|z2= 14
' TAG: native-only
' MOTORE 3D — WORLD HEIGHT ferma il mover del settore, alla quota in cui si trova.
' Porte e ascensori si fermano così: SECMOVE parte, HEIGHT lo blocca a metà corsa (14 dopo
' 0,25 s a 56 unità/s) e il pavimento NON riparte nemmeno dopo un secondo di TICK.
SUB Main()
  WORLD LOAD "worlds/demo.gmbscene"
  WORLD SECMOVE 1, 56, 28
  WORLD TICK 0.25
  PRINT "z1="; INT(WORLD.FLOORZ(1) * 100 + 0.5) / 100
  WORLD HEIGHT 1
  PRINT "m="; WORLD.MOVING(1)
  WORLD TICK 1
  PRINT "z2="; INT(WORLD.FLOORZ(1) * 100 + 0.5) / 100
END SUB
