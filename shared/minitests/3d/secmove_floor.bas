' EXPECT: m1=-1|z= 28|m2= 0
' MOTORE 3D — WORLD SECMOVE porta il pavimento a quota a velocità costante.
' Settore 1 del demo: pavimento da 0 a 28 a 56 unità/s → mezzo secondo esatto. Appena parte,
' WORLD.MOVING vale -1 (in moto); arrivato, il mover si dichiara fermo (0) da solo, senza un
' comando di stop: HEIGHT serve a fermarlo PRIMA della quota.
IMPORT WORLD
SUB Main()
  WORLD LOAD "worlds/demo.gmbscene"
  WORLD SECMOVE 1, 56, 28
  PRINT "m1="; WORLD.MOVING(1)
  WORLD TICK 0.5
  PRINT "z="; INT(WORLD.FLOORZ(1) * 100 + 0.5) / 100
  PRINT "m2="; WORLD.MOVING(1)
END SUB
