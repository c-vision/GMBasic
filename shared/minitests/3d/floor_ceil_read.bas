' EXPECT: f= 14|c= 64|m=-1
' MOTORE 3D — WORLD.FLOORZ e WORLD.CEILZ sono LETTURE (funzioni, §1): non fermano il mover.
' È la separazione voluta da HEIGHT: se leggere le quote fermasse il mover, un HUD che mostra
' l'altezza di un ascensore lo bloccherebbe. Dopo le due letture il settore 1 è ancora in moto.
IMPORT WORLD
SUB Main()
  WORLD LOAD "worlds/demo.gmbscene"
  WORLD SECMOVE 1, 56, 28
  WORLD TICK 0.25
  PRINT "f="; INT(WORLD.FLOORZ(1) * 100 + 0.5) / 100
  PRINT "c="; INT(WORLD.CEILZ(1) * 100 + 0.5) / 100
  PRINT "m="; WORLD.MOVING(1)
END SUB
