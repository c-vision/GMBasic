' EXPECT: z= 14|m=-1|z2= 28
' MOTORE 3D — WORLD SECMOVE: la quota avanza di speed × tempo, non salta alla destinazione.
' Settore 1: 56 unità/s per 0,25 s fa 14 (quindici passi da 1/60). Il mover è ancora in moto;
' dopo altri 0,25 s arriva a 28 e si ferma.
IMPORT WORLD
SUB Main()
  WORLD LOAD "worlds/demo.gmbscene"
  WORLD SECMOVE 1, 56, 28
  WORLD TICK 0.25
  PRINT "z="; INT(WORLD.FLOORZ(1) * 100 + 0.5) / 100
  PRINT "m="; WORLD.MOVING(1)
  WORLD TICK 0.25
  PRINT "z2="; INT(WORLD.FLOORZ(1) * 100 + 0.5) / 100
END SUB
